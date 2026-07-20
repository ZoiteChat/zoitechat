/* ZoiteChat
 * Copyright (C) 2026 ZoiteChat contributors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

/* Inline image previews: click an image link in the text area and the
 * picture is downloaded and shown below the message; click the link again
 * to hide it.
 *
 * The download is a deliberately small HTTP/1.0 client on top of
 * GSocketClient so no new library dependency is needed.  Everything runs
 * asynchronously on the main loop; the target textentry is referenced only
 * through the handle returned by gtk_xtext_image_load_begin(), which stays
 * valid even if the entry is scrolled out of the buffer meanwhile. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <gio/gio.h>
#include <gdk/gdkkeysyms.h>

#include "fe-gtk.h"

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/text.h"
#include "../common/cfgfiles.h"
#include "maingui.h"
#include "inline-image.h"

#define IMAGE_FETCH_MAX_REDIRECTS 5
#define IMAGE_FETCH_MAX_HEADER_BYTES (32 * 1024)
#define IMAGE_FETCH_MAX_BODY_BYTES (16 * 1024 * 1024)
#define IMAGE_FETCH_TIMEOUT_SECONDS 30

typedef struct
{
	guint handle;				/* from gtk_xtext_image_load_begin() */
	session *sess;				/* where to report errors; may die meanwhile */
	char *url;					/* URL currently being fetched */
	int redirects;
	int page_hops;				/* html pages resolved via og:image so far */
	char *content_type;		/* lowercased media type of the response */

	gboolean tls;
	char *host;					/* without brackets, for connecting */
	char *host_header;		/* host[:port] as it should appear in Host: */
	guint16 port;
	char *resource;			/* path + query */

	GSocketClient *client;
	GSocketConnection *conn;
	char *request;
	gsize request_sent;

	GByteArray *data;			/* raw response, headers + body */
	gsize body_start;			/* 0 while the headers are incomplete */
	gssize content_length;	/* -1 if not announced */

	GSList *approved_hosts;	/* hosts the user has okayed for this fetch */

	guchar readbuf[16384];
} image_fetch;

static gboolean image_fetch_begin_request (image_fetch *fetch, const char *url);
static void image_fetch_read_more (image_fetch *fetch);

/* file extensions worth trying to decode; formats without an installed
   gdk-pixbuf loader fail gracefully with an error line */
static const char *const image_exts[] =
{
	".png", ".apng", ".jpg", ".jpeg", ".jpe", ".jfif", ".gif", ".webp",
	".bmp", ".svg", ".avif", ".heif", ".heic", ".ico", ".tif", ".tiff",
	".jxl"
};

/* image hosting sites whose page links can be resolved to the picture
   itself (via the page's og:image meta tag); subdomains match too */
static const struct
{
	const char *host;
	const char *path_re;
} image_hosts[] =
{
	{ "imgur.com",     "^/(gallery/|a/|t/[^/]+/)?"
	                   "(?!(?:upload|signin|signup|register|emerald|rules|"
	                   "privacy|about|apps|blog|jobs|search|random|memegen|"
	                   "vidgif)$)[a-zA-Z0-9]{5,10}$" },
	{ "gyazo.com",     "^/[0-9a-fA-F]{32}$" },
	{ "ibb.co",        "^/[a-zA-Z0-9]{6,12}$" },
	{ "postimg.cc",    "^/[a-zA-Z0-9]{6,12}$" },
	{ "giphy.com",     "^/gifs/[a-zA-Z0-9_-]+$" },
	{ "tenor.com",     "^/view/[a-zA-Z0-9_-]+$" },
	{ "flickr.com",    "^/photos/[^/]+/[0-9]+" },
	{ "flic.kr",       "^/p/[a-zA-Z0-9]+$" },
	{ "unsplash.com",  "^/photos/[a-zA-Z0-9_-]+$" },
	/* twitter's image CDN serves the picture directly, the format is in
	   the query string instead of the path */
	{ "pbs.twimg.com", "^/media/[a-zA-Z0-9_-]+" },
};

static gboolean
image_host_matches (const char *host, gsize host_len,
						  const char *path, gsize path_len)
{
	static GRegex *path_res[G_N_ELEMENTS (image_hosts)];
	char *path0;
	gboolean hit;
	gsize i, cand_len;

	for (i = 0; i < G_N_ELEMENTS (image_hosts); i++)
	{
		const char *cand = image_hosts[i].host;

		cand_len = strlen (cand);
		if (host_len == cand_len)
		{
			if (g_ascii_strncasecmp (host, cand, cand_len) != 0)
				continue;
		}
		else if (host_len > cand_len &&
					host[host_len - cand_len - 1] == '.')
		{
			if (g_ascii_strncasecmp (host + host_len - cand_len, cand, cand_len) != 0)
				continue;
		}
		else
			continue;

		if (path_res[i] == NULL)
			path_res[i] = g_regex_new (image_hosts[i].path_re, 0, 0, NULL);
		if (path_res[i] == NULL)
			continue;

		path0 = g_strndup (path, path_len);
		hit = g_regex_match (path_res[i], path0, 0, NULL);
		g_free (path0);
		if (hit)
			return TRUE;
	}

	return FALSE;
}

gboolean
inline_image_is_image_url (const char *url)
{
	const char *host, *host_end, *host_match_end, *path, *end, *p;
	gsize i, len, ext_len;

	if (g_ascii_strncasecmp (url, "http://", 7) == 0)
		host = url + 7;
	else if (g_ascii_strncasecmp (url, "https://", 8) == 0)
		host = url + 8;
	else
		return FALSE;

	host_end = host + strcspn (host, "/?#");

	/* skip any userinfo@ */
	for (p = host; p < host_end; p++)
		if (*p == '@')
			host = p + 1;

	path = host_end;
	if (*path != '/')
		return FALSE;

	/* the extension must be at the end of the path, before query/fragment */
	end = strpbrk (path, "?#");
	if (end == NULL)
		end = path + strlen (path);
	len = end - path;

	for (i = 0; i < G_N_ELEMENTS (image_exts); i++)
	{
		ext_len = strlen (image_exts[i]);
		if (len > ext_len &&
			 g_ascii_strncasecmp (end - ext_len, image_exts[i], ext_len) == 0)
			return TRUE;
	}

	/* ignore a :port when matching known hosts */
	host_match_end = memchr (host, ':', host_end - host);
	if (host_match_end == NULL || memchr (host, ']', host_end - host))
		host_match_end = host_end;

	return image_host_matches (host, host_match_end - host, path, len);
}

