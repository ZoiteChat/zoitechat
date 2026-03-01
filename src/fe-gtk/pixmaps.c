/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "fe-gtk.h"
#include "../common/cfgfiles.h"
#include "../common/zoitechat.h"
#include "../common/fe.h"
#include "resources.h"
#include "icon-resolver.h"

#include <gio/gio.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <cairo.h>

GdkPixbuf *pix_ulist_voice;
GdkPixbuf *pix_ulist_halfop;
GdkPixbuf *pix_ulist_op;
GdkPixbuf *pix_ulist_owner;
GdkPixbuf *pix_ulist_founder;
GdkPixbuf *pix_ulist_netop;

GdkPixbuf *pix_tray_normal;
GdkPixbuf *pix_tray_fileoffer;
GdkPixbuf *pix_tray_highlight;
GdkPixbuf *pix_tray_message;

GdkPixbuf *pix_tree_channel;
GdkPixbuf *pix_tree_dialog;
GdkPixbuf *pix_tree_server;
GdkPixbuf *pix_tree_util;

GdkPixbuf *pix_book;
GdkPixbuf *pix_zoitechat;

static cairo_surface_t *
pixbuf_to_cairo_surface (GdkPixbuf *pixbuf)
{
	cairo_surface_t *surface;
	gboolean has_alpha;
	int width;
	int height;
	int src_stride;
	int dest_stride;
	int n_channels;
	const guchar *src_pixels;
	unsigned char *dest_pixels;
	int x;
	int y;

	g_return_val_if_fail (GDK_IS_PIXBUF (pixbuf), NULL);

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);

	surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
	if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
	{
		cairo_surface_destroy (surface);
		return NULL;
	}

	src_stride = gdk_pixbuf_get_rowstride (pixbuf);
	src_pixels = gdk_pixbuf_get_pixels (pixbuf);
	dest_stride = cairo_image_surface_get_stride (surface);
	dest_pixels = cairo_image_surface_get_data (surface);

	for (y = 0; y < height; y++)
	{
		const guchar *src_row = src_pixels + (y * src_stride);
		guint32 *dest_row = (guint32 *)(dest_pixels + (y * dest_stride));

		for (x = 0; x < width; x++)
		{
			const guchar *src = src_row + (x * n_channels);
			guchar alpha = has_alpha ? src[3] : 0xff;
			guchar red = src[0];
			guchar green = src[1];
			guchar blue = src[2];
			guchar premul_red = (guchar)((red * alpha + 127) / 255);
			guchar premul_green = (guchar)((green * alpha + 127) / 255);
			guchar premul_blue = (guchar)((blue * alpha + 127) / 255);

			dest_row[x] = ((guint32)alpha << 24) |
				((guint32)premul_red << 16) |
				((guint32)premul_green << 8) |
				((guint32)premul_blue);
		}
	}

	cairo_surface_mark_dirty (surface);

	return surface;
}

static cairo_surface_t *
pixmap_load_from_file_real (char *file)
{
	GdkPixbuf *img;
	cairo_surface_t *surface;

	img = gdk_pixbuf_new_from_file (file, 0);
	if (!img)
		return NULL;

	surface = pixbuf_to_cairo_surface (img);
	g_object_unref (img);

	return surface;
}

cairo_surface_t *
pixmap_load_from_file (char *filename)
{
	char buf[256];
	cairo_surface_t *pix;

	if (filename[0] == '\0')
		return NULL;

	pix = pixmap_load_from_file_real (filename);
	if (pix == NULL)
	{
		strcpy (buf, "Cannot open:\n\n");
		strncpy (buf + 14, filename, sizeof (buf) - 14);
		buf[sizeof (buf) - 1] = 0;
		fe_message (buf, FE_MSG_ERROR);
	}

	return pix;
}

/* load custom icons from <config>/icons, don't mess in system folders */
static GdkPixbuf *
load_pixmap (IconResolverRole role, int item)
{
	GdkPixbuf *pixbuf = NULL;
	GdkPixbuf *scaledpixbuf;
	const char *scale;
	int iscale;
	char *path;
	const char *system_icon_name = NULL;

	path = icon_resolver_resolve_path (role, item, GTK_ICON_SIZE_MENU, "pixmap",
	                                   ICON_RESOLVER_THEME_SYSTEM, &system_icon_name);
	if (path)
	{
		if (g_str_has_prefix (path, "/icons/"))
			pixbuf = gdk_pixbuf_new_from_resource (path, NULL);
		else
			pixbuf = gdk_pixbuf_new_from_file (path, 0);
		g_free (path);
	}

	if (!pixbuf && system_icon_name)
	{
		GtkIconTheme *theme = gtk_icon_theme_get_default ();
		if (theme)
			pixbuf = gtk_icon_theme_load_icon (theme, system_icon_name, 16, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
	}


	scale = g_getenv ("GDK_SCALE");
	if (scale && pixbuf)
	{
		iscale = atoi (scale);
		if (iscale > 0)
		{
			scaledpixbuf = gdk_pixbuf_scale_simple (pixbuf, gdk_pixbuf_get_width (pixbuf) * iscale,
				gdk_pixbuf_get_height (pixbuf) * iscale, GDK_INTERP_BILINEAR);

			if (scaledpixbuf)
			{
				g_object_unref (pixbuf);
				pixbuf = scaledpixbuf;
			}
		}
	}

	g_warn_if_fail (pixbuf != NULL);

	return pixbuf;
}

void
pixmaps_init (void)
{
	zoitechat_register_resource();

	pix_ulist_voice = load_pixmap (ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_VOICE);
	pix_ulist_halfop = load_pixmap (ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_HALFOP);
	pix_ulist_op = load_pixmap (ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_OP);
	pix_ulist_owner = load_pixmap (ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_OWNER);
	pix_ulist_founder = load_pixmap (ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_FOUNDER);
	pix_ulist_netop = load_pixmap (ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_NETOP);

	pix_tray_normal = load_pixmap (ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_NORMAL);
	pix_tray_fileoffer = load_pixmap (ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_FILEOFFER);
	pix_tray_highlight = load_pixmap (ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_HIGHLIGHT);
	pix_tray_message = load_pixmap (ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_MESSAGE);

	pix_tree_channel = load_pixmap (ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_CHANNEL);
	pix_tree_dialog = load_pixmap (ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_DIALOG);
	pix_tree_server = load_pixmap (ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_SERVER);
	pix_tree_util = load_pixmap (ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_UTIL);

	/* non-replaceable book pixmap */
	pix_book = gdk_pixbuf_new_from_resource ("/icons/book.png", NULL);

	/* used in About window, tray icon and WindowManager icon. */
	pix_zoitechat = gdk_pixbuf_new_from_resource ("/icons/zoitechat.png", NULL);
}