/* the hostname a http(s) URL points at, without userinfo or port; IPv6
   literals keep their brackets.  Unicode hostnames are returned in their
   punycode (xn--) form: this is the name that is actually resolved, and
   showing it keeps lookalike characters from disguising the real
   destination in the placeholder label, the confirmation dialog and the
   stored domain permissions.  Returns NULL if it cannot be parsed. */

static char *
image_url_host (const char *url)
{
	const char *auth, *auth_end, *host_start, *host_end, *p, *at;
	char *host;

	if (g_ascii_strncasecmp (url, "http://", 7) == 0)
		auth = url + 7;
	else if (g_ascii_strncasecmp (url, "https://", 8) == 0)
		auth = url + 8;
	else
		return NULL;

	auth_end = auth + strcspn (auth, "/?#");

	/* skip any userinfo@ */
	at = NULL;
	for (p = auth; p < auth_end; p++)
		if (*p == '@')
			at = p;
	host_start = at ? at + 1 : auth;
	if (host_start >= auth_end)
		return NULL;

	if (*host_start == '[')	/* IPv6 literal */
	{
		host_end = memchr (host_start, ']', auth_end - host_start);
		if (host_end == NULL || host_end == host_start + 1)
			return NULL;
		return g_strndup (host_start, host_end + 1 - host_start);
	}

	host_end = memchr (host_start, ':', auth_end - host_start);
	if (host_end == NULL)
		host_end = auth_end;
	if (host_end == host_start)
		return NULL;

	host = g_strndup (host_start, host_end - host_start);

	if (g_hostname_is_non_ascii (host))
	{
		char *ascii = g_hostname_to_ascii (host);

		/* if the IDN conversion fails, keep the raw name: the user still
		   sees and confirms exactly what will be looked up */
		if (ascii)
		{
			g_free (host);
			host = ascii;
		}
	}

	return host;
}

static void
image_fetch_conn_closed (GObject *source, GAsyncResult *res, gpointer user_data)
{
	g_io_stream_close_finish (G_IO_STREAM (source), res, NULL);
	g_object_unref (user_data);	/* the connection */
}

static void
image_fetch_drop_connection (image_fetch *fetch)
{
	if (fetch->conn)
	{
		/* close asynchronously; TLS shutdown may want to write */
		g_io_stream_close_async (G_IO_STREAM (fetch->conn), G_PRIORITY_DEFAULT,
										 NULL, image_fetch_conn_closed, fetch->conn);
		fetch->conn = NULL;
	}

	if (fetch->client)
	{
		g_object_unref (fetch->client);
		fetch->client = NULL;
	}
}

static void
image_fetch_free (image_fetch *fetch)
{
	image_fetch_drop_connection (fetch);
	g_free (fetch->url);
	g_free (fetch->content_type);
	g_free (fetch->host);
	g_free (fetch->host_header);
	g_free (fetch->resource);
	g_free (fetch->request);
	if (fetch->data)
		g_byte_array_free (fetch->data, TRUE);
	g_slist_free_full (fetch->approved_hosts, g_free);
	g_free (fetch);
}

static void
image_fetch_fail (image_fetch *fetch, const char *reason)
{
	gtk_xtext_image_load_finish (fetch->handle, NULL);

	/* the <> keep the URL from being turned into a placeholder again */
	if (is_session (fetch->sess))
		PrintTextf (fetch->sess, _("Could not load image <%s> (%s)\n"),
						fetch->url, reason);

	image_fetch_free (fetch);
}

/* the user declined to contact a host: drop the fetch without printing
   an error line */

static void
image_fetch_abandon (image_fetch *fetch)
{
	gtk_xtext_image_load_finish (fetch->handle, NULL);
	image_fetch_free (fetch);
}

/* ------------------------------------------------------------------ */
/* Domain permissions: fetching remote media reveals the user's IP address
 * (and that the message was viewed) to the contacted server, so the first
 * load from each domain needs explicit confirmation.  Permanently allowed
 * domains are kept in remotemedia.conf, one hostname per line.
 * Permissions are tied to domains, never to the IRC user who posted the
 * link: nicknames and accounts can be compromised. */

#define IMAGE_DOMAINS_FILE "remotemedia.conf"

static GSList *allowed_domains = NULL;
static gboolean allowed_domains_loaded = FALSE;

static gboolean
image_domain_always_allowed (const char *host)
{
	GSList *list;

	if (!allowed_domains_loaded)
	{
		char line[512];
		FILE *fp;

		allowed_domains_loaded = TRUE;
		fp = zoitechat_fopen_file (IMAGE_DOMAINS_FILE, "r", 0);
		if (fp)
		{
			while (fgets (line, sizeof line, fp))
			{
				g_strstrip (line);
				if (line[0])
					allowed_domains = g_slist_append (allowed_domains,
																 g_strdup (line));
			}
			fclose (fp);
		}
	}

	for (list = allowed_domains; list; list = list->next)
	{
		if (g_ascii_strcasecmp (list->data, host) == 0)
			return TRUE;
	}

	return FALSE;
}

static void
image_domain_always_allow (const char *host)
{
	GSList *list;
	FILE *fp;

	if (image_domain_always_allowed (host))
		return;

	allowed_domains = g_slist_append (allowed_domains, g_strdup (host));

	fp = zoitechat_fopen_file (IMAGE_DOMAINS_FILE, "w", 0);
	if (fp == NULL)
		return;
	for (list = allowed_domains; list; list = list->next)
		fprintf (fp, "%s\n", (char *) list->data);
	fclose (fp);
}

/* ask the user before the first contact with 'host'.  Returns TRUE to go
   ahead; *always is set if the whole domain was allowed permanently. */

static gboolean
image_domain_confirm (const char *host, const char *url, gboolean *always)
{
	GtkWidget *dialog;
	int response;

	*always = FALSE;

	dialog = gtk_message_dialog_new (
		parent_window ? GTK_WINDOW (parent_window) : NULL,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
		_("Loading remote media will contact %s. This may reveal your IP "
		  "address and that you viewed this message."), host);
	/* the complete URL, visible before any request is made */
	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
															"%s", url);
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
									_("_Cancel"), GTK_RESPONSE_CANCEL,
									_("Always allow this _domain"), 1,
									_("Load _once"), 2,
									NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Load remote media?"));

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	*always = response == 1;
	return response == 1 || response == 2;
}

/* make sure the user has agreed to contacting the host 'url' points at.
   Checked before every request - the initial one, redirects and image
   host page hops - so a fetch can never be bounced to a domain the user
   has not confirmed.  Returns FALSE if the user declined. */

static gboolean
image_fetch_host_permitted (image_fetch *fetch, const char *url)
{
	char *host = image_url_host (url);
	GSList *list;
	gboolean always;

	if (host == NULL)	/* not parsable; the request itself will fail */
		return TRUE;

	if (image_domain_always_allowed (host))
	{
		g_free (host);
		return TRUE;
	}

	for (list = fetch->approved_hosts; list; list = list->next)
	{
		if (g_ascii_strcasecmp (list->data, host) == 0)
		{
			g_free (host);
			return TRUE;
		}
	}

	if (!image_domain_confirm (host, url, &always))
	{
		g_free (host);
		return FALSE;
	}

	if (always)
		image_domain_always_allow (host);

	/* remember for this fetch either way, so redirects that bounce back
	   to the same host do not ask twice */
	fetch->approved_hosts = g_slist_prepend (fetch->approved_hosts, host);

	return TRUE;
}

/* accept "http://host[:port]/path" or "https://...", filling in the
   connection details.  Returns FALSE if it cannot be parsed. */

static gboolean
image_url_parse (image_fetch *fetch, const char *url)
{
	const char *authority, *auth_end, *host_start, *host_end;
	const char *port_str = NULL, *p, *at;
	guint16 default_port;
	char *raw, *host = NULL;
	gsize len;
	guint64 port_num;

	if (g_ascii_strncasecmp (url, "https://", 8) == 0)
	{
		fetch->tls = TRUE;
		default_port = 443;
		authority = url + 8;
	}
	else if (g_ascii_strncasecmp (url, "http://", 7) == 0)
	{
		fetch->tls = FALSE;
		default_port = 80;
		authority = url + 7;
	}
	else
		return FALSE;

	auth_end = authority + strcspn (authority, "/?#");

	/* skip any userinfo@ */
	at = NULL;
	for (p = authority; p < auth_end; p++)
		if (*p == '@')
			at = p;
	host_start = at ? at + 1 : authority;
	if (host_start >= auth_end)
		return FALSE;

	if (*host_start == '[')	/* IPv6 literal */
	{
		host_end = memchr (host_start, ']', auth_end - host_start);
		if (host_end == NULL || host_end == host_start + 1)
			return FALSE;
		host = g_strndup (host_start + 1, host_end - host_start - 1);
		if (host_end + 1 < auth_end)
		{
			if (host_end[1] != ':')
			{
				g_free (host);
				return FALSE;
			}
			port_str = host_end + 2;
		}
	}
	else
	{
		host_end = memchr (host_start, ':', auth_end - host_start);
		if (host_end)
			port_str = host_end + 1;
		else
			host_end = auth_end;
		if (host_end == host_start)
			return FALSE;
		host = g_strndup (host_start, host_end - host_start);
	}

	fetch->port = default_port;
	if (port_str)
	{
		if (port_str >= auth_end)
		{
			g_free (host);
			return FALSE;
		}
		port_num = 0;
		for (p = port_str; p < auth_end; p++)
		{
			if (!g_ascii_isdigit (*p) || port_num > 65535)
			{
				g_free (host);
				return FALSE;
			}
			port_num = port_num * 10 + (*p - '0');
		}
		if (port_num < 1 || port_num > 65535)
		{
			g_free (host);
			return FALSE;
		}
		fetch->port = (guint16) port_num;
	}

	g_free (fetch->host);
	fetch->host = host;

	g_free (fetch->host_header);
	if (fetch->port == default_port)
		fetch->host_header = strchr (host, ':') ?	/* IPv6 */
			g_strdup_printf ("[%s]", host) : g_strdup (host);
	else
		fetch->host_header = strchr (host, ':') ?
			g_strdup_printf ("[%s]:%u", host, fetch->port) :
			g_strdup_printf ("%s:%u", host, fetch->port);

	/* path + query, with the fragment stripped; escape anything the
	   tokenizer let through that is not valid raw in a request line */
	p = strchr (auth_end, '#');
	len = p ? (gsize) (p - auth_end) : strlen (auth_end);
	if (len == 0)
		raw = g_strdup ("/");
	else if (auth_end[0] == '?')
		raw = g_strdup_printf ("/%.*s", (int) len, auth_end);
	else
		raw = g_strndup (auth_end, len);

	g_free (fetch->resource);
	fetch->resource = g_uri_escape_string (raw, "%:/?[]@!$&'()*+,;=", FALSE);
	g_free (raw);

	return TRUE;
}

/* turn a possibly relative reference (redirect location, og:image URL)
   into an absolute URL against the current request */

static char *
image_fetch_resolve (image_fetch *fetch, const char *location)
{
	const char *scheme = fetch->tls ? "https" : "http";

	if (g_ascii_strncasecmp (location, "http://", 7) == 0 ||
		 g_ascii_strncasecmp (location, "https://", 8) == 0)
		return g_strdup (location);

	if (location[0] == '/' && location[1] == '/')	/* scheme-relative */
		return g_strdup_printf ("%s:%s", scheme, location);

	if (location[0] == '/')
		return g_strdup_printf ("%s://%s%s", scheme, fetch->host_header,
										location);

	/* relative to the directory of the current resource */
	{
		char *slash = strrchr (fetch->resource, '/');
		int dir_len = slash ? (int) (slash - fetch->resource) + 1 : 1;

		return g_strdup_printf ("%s://%s%.*s%s", scheme, fetch->host_header,
										dir_len, fetch->resource, location);
	}
}

/* drop the current connection and fetch 'url' instead.  On a malformed
   URL the fetch is failed (and freed) with 'fail_reason'. */

static void
image_fetch_restart (image_fetch *fetch, const char *url,
							const char *fail_reason)
{
	/* a redirect or page hop may point at a domain the user has not
	   confirmed yet */
	if (!image_fetch_host_permitted (fetch, url))
	{
		image_fetch_abandon (fetch);
		return;
	}

	image_fetch_drop_connection (fetch);
	g_byte_array_set_size (fetch->data, 0);
	fetch->body_start = 0;
	fetch->content_length = -1;
	fetch->request_sent = 0;
	g_free (fetch->content_type);
	fetch->content_type = NULL;

	if (!image_fetch_begin_request (fetch, url))
		image_fetch_fail (fetch, fail_reason);
}

/* decode the handful of character entities that may appear in an html
   attribute holding a URL */

static char *
image_html_unescape (const char *value, gsize len)
{
	GString *out = g_string_sized_new (len);
	gsize i = 0;

	while (i < len)
	{
		if (value[i] == '&')
		{
			if (len - i >= 5 && strncmp (value + i, "&amp;", 5) == 0)
			{
				g_string_append_c (out, '&');
				i += 5;
				continue;
			}
			if (len - i >= 6 && strncmp (value + i, "&quot;", 6) == 0)
			{
				g_string_append_c (out, '"');
				i += 6;
				continue;
			}
			if (len - i >= 5 && strncmp (value + i, "&#39;", 5) == 0)
			{
				g_string_append_c (out, '\'');
				i += 5;
				continue;
			}
		}
		g_string_append_c (out, value[i]);
		i++;
	}

	return g_string_free (out, FALSE);
}

/* find one attribute's value inside a tag; the result points into 'tag' */

static gboolean
image_html_get_attr (const char *tag, gsize tag_len, const char *name,
							const char **value, gsize *value_len)
{
	gsize name_len = strlen (name);
	gsize i = 0, j, k;

	while (i + name_len < tag_len)
	{
		if (g_ascii_strncasecmp (tag + i, name, name_len) != 0 ||
			 (i > 0 && (g_ascii_isalnum (tag[i - 1]) || tag[i - 1] == '-' ||
							tag[i - 1] == ':' || tag[i - 1] == '_')))
		{
			i++;
			continue;
		}

		j = i + name_len;
		while (j < tag_len && g_ascii_isspace (tag[j]))
			j++;
		if (j >= tag_len || tag[j] != '=')
		{
			i++;
			continue;
		}
		j++;
		while (j < tag_len && g_ascii_isspace (tag[j]))
			j++;
		if (j >= tag_len)
			return FALSE;

		if (tag[j] == '"' || tag[j] == '\'')
		{
			const char *endq = memchr (tag + j + 1, tag[j], tag_len - j - 1);

			if (endq == NULL)
				return FALSE;
			*value = tag + j + 1;
			*value_len = endq - (tag + j + 1);
			return TRUE;
		}

		k = j;
		while (k < tag_len && !g_ascii_isspace (tag[k]) && tag[k] != '>')
			k++;
		*value = tag + j;
		*value_len = k - j;
		return TRUE;
	}

	return FALSE;
}

/* find the page's picture from its meta tags.  Returns the (unescaped)
   URL or NULL. */

static char *
image_html_find_image (const char *html, gsize len)
{
	static const char *const props[] =
	{
		"og:image:secure_url", "og:image", "twitter:image",
		"twitter:image:src", "image_src"	/* <link rel=...> */
	};
	char *found[G_N_ELEMENTS (props)] = { NULL };
	const char *pos = html, *html_end = html + len;
	char *result = NULL;
	gsize i;

	while (pos < html_end && found[0] == NULL)
	{
		const char *tag, *tag_end, *prop, *content;
		gsize tag_len, prop_len, content_len;
		gboolean is_link;

		tag = memchr (pos, '<', html_end - pos);
		if (tag == NULL)
			break;
		pos = tag + 1;

		if (html_end - tag >= 6 &&
			 g_ascii_strncasecmp (tag, "<meta", 5) == 0 &&
			 !g_ascii_isalnum (tag[5]))
			is_link = FALSE;
		else if (html_end - tag >= 6 &&
					g_ascii_strncasecmp (tag, "<link", 5) == 0 &&
					!g_ascii_isalnum (tag[5]))
			is_link = TRUE;
		else
			continue;

		tag_end = memchr (tag, '>', html_end - tag);
		if (tag_end == NULL)
			break;
		tag_len = tag_end - tag;

		if (is_link)
		{
			if (!image_html_get_attr (tag, tag_len, "rel", &prop, &prop_len) ||
				 !image_html_get_attr (tag, tag_len, "href", &content, &content_len))
				continue;
		}
		else
		{
			if (!image_html_get_attr (tag, tag_len, "property", &prop, &prop_len) &&
				 !image_html_get_attr (tag, tag_len, "name", &prop, &prop_len))
				continue;
			if (!image_html_get_attr (tag, tag_len, "content", &content, &content_len))
				continue;
		}

		if (content_len == 0)
			continue;

		for (i = 0; i < G_N_ELEMENTS (props); i++)
		{
			if (found[i] == NULL && prop_len == strlen (props[i]) &&
				 g_ascii_strncasecmp (prop, props[i], prop_len) == 0)
			{
				found[i] = image_html_unescape (content, content_len);
				break;
			}
		}
	}

	for (i = 0; i < G_N_ELEMENTS (props); i++)
	{
		if (result == NULL)
			result = found[i];
		else
			g_free (found[i]);
	}

	return result;
}

/* does the response look like an html page rather than image data? */

static gboolean
image_body_is_html (image_fetch *fetch, const guchar *body, gsize len)
{
	gsize i;

	if (fetch->content_type)
	{
		if (strcmp (fetch->content_type, "text/html") == 0 ||
			 strcmp (fetch->content_type, "application/xhtml+xml") == 0)
			return TRUE;
		if (strncmp (fetch->content_type, "image/", 6) == 0)
			return FALSE;
	}

	/* sniff: an html page starts with '<' (after whitespace/BOM) */
	for (i = 0; i < len && i < 256; i++)
	{
		if (g_ascii_isspace (body[i]))
			continue;
		if (body[i] == 0xef && i + 2 < len &&
			 body[i + 1] == 0xbb && body[i + 2] == 0xbf)
		{
			i += 2;
			continue;
		}
		return body[i] == '<';
	}

	return FALSE;
}

static void
image_fetch_complete (image_fetch *fetch)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf = NULL;
	GError *err = NULL;
	const guchar *body;
	gsize body_len;
	gboolean ok;

	if (fetch->body_start == 0 || fetch->body_start > fetch->data->len)
	{
		image_fetch_fail (fetch, _("truncated response"));
		return;
	}

	body = fetch->data->data + fetch->body_start;
	body_len = fetch->data->len - fetch->body_start;
	if (fetch->content_length >= 0 && body_len > (gsize) fetch->content_length)
		body_len = fetch->content_length;

	if (body_len == 0)
	{
		image_fetch_fail (fetch, _("empty response"));
		return;
	}

	/* an image host page instead of the image itself?  find the picture
	   in its meta tags and fetch that instead (one hop only) */
	if (image_body_is_html (fetch, body, body_len))
	{
		char *img, *next_url;

		if (fetch->page_hops >= 1)
		{
			image_fetch_fail (fetch, _("no direct image on the page"));
			return;
		}

		img = image_html_find_image ((const char *) body, body_len);
		if (img == NULL || img[0] == '\0')
		{
			g_free (img);
			image_fetch_fail (fetch, _("no image found on the page"));
			return;
		}

		fetch->page_hops++;
		fetch->redirects = 0;	/* the image may have its own redirects */
		next_url = image_fetch_resolve (fetch, img);
		g_free (img);
		image_fetch_restart (fetch, next_url, _("bad image link on the page"));
		g_free (next_url);
		return;
	}

	loader = gdk_pixbuf_loader_new ();
	ok = gdk_pixbuf_loader_write (loader, body, body_len, &err);
	if (ok)
		ok = gdk_pixbuf_loader_close (loader, &err);
	else
		gdk_pixbuf_loader_close (loader, NULL);
	if (ok)
	{
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf)
			g_object_ref (pixbuf);
	}
	g_object_unref (loader);

	if (pixbuf == NULL)
	{
		image_fetch_fail (fetch, err ? err->message : _("not a supported image"));
		g_clear_error (&err);
		return;
	}
	g_clear_error (&err);

	gtk_xtext_image_load_finish (fetch->handle, pixbuf);
	g_object_unref (pixbuf);
	image_fetch_free (fetch);
}

/* find the first occurrence of the (short) pattern in the memory block */

static gssize
image_fetch_memfind (const guchar *haystack, gsize hay_len,
							const char *needle, gsize needle_len)
{
	gsize i;

	if (hay_len < needle_len)
		return -1;

	for (i = 0; i <= hay_len - needle_len; i++)
	{
		if (haystack[i] == (guchar) needle[0] &&
			 memcmp (haystack + i, needle, needle_len) == 0)
			return i;
	}

	return -1;
}

/* returns the header's value in newly allocated memory, or NULL */

static char *
image_fetch_get_header (const char *headers, gsize len, const char *name)
{
	const char *line = headers;
	const char *end = headers + len;
	gsize name_len = strlen (name);

	while (line < end)
	{
		const char *eol = memchr (line, '\n', end - line);
		gsize line_len = eol ? (gsize) (eol - line) : (gsize) (end - line);

		if (line_len > name_len &&
			 g_ascii_strncasecmp (line, name, name_len) == 0 &&
			 line[name_len] == ':')
		{
			const char *value = line + name_len + 1;
			gsize value_len = line_len - name_len - 1;

			while (value_len > 0 && (*value == ' ' || *value == '\t'))
			{
				value++;
				value_len--;
			}
			while (value_len > 0 && (value[value_len - 1] == '\r' ||
											 value[value_len - 1] == ' ' ||
											 value[value_len - 1] == '\t'))
				value_len--;

			return g_strndup (value, value_len);
		}

		if (eol == NULL)
			break;
		line = eol + 1;
	}

	return NULL;
}

/* the outcome of looking at the data received so far */
typedef enum
{
	IMAGE_FETCH_KEEP_READING,
	IMAGE_FETCH_DONE,			/* fetch was completed, failed or restarted */
} image_fetch_state;

static image_fetch_state
image_fetch_check_headers (image_fetch *fetch)
{
	const char *headers;
	gssize header_end;
	gsize header_len, skip;
	char *location, *length, *msg, *ctype;
	const char *status_str;
	int status;

	if (fetch->body_start != 0)
		return IMAGE_FETCH_KEEP_READING;

	header_end = image_fetch_memfind (fetch->data->data, fetch->data->len,
												 "\r\n\r\n", 4);
	skip = 4;
	if (header_end < 0)
	{
		header_end = image_fetch_memfind (fetch->data->data, fetch->data->len,
													 "\n\n", 2);
		skip = 2;
	}

	if (header_end < 0)
	{
		if (fetch->data->len > IMAGE_FETCH_MAX_HEADER_BYTES)
		{
			image_fetch_fail (fetch, _("response headers too large"));
			return IMAGE_FETCH_DONE;
		}
		return IMAGE_FETCH_KEEP_READING;
	}

	headers = (const char *) fetch->data->data;
	header_len = header_end;

	/* "HTTP/1.x NNN ..." */
	if (header_len < 12 || g_ascii_strncasecmp (headers, "HTTP/", 5) != 0)
	{
		image_fetch_fail (fetch, _("not a HTTP response"));
		return IMAGE_FETCH_DONE;
	}
	status_str = memchr (headers, ' ', header_len);
	status = status_str ? atoi (status_str + 1) : 0;

	if (status == 301 || status == 302 || status == 303 ||
		 status == 307 || status == 308)
	{
		char *next_url;

		location = image_fetch_get_header (headers, header_len, "Location");
		if (location == NULL || location[0] == '\0')
		{
			g_free (location);
			image_fetch_fail (fetch, _("redirect without location"));
			return IMAGE_FETCH_DONE;
		}

		if (++fetch->redirects > IMAGE_FETCH_MAX_REDIRECTS)
		{
			g_free (location);
			image_fetch_fail (fetch, _("too many redirects"));
			return IMAGE_FETCH_DONE;
		}

		next_url = image_fetch_resolve (fetch, location);
		g_free (location);

		image_fetch_restart (fetch, next_url, _("bad redirect location"));
		g_free (next_url);
		return IMAGE_FETCH_DONE;	/* restarted; a new read chain is running */
	}

	if (status < 200 || status > 299)
	{
		msg = g_strdup_printf ("HTTP %d", status);
		image_fetch_fail (fetch, msg);
		g_free (msg);
		return IMAGE_FETCH_DONE;
	}

	fetch->content_length = -1;
	length = image_fetch_get_header (headers, header_len, "Content-Length");
	if (length)
	{
		fetch->content_length = g_ascii_strtoll (length, NULL, 10);
		g_free (length);
		if (fetch->content_length < 0 ||
			 fetch->content_length > IMAGE_FETCH_MAX_BODY_BYTES)
		{
			image_fetch_fail (fetch, _("image too large"));
			return IMAGE_FETCH_DONE;
		}
	}

	g_free (fetch->content_type);
	fetch->content_type = NULL;
	ctype = image_fetch_get_header (headers, header_len, "Content-Type");
	if (ctype)
	{
		char *semi = strchr (ctype, ';');

		if (semi)
			*semi = '\0';
		fetch->content_type = g_ascii_strdown (g_strstrip (ctype), -1);
		g_free (ctype);
	}

	fetch->body_start = header_end + skip;
	return IMAGE_FETCH_KEEP_READING;
}

static void
image_fetch_read_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
	image_fetch *fetch = user_data;
	GError *err = NULL;
	gssize n;

	n = g_input_stream_read_finish (G_INPUT_STREAM (source), res, &err);
	if (n < 0)
	{
		image_fetch_fail (fetch, err->message);
		g_error_free (err);
		return;
	}

	if (n == 0)	/* EOF: HTTP/1.0 end of body */
	{
		image_fetch_complete (fetch);
		return;
	}

	g_byte_array_append (fetch->data, fetch->readbuf, n);

	if (image_fetch_check_headers (fetch) == IMAGE_FETCH_DONE)
		return;

	if (fetch->body_start != 0)
	{
		gsize body_len = fetch->data->len - fetch->body_start;

		if (body_len > IMAGE_FETCH_MAX_BODY_BYTES)
		{
			image_fetch_fail (fetch, _("image too large"));
			return;
		}

		if (fetch->content_length >= 0 &&
			 body_len >= (gsize) fetch->content_length)
		{
			image_fetch_complete (fetch);
			return;
		}
	}

	image_fetch_read_more (fetch);
}

static void
image_fetch_read_more (image_fetch *fetch)
{
	GInputStream *in = g_io_stream_get_input_stream (G_IO_STREAM (fetch->conn));

	g_input_stream_read_async (in, fetch->readbuf, sizeof (fetch->readbuf),
										G_PRIORITY_DEFAULT, NULL,
										image_fetch_read_done, fetch);
}

static void image_fetch_write_more (image_fetch *fetch);

static void
image_fetch_write_done (GObject *source, GAsyncResult *res, gpointer user_data)
{
	image_fetch *fetch = user_data;
	GError *err = NULL;
	gssize n;

	n = g_output_stream_write_finish (G_OUTPUT_STREAM (source), res, &err);
	if (n <= 0)
	{
		image_fetch_fail (fetch, err ? err->message : _("connection lost"));
		g_clear_error (&err);
		return;
	}

	fetch->request_sent += n;
	if (fetch->request_sent < strlen (fetch->request))
		image_fetch_write_more (fetch);
	else
		image_fetch_read_more (fetch);
}

static void
image_fetch_write_more (image_fetch *fetch)
{
	GOutputStream *out = g_io_stream_get_output_stream (G_IO_STREAM (fetch->conn));

	g_output_stream_write_async (out, fetch->request + fetch->request_sent,
										  strlen (fetch->request) - fetch->request_sent,
										  G_PRIORITY_DEFAULT, NULL,
										  image_fetch_write_done, fetch);
}

static void
image_fetch_connected (GObject *source, GAsyncResult *res, gpointer user_data)
{
	image_fetch *fetch = user_data;
	GError *err = NULL;

	fetch->conn = g_socket_client_connect_to_host_finish (G_SOCKET_CLIENT (source),
																			 res, &err);
	if (fetch->conn == NULL)
	{
		image_fetch_fail (fetch, err->message);
		g_error_free (err);
		return;
	}

	g_free (fetch->request);
	fetch->request = g_strdup_printf (
		"GET %s HTTP/1.0\r\n"
		"Host: %s\r\n"
		/* a stable, versionless value; the exact client version would
		   let the server fingerprint the user across requests */
		"User-Agent: ZoiteChat-Image\r\n"
		"Accept: image/*\r\n"
		"Connection: close\r\n"
		"\r\n",
		fetch->resource, fetch->host_header);
	fetch->request_sent = 0;

	image_fetch_write_more (fetch);
}

/* does ZoiteChat's proxy configuration apply to media downloads, and if
   so, can they actually go through it?  Media is treated like the IRC
   connection itself: the proxy applies unless it is set to DCC-only. */

static gboolean
image_fetch_proxy_wanted (void)
{
	return prefs.hex_net_proxy_host[0] &&
		prefs.hex_net_proxy_type > 0 && prefs.hex_net_proxy_type != 5 &&
		prefs.hex_net_proxy_use != 2;	/* proxy is NOT dcc-only */
}

gboolean
inline_image_proxy_usable (void)
{
	if (!image_fetch_proxy_wanted ())
		return TRUE;

	/* wingate (1) cannot relay an http download */
	return prefs.hex_net_proxy_type >= 2 && prefs.hex_net_proxy_type <= 4;
}

/* configure the client according to ZoiteChat's own proxy settings.  The
   GSocketClient default would be GLib's *system* proxy resolver, which
   may be entirely different from the proxy the user set up to protect
   the IRC session.  Returns FALSE for a proxy type downloads cannot go
   through - the fetch must fail rather than bypass the proxy. */

static gboolean
image_fetch_setup_proxy (GSocketClient *client)
{
	GProxyResolver *resolver;
	const char *scheme;
	char *creds = NULL, *uri;

	if (prefs.hex_net_proxy_type == 5)	/* Auto: the system resolver */
		return TRUE;

	if (!image_fetch_proxy_wanted ())
	{
		/* no proxy configured for this kind of connection: connect
		   directly, exactly like the IRC connection would */
		g_socket_client_set_enable_proxy (client, FALSE);
		return TRUE;
	}

	switch (prefs.hex_net_proxy_type)
	{
		case 2: scheme = "socks4"; break;
		case 3: scheme = "socks5"; break;
		case 4: scheme = "http"; break;
		default: return FALSE;
	}

	/* only http and socks5 authenticate */
	if (prefs.hex_net_proxy_auth && prefs.hex_net_proxy_type != 2 &&
		 prefs.hex_net_proxy_user[0] && prefs.hex_net_proxy_pass[0])
	{
		char *user = g_uri_escape_string (prefs.hex_net_proxy_user, NULL, FALSE);
		char *pass = g_uri_escape_string (prefs.hex_net_proxy_pass, NULL, FALSE);

		creds = g_strdup_printf ("%s:%s@", user, pass);
		g_free (user);
		g_free (pass);
	}

	uri = g_strdup_printf (strchr (prefs.hex_net_proxy_host, ':') ?
										"%s://%s[%s]:%d" : "%s://%s%s:%d",	/* IPv6 */
								  scheme, creds ? creds : "",
								  prefs.hex_net_proxy_host,
								  prefs.hex_net_proxy_port);

	resolver = g_simple_proxy_resolver_new (uri, NULL);
	g_socket_client_set_proxy_resolver (client, resolver);
	g_object_unref (resolver);
	g_free (uri);
	g_free (creds);

	return TRUE;
}

/* (re)start the download for 'url'.  Returns FALSE if the URL cannot be
   parsed or the configured proxy cannot carry the download, in which
   case nothing was started. */

static gboolean
image_fetch_begin_request (image_fetch *fetch, const char *url)
{
	if (!image_url_parse (fetch, url))
		return FALSE;

	g_free (fetch->url);
	fetch->url = g_strdup (url);

	fetch->client = g_socket_client_new ();
	if (!image_fetch_setup_proxy (fetch->client))
	{
		g_object_unref (fetch->client);
		fetch->client = NULL;
		return FALSE;
	}
	g_socket_client_set_timeout (fetch->client, IMAGE_FETCH_TIMEOUT_SECONDS);
	if (fetch->tls)
		g_socket_client_set_tls (fetch->client, TRUE);

	g_socket_client_connect_to_host_async (fetch->client, fetch->host,
														fetch->port, NULL,
														image_fetch_connected, fetch);

	return TRUE;
}

void
inline_image_toggle (session *sess, GtkXText *xtext, textentry *ent,
							const char *url)
{
	image_fetch *fetch;
	const char *shown_url;
	guint handle;

	/* second click on the same link hides the image again; a click on a
	   different link of the same line replaces it */
	shown_url = gtk_xtext_entry_get_image_url (xtext, ent);
	if (shown_url)
	{
		gboolean same = strcmp (shown_url, url) == 0;

		gtk_xtext_image_remove (xtext, ent);
		if (same)
			return;
	}

	handle = gtk_xtext_image_load_begin (xtext, ent, url);
	if (handle == 0)	/* a download for this line is already underway */
		return;

	fetch = g_new0 (image_fetch, 1);
	fetch->handle = handle;
	fetch->sess = sess;
	fetch->content_length = -1;
	fetch->data = g_byte_array_new ();

	if (!inline_image_proxy_usable ())
	{
		fetch->url = g_strdup (url);
		image_fetch_fail (fetch,
			_("the configured proxy type cannot carry media downloads"));
		return;
	}

	if (!image_fetch_host_permitted (fetch, url))
	{
		image_fetch_abandon (fetch);
		return;
	}

	if (!image_fetch_begin_request (fetch, url))
	{
		fetch->url = g_strdup (url);
		image_fetch_fail (fetch, _("unsupported URL"));
	}
}

/* ------------------------------------------------------------------ */
/* Placeholder labels: image links are shown as "[Load image from
 * example.com]" with the URL kept in the entry as hidden text, so clicking
 * the label still resolves to the URL.  Naming the destination host in the
 * label keeps the sender from disguising where the click will connect to;
 * the full URL is additionally shown as a tooltip while hovering. */

/* the byte that hides text in the xtext widget; see ATTR_HIDDEN */
#define IMAGE_ATTR_HIDDEN "\010"
/* keeps the URL regex from running into the visible label; hidden too */
#define IMAGE_URL_STOP ")"

static char *
inline_image_placeholder (const char *url)
{
	char *host = image_url_host (url);
	char *text, *joined, *label;
	char **parts;

	/* the label must stay a single word, so spaces inside it become
	   no-break spaces; a hostname can never contain one */
	text = g_strdup_printf (_("Load image from %s"), host ? host : "?");
	parts = g_strsplit (text, " ", -1);
	joined = g_strjoinv ("\xc2\xa0", parts);
	label = g_strdup_printf ("[%s]", joined);

	g_strfreev (parts);
	g_free (joined);
	g_free (text);
	g_free (host);

	return label;
}

/* a text formatting byte as inserted by the message format strings */

static gboolean
inline_image_attr_byte (char c)
{
	switch (c)
	{
		case ATTR_BOLD:
		case ATTR_COLOR:
		case ATTR_BLINK:
		case ATTR_BEEP:
		case ATTR_HIDDEN:
		case ATTR_RESET:
		case ATTR_REVERSE:
		case ATTR_ITALICS:
		case ATTR_STRIKETHROUGH:
		case ATTR_UNDERLINE:
			return TRUE;
	}
	return FALSE;
}

/* length of the run of formatting codes (including any color arguments,
   e.g. "\00304,07") at the start of the word */

static gsize
inline_image_attr_span (const char *word, gsize len)
{
	gsize i = 0, d;

	while (i < len && inline_image_attr_byte (word[i]))
	{
		if (word[i] != ATTR_COLOR)
		{
			i++;
			continue;
		}

		i++;
		for (d = 0; i < len && d < 2 && g_ascii_isdigit (word[i]); d++)
			i++;
		if (i + 1 < len && word[i] == ',' && g_ascii_isdigit (word[i + 1]))
		{
			i++;
			for (d = 0; i < len && d < 2 && g_ascii_isdigit (word[i]); d++)
				i++;
		}
	}

	return i;
}

char *
inline_image_filter_text (const char *text)
{
	GString *out = NULL;
	const char *pos = text;

	if (!prefs.hex_net_remote_media)
		return NULL;

	while (*pos)
	{
		gsize word_len = strcspn (pos, " \t\r\n");

		if (word_len > 0)
		{
			/* formatting codes from the message format cling to the word
			   (e.g. the %O right after your own message text): the URL is
			   what is left between them */
			gsize lead = inline_image_attr_span (pos, word_len);
			gsize core_len = 0;
			char *core;

			while (lead + core_len < word_len &&
					 !inline_image_attr_byte (pos[lead + core_len]))
				core_len++;

			core = g_strndup (pos + lead, core_len);

			if (core_len > 0 && inline_image_is_image_url (core))
			{
				char *label = inline_image_placeholder (core);

				if (out == NULL)
				{
					out = g_string_new (NULL);
					g_string_append_len (out, text, pos - text);
				}
				g_string_append_len (out, pos, lead);
				g_string_append (out, IMAGE_ATTR_HIDDEN);
				g_string_append (out, core);
				g_string_append (out, IMAGE_URL_STOP);
				g_string_append (out, IMAGE_ATTR_HIDDEN);
				g_string_append (out, label);
				g_string_append_len (out, pos + lead + core_len,
										  word_len - lead - core_len);
				g_free (label);
			}
			else if (out)
			{
				g_string_append_len (out, pos, word_len);
			}
			g_free (core);
			pos += word_len;
		}

		if (*pos)	/* the delimiter */
		{
			if (out)
				g_string_append_c (out, *pos);
			pos++;
		}
	}

	return out ? g_string_free (out, FALSE) : NULL;
}

/* ------------------------------------------------------------------ */
/* Image viewer: a click on the rendered inline image opens the larger
 * version in a window that closes on click or Escape. */

static gboolean
image_viewer_key_press (GtkWidget *window, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_KEY_Escape)
	{
		gtk_widget_destroy (window);
		return TRUE;
	}
	return FALSE;
}

static gboolean
image_viewer_button_press (GtkWidget *widget, GdkEventButton *event,
									gpointer window)
{
	gtk_widget_destroy (GTK_WIDGET (window));
	return TRUE;
}

void
inline_image_show_viewer (GtkXText *xtext, textentry *ent)
{
	GdkPixbuf *pixbuf, *scaled = NULL;
	GtkWidget *window, *eventbox, *image, *toplevel;
	const char *url;
	int width, height, max_width = 1280, max_height = 960;
	GdkWindow *gdkwin;

	pixbuf = gtk_xtext_entry_get_full_image (xtext, ent);
	if (pixbuf == NULL)
		return;

	/* fit within the monitor's workarea */
	gdkwin = gtk_widget_get_window (GTK_WIDGET (xtext));
	if (gdkwin)
	{
		GdkDisplay *display = gdk_window_get_display (gdkwin);
		GdkMonitor *monitor = gdk_display_get_monitor_at_window (display, gdkwin);

		if (monitor)
		{
			GdkRectangle area;

			gdk_monitor_get_workarea (monitor, &area);
			max_width = area.width * 9 / 10;
			max_height = area.height * 9 / 10;
		}
	}

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	if (width > max_width || height > max_height)
	{
		double scale = MIN ((double) max_width / width,
								  (double) max_height / height);

		scaled = gdk_pixbuf_scale_simple (pixbuf,
			MAX (1, (int) (width * scale)), MAX (1, (int) (height * scale)),
			GDK_INTERP_BILINEAR);
		if (scaled)
			pixbuf = scaled;
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	url = gtk_xtext_entry_get_image_url (xtext, ent);
	gtk_window_set_title (GTK_WINDOW (window), url ? url : _("Image"));
	gtk_window_set_type_hint (GTK_WINDOW (window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (xtext));
	if (GTK_IS_WINDOW (toplevel))
	{
		gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (toplevel));
		gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER_ON_PARENT);
		gtk_window_set_modal (GTK_WINDOW (window), TRUE);
		gtk_window_set_destroy_with_parent (GTK_WINDOW (window), TRUE);
	}

	eventbox = gtk_event_box_new ();
	image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_container_add (GTK_CONTAINER (eventbox), image);
	gtk_container_add (GTK_CONTAINER (window), eventbox);

	g_signal_connect (G_OBJECT (window), "key-press-event",
							G_CALLBACK (image_viewer_key_press), NULL);
	g_signal_connect (G_OBJECT (eventbox), "button-press-event",
							G_CALLBACK (image_viewer_button_press), window);

	gtk_widget_show_all (window);

	if (scaled)
		g_object_unref (scaled);
}
