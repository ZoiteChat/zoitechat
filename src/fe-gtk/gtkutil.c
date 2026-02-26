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
#define _FILE_OFFSET_BITS 64 /* allow selection of large files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fe-gtk.h"

#include <gdk/gdkkeysyms.h>
#if defined (WIN32) || defined (__APPLE__)
#include <pango/pangocairo.h>
#endif

#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif

#include "../common/zoitechat.h"
#include "../common/fe.h"
#include "../common/util.h"
#include "../common/cfgfiles.h"
#include "../common/zoitechatc.h"
#include "../common/typedef.h"
#include "gtkutil.h"
#include "pixmaps.h"

#ifdef WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* gtkutil.c, just some gtk wrappers */

extern void path_part (char *file, char *path, int pathlen);

struct file_req
{
	GtkWidget *dialog;
	void *userdata;
	filereqcallback callback;
	int flags;		/* FRF_* flags */
};

static const char *
gtkutil_menu_custom_icon_from_stock (const char *stock_name)
{
	static const struct
	{
		const char *stock;
		const char *custom_icon;
	} icon_map[] = {
		{ "gtk-new", "zc-menu-new" },
		{ "gtk-index", "zc-menu-network-list" },
		{ "gtk-revert-to-saved", "zc-menu-load-plugin" },
		{ "gtk-redo", "zc-menu-detach" },
		{ "gtk-close", "zc-menu-close" },
		{ "gtk-quit", "zc-menu-quit" },
		{ "gtk-disconnect", "zc-menu-disconnect" },
		{ "gtk-connect", "zc-menu-connect" },
		{ "gtk-jump-to", "zc-menu-join" },
		{ "gtk-preferences", "zc-menu-preferences" },
		{ "gtk-clear", "zc-menu-clear" },
		{ "gtk-copy", "zc-menu-copy" },
		{ "gtk-delete", "zc-menu-delete" },
		{ "gtk-add", "zc-menu-add" },
		{ "gtk-remove", "zc-menu-remove" },
		{ "gtk-spell-check", "zc-menu-spell-check" },
		{ "gtk-save", "zc-menu-save" },
		{ "gtk-save-as", "zc-menu-save-as" },
		{ "gtk-refresh", "zc-menu-refresh" },
		{ "gtk-justify-left", "zc-menu-search" },
		{ "gtk-find", "zc-menu-find" },
		{ "gtk-go-back", "zc-menu-previous" },
		{ "gtk-go-forward", "zc-menu-next" },
		{ "gtk-help", "zc-menu-help" },
		{ "gtk-about", "zc-menu-about" },
		{ "gtk-convert", "zc-menu-emoji" },
	};
	size_t i;

	if (!stock_name)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (icon_map); i++)
	{
		if (strcmp (stock_name, icon_map[i].stock) == 0)
			return icon_map[i].custom_icon;
	}

	return NULL;
}

static const char *
gtkutil_menu_custom_icon_from_icon_name (const char *icon_name)
{
	static const struct
	{
		const char *icon;
		const char *custom_icon;
	} icon_map[] = {
		{ "document-new", "zc-menu-new" },
		{ "view-list", "zc-menu-network-list" },
		{ "document-open", "zc-menu-load-plugin" },
		{ "edit-redo", "zc-menu-detach" },
		{ "window-close", "zc-menu-close" },
		{ "application-exit", "zc-menu-quit" },
		{ "network-disconnect", "zc-menu-disconnect" },
		{ "network-connect", "zc-menu-connect" },
		{ "go-jump", "zc-menu-join" },
		{ "preferences-system", "zc-menu-preferences" },
		{ "edit-clear", "zc-menu-clear" },
		{ "edit-copy", "zc-menu-copy" },
		{ "edit-delete", "zc-menu-delete" },
		{ "list-add", "zc-menu-add" },
		{ "list-remove", "zc-menu-remove" },
		{ "tools-check-spelling", "zc-menu-spell-check" },
		{ "document-save", "zc-menu-save" },
		{ "document-save-as", "zc-menu-save-as" },
		{ "view-refresh", "zc-menu-refresh" },
		{ "edit-find", "zc-menu-find" },
		{ "go-previous", "zc-menu-previous" },
		{ "go-next", "zc-menu-next" },
		{ "help-browser", "zc-menu-help" },
		{ "help-about", "zc-menu-about" },
		{ "face-smile", "zc-menu-emoji" },
		{ "insert-emoticon", "zc-menu-emoji" },
		{ "software-update-available", "zc-menu-update" },
		{ "network-workgroup", "zc-menu-chanlist" },
	};
	size_t i;

	if (!icon_name)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (icon_map); i++)
	{
		if (strcmp (icon_name, icon_map[i].icon) == 0)
			return icon_map[i].custom_icon;
	}

	return NULL;
}


static GdkPixbuf *
gtkutil_menu_icon_pixbuf_new (const char *icon_name)
{
	GdkPixbuf *pixbuf = NULL;
	char *resource_path;

	if (!icon_name || !g_str_has_prefix (icon_name, "zc-menu-"))
		return NULL;

	resource_path = g_strdup_printf ("/icons/menu/light/%s.png", icon_name + strlen ("zc-menu-"));
	pixbuf = gdk_pixbuf_new_from_resource (resource_path, NULL);
	if (!pixbuf)
	{
		g_free (resource_path);
		resource_path = g_strdup_printf ("/icons/menu/light/%s.svg", icon_name + strlen ("zc-menu-"));
		pixbuf = gdk_pixbuf_new_from_resource (resource_path, NULL);
	}
	g_free (resource_path);

	return pixbuf;
}

const char *
gtkutil_icon_name_from_stock (const char *stock_name)
{
	static const struct
	{
		const char *stock;
		const char *icon;
	} icon_map[] = {
		{ "gtk-new", "document-new" },
		{ "gtk-open", "document-open" },
		{ "gtk-revert-to-saved", "document-open" },
		{ "gtk-save", "document-save" },
		{ "gtk-save-as", "document-save-as" },
		{ "gtk-add", "list-add" },
		{ "gtk-cancel", "dialog-cancel" },
		{ "gtk-ok", "dialog-ok" },
		{ "gtk-no", "dialog-cancel" },
		{ "gtk-yes", "dialog-ok" },
		{ "gtk-apply", "dialog-apply" },
		{ "gtk-dialog-error", "dialog-error" },
		{ "gtk-copy", "edit-copy" },
		{ "gtk-delete", "edit-delete" },
		{ "gtk-remove", "list-remove" },
		{ "gtk-clear", "edit-clear" },
		{ "gtk-redo", "edit-redo" },
		{ "gtk-find", "edit-find" },
		{ "gtk-justify-left", "edit-find" },
		{ "gtk-refresh", "view-refresh" },
		{ "gtk-go-back", "go-previous" },
		{ "gtk-go-forward", "go-next" },
		{ "gtk-index", "view-list" },
		{ "gtk-jump-to", "go-jump" },
		{ "gtk-media-play", "media-playback-start" },
		{ "gtk-preferences", "preferences-system" },
		{ "gtk-help", "help-browser" },
		{ "gtk-about", "help-about" },
		{ "gtk-close", "window-close" },
		{ "gtk-quit", "application-exit" },
		{ "gtk-connect", "network-connect" },
		{ "gtk-disconnect", "network-disconnect" },
		{ "gtk-network", "network-workgroup" },
		{ "gtk-spell-check", "tools-check-spelling" },
	};
	size_t i;

	if (!stock_name)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (icon_map); i++)
	{
		if (strcmp (stock_name, icon_map[i].stock) == 0)
			return icon_map[i].icon;
	}

	return stock_name;
}

static const char *
gtkutil_menu_icon_theme_variant (void)
{
	GtkSettings *settings;
	gboolean prefer_dark = FALSE;
	char *theme_name = NULL;
	char *theme_name_lower = NULL;
	const char *theme_variant = "light";

	settings = gtk_settings_get_default ();
	if (settings)
	{
		g_object_get (G_OBJECT (settings), "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
		g_object_get (G_OBJECT (settings), "gtk-theme-name", &theme_name, NULL);
	}

	if (theme_name)
		theme_name_lower = g_ascii_strdown (theme_name, -1);
	if (prefer_dark || (theme_name_lower && g_strrstr (theme_name_lower, "dark")))
		theme_variant = "dark";

	g_free (theme_name_lower);
	g_free (theme_name);

	return theme_variant;
}

static GtkWidget *
gtkutil_menu_icon_image_new (const char *icon_name, GtkIconSize size)
{
	GtkWidget *image = NULL;
	GdkPixbuf *pixbuf = NULL;
	char *resource_path;
	const char *variant;

	if (!icon_name || !g_str_has_prefix (icon_name, "zc-menu-"))
		return NULL;

	variant = gtkutil_menu_icon_theme_variant ();
	resource_path = g_strdup_printf ("/icons/menu/%s/%s.png", variant, icon_name + strlen ("zc-menu-"));
	if (!g_resources_get_info (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
	{
		g_free (resource_path);
		resource_path = g_strdup_printf ("/icons/menu/light/%s.png", icon_name + strlen ("zc-menu-"));
	}

	pixbuf = gdk_pixbuf_new_from_resource (resource_path, NULL);
	if (!pixbuf)
	{
		g_free (resource_path);
		resource_path = g_strdup_printf ("/icons/menu/%s/%s.svg", variant, icon_name + strlen ("zc-menu-"));
		if (!g_resources_get_info (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL))
		{
			g_free (resource_path);
			resource_path = g_strdup_printf ("/icons/menu/light/%s.svg", icon_name + strlen ("zc-menu-"));
		}
		pixbuf = gdk_pixbuf_new_from_resource (resource_path, NULL);
	}
	if (pixbuf)
	{
		image = gtk_image_new_from_pixbuf (pixbuf);
		g_object_unref (pixbuf);
	}

	g_free (resource_path);

	if (image)
	{
		GtkIconSize tmp_size;
		gint width;
		gint height;

		tmp_size = size;
		if (gtk_icon_size_lookup (tmp_size, &width, &height))
			gtk_image_set_pixel_size (GTK_IMAGE (image), MAX (width, height));
	}

	return image;
}

GtkWidget *
gtkutil_image_new_from_stock (const char *stock, GtkIconSize size)
{
	GtkWidget *image;
	const char *icon_name;

	icon_name = gtkutil_icon_name_from_stock (stock);
	if (!icon_name && stock && g_str_has_prefix (stock, "zc-menu-"))
		icon_name = stock;
	if (size == GTK_ICON_SIZE_MENU)
	{
		const char *menu_icon_name = gtkutil_menu_custom_icon_from_stock (stock);

		if (!menu_icon_name)
			menu_icon_name = gtkutil_menu_custom_icon_from_icon_name (icon_name);

		if (menu_icon_name)
			icon_name = menu_icon_name;
	}

	image = gtkutil_menu_icon_image_new (icon_name, size);
	if (image)
		return image;

	return gtk_image_new_from_icon_name (icon_name, size);
}

GtkWidget *
gtkutil_button_new_from_stock (const char *stock, const char *label)
{
	GtkWidget *button = label ? gtk_button_new_with_mnemonic (label) : gtk_button_new ();

	if (stock)
	{
		GtkWidget *image = gtkutil_image_new_from_stock (stock, GTK_ICON_SIZE_BUTTON);

		if (image)
		{
			gtk_button_set_image (GTK_BUTTON (button), image);
			gtk_button_set_always_show_image (GTK_BUTTON (button), TRUE);
		}
	}

	return button;
}

void
gtkutil_append_font_css (GString *css, const PangoFontDescription *font_desc)
{
	PangoFontMask mask;

	if (!font_desc)
		return;

	mask = pango_font_description_get_set_fields (font_desc);

	if (mask & PANGO_FONT_MASK_FAMILY)
	{
		const char *family = pango_font_description_get_family (font_desc);

		if (family && *family)
			g_string_append_printf (css, " font-family: \"%s\";", family);
	}

	if (mask & PANGO_FONT_MASK_STYLE)
	{
		const char *style = "normal";

		switch (pango_font_description_get_style (font_desc))
		{
		case PANGO_STYLE_ITALIC:
			style = "italic";
			break;
		case PANGO_STYLE_OBLIQUE:
			style = "oblique";
			break;
		default:
			style = "normal";
			break;
		}

		g_string_append_printf (css, " font-style: %s;", style);
	}

	if (mask & PANGO_FONT_MASK_VARIANT)
	{
		const char *variant = "normal";

		if (pango_font_description_get_variant (font_desc) == PANGO_VARIANT_SMALL_CAPS)
			variant = "small-caps";

		g_string_append_printf (css, " font-variant: %s;", variant);
	}

	if (mask & PANGO_FONT_MASK_WEIGHT)
	{
		int weight = (int) pango_font_description_get_weight (font_desc);

		if (weight < 100)
			weight = 100;
		if (weight > 900)
			weight = 900;

		g_string_append_printf (css, " font-weight: %d;", weight);
	}

	if (mask & PANGO_FONT_MASK_STRETCH)
	{
		const char *stretch = "normal";

		switch (pango_font_description_get_stretch (font_desc))
		{
		case PANGO_STRETCH_ULTRA_CONDENSED:
			stretch = "ultra-condensed";
			break;
		case PANGO_STRETCH_EXTRA_CONDENSED:
			stretch = "extra-condensed";
			break;
		case PANGO_STRETCH_CONDENSED:
			stretch = "condensed";
			break;
		case PANGO_STRETCH_SEMI_CONDENSED:
			stretch = "semi-condensed";
			break;
		case PANGO_STRETCH_SEMI_EXPANDED:
			stretch = "semi-expanded";
			break;
		case PANGO_STRETCH_EXPANDED:
			stretch = "expanded";
			break;
		case PANGO_STRETCH_EXTRA_EXPANDED:
			stretch = "extra-expanded";
			break;
		case PANGO_STRETCH_ULTRA_EXPANDED:
			stretch = "ultra-expanded";
			break;
		default:
			stretch = "normal";
			break;
		}

		g_string_append_printf (css, " font-stretch: %s;", stretch);
	}

	if (mask & PANGO_FONT_MASK_SIZE)
	{
		double size = (double) pango_font_description_get_size (font_desc) / PANGO_SCALE;
		char size_buf[G_ASCII_DTOSTR_BUF_SIZE];
		const char *unit = "pt";

		if (pango_font_description_get_size_is_absolute (font_desc))
			unit = "px";

		g_ascii_formatd (size_buf, sizeof (size_buf), "%.2f", size);
		g_string_append_printf (css, " font-size: %s%s;", size_buf, unit);
	}
}

void
gtkutil_apply_palette (GtkWidget *widget, const GdkRGBA *bg, const GdkRGBA *fg,
                       const PangoFontDescription *font_desc)
{
	if (!widget)
		return;

	{
		static const char *class_name = "zoitechat-palette";
		GtkStyleContext *context = gtk_widget_get_style_context (widget);
		GtkCssProvider *provider = g_object_get_data (G_OBJECT (widget),
		                                             "zoitechat-palette-provider");
		gboolean new_provider = FALSE;
		GString *css;
		gchar *bg_color = NULL;
		gchar *fg_color = NULL;

		if (!bg && !fg && !font_desc)
		{
			gtk_style_context_remove_class (context, class_name);
			if (provider)
			{
				gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (provider));
				g_object_set_data (G_OBJECT (widget), "zoitechat-palette-provider", NULL);
			}
			return;
		}

		if (!provider)
		{
			provider = gtk_css_provider_new ();
			g_object_set_data_full (G_OBJECT (widget), "zoitechat-palette-provider",
			                        provider, g_object_unref);
			new_provider = TRUE;
		}

		css = g_string_new (".");
		g_string_append (css, class_name);
		g_string_append (css, " {");
		if (bg)
		{
			bg_color = gdk_rgba_to_string (bg);
			g_string_append_printf (css, " background-color: %s;", bg_color);
		}
		if (fg)
		{
			fg_color = gdk_rgba_to_string (fg);
			g_string_append_printf (css, " color: %s;", fg_color);
		}
		gtkutil_append_font_css (css, font_desc);
		g_string_append (css, " }");

		gtk_css_provider_load_from_data (provider, css->str, -1, NULL);
		if (new_provider)
		{
			gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider),
			                                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		}
		gtk_style_context_add_class (context, class_name);

		g_string_free (css, TRUE);
		g_free (bg_color);
		g_free (fg_color);
	}
}

static void
gtkutil_file_req_destroy (GtkWidget * wid, struct file_req *freq)
{
	freq->callback (freq->userdata, NULL);
	g_free (freq);
}

static void
gtkutil_check_file (char *filename, struct file_req *freq)
{
	int axs = FALSE;

	if (filename == NULL || filename[0] == '\0')
	{
		fe_message (_("No file selected."), FE_MSG_ERROR);
		return;
	}

	GFile *file = g_file_new_for_path (filename);

	if (freq->flags & FRF_WRITE)
	{
		GFile *parent = g_file_get_parent (file);

		GFileInfo *fi = g_file_query_info (parent, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, G_FILE_QUERY_INFO_NONE, NULL, NULL);
		if (fi != NULL)
		{
			if (g_file_info_get_attribute_boolean (fi, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
			{
				axs = TRUE;
			}

			g_object_unref (fi);
		}

		g_object_unref (parent);
	}
	else
	{
		GFileInfo *fi = g_file_query_info (file, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE "," G_FILE_ATTRIBUTE_STANDARD_TYPE, G_FILE_QUERY_INFO_NONE, NULL, NULL);

		if (fi != NULL)
		{
			if (g_file_info_get_file_type (fi) != G_FILE_TYPE_DIRECTORY || (freq->flags & FRF_CHOOSEFOLDER))
			{
				axs = TRUE;
			}

			g_object_unref (fi);
		}
	}

	g_object_unref (file);

	if (axs)
	{
		char *filename_utf8 = g_filename_to_utf8 (filename, -1, NULL, NULL, NULL);
		if (filename_utf8 != NULL)
		{
			freq->callback (freq->userdata, filename_utf8);
			g_free (filename_utf8);
		}
		else
		{
			fe_message ("Filename encoding is corrupt.", FE_MSG_ERROR);
		}
	}
	else
	{
		if (freq->flags & FRF_WRITE)
		{
			fe_message (_("Cannot write to that file."), FE_MSG_ERROR);
		}
		else
		{
			fe_message (_("Cannot read that file."), FE_MSG_ERROR);
		}
	}
}

static void
gtkutil_file_req_done_chooser (GtkFileChooser *fs, struct file_req *freq)
{
	GSList *files, *cur;

	if (freq->flags & FRF_MULTIPLE)
	{
		files = cur = gtk_file_chooser_get_filenames (fs);
		while (cur)
		{
			gtkutil_check_file (cur->data, freq);
			g_free (cur->data);
			cur = cur->next;
		}
		if (files)
			g_slist_free (files);
	}
	else
	{
		if (freq->flags & FRF_CHOOSEFOLDER)
		{
			gchar *filename = gtk_file_chooser_get_current_folder (fs);
			gtkutil_check_file (filename, freq);
			g_free (filename);
		}
		else
		{
			gchar *filename = gtk_file_chooser_get_filename (fs);
			if (filename != NULL)
			{
				gtkutil_check_file (filename, freq);
				g_free (filename);
			}
		}
	}

}

static void
gtkutil_file_req_done (GtkWidget * wid, struct file_req *freq)
{
	gtkutil_file_req_done_chooser (GTK_FILE_CHOOSER (freq->dialog), freq);
	gtk_widget_destroy (freq->dialog);
}

static void
gtkutil_file_req_response (GtkWidget *dialog, gint res, struct file_req *freq)
{
	if (res == GTK_RESPONSE_ACCEPT)
	{
		gtkutil_file_req_done (dialog, freq);
		return;
	}

	gtk_widget_destroy (dialog);
}

#ifdef WIN32
static gboolean
gtkutil_native_dialog_unref_idle (gpointer native)
{
	g_object_unref (native);
	return G_SOURCE_REMOVE;
}

static void
gtkutil_native_file_req_response (GtkNativeDialog *dialog, gint res, struct file_req *freq)
{
	if (res == GTK_RESPONSE_ACCEPT)
		gtkutil_file_req_done_chooser (GTK_FILE_CHOOSER (dialog), freq);

	/* Match gtk dialog flow by always sending NULL to indicate completion. */
	freq->callback (freq->userdata, NULL);
	g_free (freq);

	/*
	 * Defer unref until idle to avoid disposing the native chooser while
	 * still in the button-release signal stack on Windows.
	 */
	g_idle_add (gtkutil_native_dialog_unref_idle, dialog);
}
#endif

void
gtkutil_file_req (GtkWindow *parent, const char *title, void *callback, void *userdata, char *filter, char *extensions,
						int flags)
{
	struct file_req *freq;
	GtkWidget *dialog;
	GtkFileFilter *filefilter;
	char *token;
	char *tokenbuffer;
	const char *xdir;
	GtkWindow *effective_parent = parent;

	extern GtkWidget *parent_window;
	if (effective_parent == NULL && parent_window != NULL)
		effective_parent = GTK_WINDOW (parent_window);

	xdir = get_xdir ();

#ifdef WIN32
	{
		GtkFileChooserNative *native = gtk_file_chooser_native_new (
			title,
			effective_parent,
			(flags & FRF_CHOOSEFOLDER) ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER :
				((flags & FRF_WRITE) ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN),
			(flags & FRF_WRITE) ? _("_Save") : _("_Open"),
			_("_Cancel"));
		GtkFileChooser *native_chooser = GTK_FILE_CHOOSER (native);

		if (flags & FRF_MULTIPLE)
			gtk_file_chooser_set_select_multiple (native_chooser, TRUE);
		if (flags & FRF_WRITE && !(flags & FRF_NOASKOVERWRITE))
			gtk_file_chooser_set_do_overwrite_confirmation (native_chooser, TRUE);

		if ((flags & FRF_EXTENSIONS || flags & FRF_MIMETYPES) && extensions != NULL)
		{
			GtkFileFilter *native_filter = gtk_file_filter_new ();
			char *native_tokenbuffer = g_strdup (extensions);
			char *native_token = strtok (native_tokenbuffer, ";");

			while (native_token != NULL)
			{
				if (flags & FRF_EXTENSIONS)
					gtk_file_filter_add_pattern (native_filter, native_token);
				else
					gtk_file_filter_add_mime_type (native_filter, native_token);
				native_token = strtok (NULL, ";");
			}

			g_free (native_tokenbuffer);
			gtk_file_chooser_set_filter (native_chooser, native_filter);
		}

		if (filter && filter[0] && (flags & FRF_FILTERISINITIAL))
		{
			if (flags & FRF_WRITE)
			{
				char temp[1024];
				path_part (filter, temp, sizeof (temp));
				if (temp[0] && g_file_test (temp, G_FILE_TEST_IS_DIR))
					gtk_file_chooser_set_current_folder (native_chooser, temp);
				else if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
					gtk_file_chooser_set_current_folder (native_chooser, xdir);
				gtk_file_chooser_set_current_name (native_chooser, file_part (filter));
			}
			else
			{
				if (g_file_test (filter, G_FILE_TEST_IS_DIR))
					gtk_file_chooser_set_current_folder (native_chooser, filter);
				else if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
					gtk_file_chooser_set_current_folder (native_chooser, xdir);
			}
		}
		else if (!(flags & FRF_RECENTLYUSED))
		{
			if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
				gtk_file_chooser_set_current_folder (native_chooser, xdir);
		}

		freq = g_new (struct file_req, 1);
		freq->dialog = NULL;
		freq->flags = flags;
		freq->callback = callback;
		freq->userdata = userdata;

		g_signal_connect (native, "response",
						G_CALLBACK (gtkutil_native_file_req_response), freq);
		gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
		return;
	}
#endif

	if (flags & FRF_WRITE)
	{
		dialog = gtk_file_chooser_dialog_new (title, NULL,
												GTK_FILE_CHOOSER_ACTION_SAVE,
												_("_Cancel"), GTK_RESPONSE_CANCEL,
												_("_Save"), GTK_RESPONSE_ACCEPT,
												NULL);

		if (!(flags & FRF_NOASKOVERWRITE))
			gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (dialog), TRUE);
	}
	else
		dialog = gtk_file_chooser_dialog_new (title, NULL,
												GTK_FILE_CHOOSER_ACTION_OPEN,
												_("_Cancel"), GTK_RESPONSE_CANCEL,
												_("_Open"), GTK_RESPONSE_ACCEPT,
												NULL);

	if (filter && filter[0] && (flags & FRF_FILTERISINITIAL))
	{
		if (flags & FRF_WRITE)
		{
			char temp[1024];
			path_part (filter, temp, sizeof (temp));
			if (temp[0] && g_file_test (temp, G_FILE_TEST_IS_DIR))
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), temp);
			else if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), xdir);
			gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), file_part (filter));
		}
		else
		{
			if (g_file_test (filter, G_FILE_TEST_IS_DIR))
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), filter);
			else if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
				gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), xdir);
		}
	}
	else if (!(flags & FRF_RECENTLYUSED))
	{
		if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
			gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), xdir);
	}

	if (flags & FRF_MULTIPLE)
		gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER (dialog), TRUE);
	if (flags & FRF_CHOOSEFOLDER)
		gtk_file_chooser_set_action (GTK_FILE_CHOOSER (dialog), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);

	if ((flags & FRF_EXTENSIONS || flags & FRF_MIMETYPES) && extensions != NULL)
	{
		filefilter = gtk_file_filter_new ();
		tokenbuffer = g_strdup (extensions);
		token = strtok (tokenbuffer, ";");

		while (token != NULL)
		{
			if (flags & FRF_EXTENSIONS)
				gtk_file_filter_add_pattern (filefilter, token);
			else
				gtk_file_filter_add_mime_type (filefilter, token);
			token = strtok (NULL, ";");
		}

		g_free (tokenbuffer);
		gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filefilter);
	}

	if (xdir && xdir[0] && g_file_test (xdir, G_FILE_TEST_IS_DIR))
	{
		GError *shortcut_error = NULL;

		gtk_file_chooser_add_shortcut_folder (GTK_FILE_CHOOSER (dialog), xdir, &shortcut_error);
		if (shortcut_error)
			g_error_free (shortcut_error);
	}
	freq = g_new (struct file_req, 1);
	freq->dialog = dialog;
	freq->flags = flags;
	freq->callback = callback;
	freq->userdata = userdata;

	g_signal_connect (G_OBJECT (dialog), "response",
							G_CALLBACK (gtkutil_file_req_response), freq);
	g_signal_connect (G_OBJECT (dialog), "destroy",
						   G_CALLBACK (gtkutil_file_req_destroy), (gpointer) freq);

	if (effective_parent)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), effective_parent);

	if (flags & FRF_MODAL)
	{
		g_assert (effective_parent);
		gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	}

	gtk_widget_show (dialog);
}

static gboolean
gtkutil_esc_destroy (GtkWidget * win, GdkEventKey * key, gpointer userdata)
{
	GtkWidget *wid;

	/* Destroy the window of detached utils */
	if (!gtk_widget_is_toplevel (win))
	{
		if (gdk_window_get_type_hint (gtk_widget_get_window (win)) == GDK_WINDOW_TYPE_HINT_DIALOG)
			wid = gtk_widget_get_parent (win);
		else
			return FALSE;
	}
	else
		wid = win;

	if (key->keyval == GDK_KEY_Escape)
		gtk_widget_destroy (wid);
			
	return FALSE;
}

void
gtkutil_destroy_on_esc (GtkWidget *win)
{
	g_signal_connect (G_OBJECT (win), "key_press_event", G_CALLBACK (gtkutil_esc_destroy), win);
}

void
gtkutil_destroy (GtkWidget * igad, GtkWidget * dgad)
{
	gtk_widget_destroy (dgad);
}

static void
gtkutil_get_str_response (GtkDialog *dialog, gint arg1, gpointer entry)
{
	void (*callback) (int cancel, char *text, void *user_data);
	char *text;
	void *user_data;

	text = (char *) gtk_entry_get_text (GTK_ENTRY (entry));
	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	user_data = g_object_get_data (G_OBJECT (dialog), "ud");

	switch (arg1)
	{
	case GTK_RESPONSE_REJECT:
		callback (TRUE, text, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_ACCEPT:
		callback (FALSE, text, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	}
}

static void
gtkutil_str_enter (GtkWidget *entry, GtkWidget *dialog)
{
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
}

void
fe_get_str (char *msg, char *def, void *callback, void *userdata)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *hbox;
	GtkWidget *label;
	extern GtkWidget *parent_window;

	dialog = gtk_dialog_new_with_buttons (msg, NULL, 0,
										_("_Cancel"), GTK_RESPONSE_REJECT,
										_("_OK"), GTK_RESPONSE_ACCEPT,
										NULL);
	/* Window classes are required for GTK CSS selectors like
	 * .zoitechat-dark / .zoitechat-light. */
	fe_apply_theme_to_toplevel (dialog);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));
	gtk_box_set_homogeneous (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), TRUE);

	if (userdata == (void *)1)	/* nick box is usually on the very bottom, make it centered */
	{
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
	}
	else
	{
		gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	}

	hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, TRUE, 0);

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	entry = gtk_entry_new ();
	g_signal_connect (G_OBJECT (entry), "activate",
						 	G_CALLBACK (gtkutil_str_enter), dialog);
	gtk_entry_set_text (GTK_ENTRY (entry), def);
	gtk_box_pack_end (GTK_BOX (hbox), entry, 0, 0, 0);

	label = gtk_label_new (msg);
	gtk_box_pack_end (GTK_BOX (hbox), label, 0, 0, 0);

	g_signal_connect (G_OBJECT (dialog), "response",
						   G_CALLBACK (gtkutil_get_str_response), entry);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox);

	gtk_widget_show_all (dialog);
}

static void
gtkutil_get_number_response (GtkDialog *dialog, gint arg1, gpointer spin)
{
	void (*callback) (int cancel, int value, void *user_data);
	int num;
	void *user_data;

	num = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	user_data = g_object_get_data (G_OBJECT (dialog), "ud");

	switch (arg1)
	{
	case GTK_RESPONSE_REJECT:
		callback (TRUE, num, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_ACCEPT:
		callback (FALSE, num, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	}
}

static void
gtkutil_get_bool_response (GtkDialog *dialog, gint arg1, gpointer spin)
{
	void (*callback) (int value, void *user_data);
	void *user_data;

	callback = g_object_get_data (G_OBJECT (dialog), "cb");
	user_data = g_object_get_data (G_OBJECT (dialog), "ud");

	switch (arg1)
	{
	case GTK_RESPONSE_REJECT:
		callback (0, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	case GTK_RESPONSE_ACCEPT:
		callback (1, user_data);
		gtk_widget_destroy (GTK_WIDGET (dialog));
		break;
	}
}

void
fe_get_int (char *msg, int def, void *callback, void *userdata)
{
	GtkWidget *dialog;
	GtkWidget *spin;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkAdjustment *adj;
	extern GtkWidget *parent_window;

	dialog = gtk_dialog_new_with_buttons (msg, NULL, 0,
										_("_Cancel"), GTK_RESPONSE_REJECT,
										_("_OK"), GTK_RESPONSE_ACCEPT,
										NULL);
	/* Window classes are required for GTK CSS selectors like
	 * .zoitechat-dark / .zoitechat-light. */
	fe_apply_theme_to_toplevel (dialog);
	gtk_box_set_homogeneous (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));

	hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, TRUE, 0);

	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	spin = gtk_spin_button_new (NULL, 1, 0);
	adj = gtk_spin_button_get_adjustment ((GtkSpinButton*)spin);
	gtk_adjustment_set_lower (adj, 0);
	gtk_adjustment_set_upper (adj, 1024);
	gtk_adjustment_set_step_increment (adj, 1);
	gtk_adjustment_changed (adj);
	gtk_spin_button_set_value ((GtkSpinButton*)spin, def);
	gtk_box_pack_end (GTK_BOX (hbox), spin, 0, 0, 0);

	label = gtk_label_new (msg);
	gtk_box_pack_end (GTK_BOX (hbox), label, 0, 0, 0);

	g_signal_connect (G_OBJECT (dialog), "response",
						   G_CALLBACK (gtkutil_get_number_response), spin);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), hbox);

	gtk_widget_show_all (dialog);
}

void
fe_get_bool (char *title, char *prompt, void *callback, void *userdata)
{
	GtkWidget *dialog;
	GtkWidget *prompt_label;
	extern GtkWidget *parent_window;

	dialog = gtk_dialog_new_with_buttons (title, NULL, 0,
		_("_No"), GTK_RESPONSE_REJECT,
		_("_Yes"), GTK_RESPONSE_ACCEPT,
		NULL);
	/* Window classes are required for GTK CSS selectors like
	 * .zoitechat-dark / .zoitechat-light. */
	fe_apply_theme_to_toplevel (dialog);
	gtk_box_set_homogeneous (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), TRUE);
	gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_MOUSE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (parent_window));


	g_object_set_data (G_OBJECT (dialog), "cb", callback);
	g_object_set_data (G_OBJECT (dialog), "ud", userdata);

	prompt_label = gtk_label_new (prompt);

	g_signal_connect (G_OBJECT (dialog), "response",
		G_CALLBACK (gtkutil_get_bool_response), NULL);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), prompt_label);

	gtk_widget_show_all (dialog);
}

GtkWidget *
gtkutil_button (GtkWidget *box, char *stock, char *tip, void *callback,
					 void *userdata, char *labeltext)
{
	GtkWidget *wid, *img, *bbox;

	wid = gtk_button_new ();

	if (labeltext)
	{
		gtk_button_set_label (GTK_BUTTON (wid), labeltext);
		img = NULL;
		if (stock)
			img = gtkutil_image_new_from_stock (stock, GTK_ICON_SIZE_BUTTON);
		if (img)
		{
			gtk_button_set_image (GTK_BUTTON (wid), img);
			gtk_button_set_always_show_image (GTK_BUTTON (wid), TRUE);
		}
		gtk_button_set_use_underline (GTK_BUTTON (wid), TRUE);
		if (box)
			gtk_container_add (GTK_CONTAINER (box), wid);
	}
	else
	{
		bbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
		gtk_container_add (GTK_CONTAINER (wid), bbox);
		gtk_widget_show (bbox);

		img = NULL;
		if (stock)
			img = gtkutil_image_new_from_stock (stock, GTK_ICON_SIZE_BUTTON);
		if (img)
		{
			gtk_container_add (GTK_CONTAINER (bbox), img);
			gtk_widget_show (img);
			gtk_button_set_always_show_image (GTK_BUTTON (wid), TRUE);
		}
		gtk_box_pack_start (GTK_BOX (box), wid, 0, 0, 0);
	}

	g_signal_connect (G_OBJECT (wid), "clicked",
							G_CALLBACK (callback), userdata);
	gtk_widget_show (wid);
	if (tip)
		gtk_widget_set_tooltip_text (wid, tip);

	return wid;
}

void
gtkutil_label_new (char *text, GtkWidget * box)
{
	GtkWidget *label = gtk_label_new (text);
	gtk_container_add (GTK_CONTAINER (box), label);
	gtk_widget_show (label);
}

GtkWidget *
gtkutil_entry_new (int max, GtkWidget * box, void *callback,
						 gpointer userdata)
{
	GtkWidget *entry = gtk_entry_new ();
	gtk_entry_set_max_length (GTK_ENTRY (entry), max);
	gtk_container_add (GTK_CONTAINER (box), entry);
	if (callback)
		g_signal_connect (G_OBJECT (entry), "changed",
								G_CALLBACK (callback), userdata);
	gtk_widget_show (entry);
	return entry;
}

void
show_and_unfocus (GtkWidget * wid)
{
	gtk_widget_set_can_focus (wid, FALSE);
	gtk_widget_show (wid);
}

void
gtkutil_set_icon (GtkWidget *win)
{
#ifndef WIN32
	/* FIXME: Magically breaks icon rendering in most
	 * (sub)windows, but OFC only on Windows. GTK <3
	 */
	gtk_window_set_icon (GTK_WINDOW (win), pix_zoitechat);
#endif
}

extern GtkWidget *parent_window;	/* maingui.c */

GtkWidget *
gtkutil_window_new (char *title, char *role, int width, int height, int flags)
{
	GtkWidget *win;

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtkutil_set_icon (win);
#ifdef WIN32
	gtk_window_set_wmclass (GTK_WINDOW (win), "ZoiteChat", "zoitechat");
#endif
	gtk_window_set_title (GTK_WINDOW (win), title);
	gtk_window_set_default_size (GTK_WINDOW (win), width, height);
	gtk_window_set_role (GTK_WINDOW (win), role);
	fe_apply_theme_to_toplevel (win);
	if (flags & 1)
		gtk_window_set_position (GTK_WINDOW (win), GTK_WIN_POS_MOUSE);
	if ((flags & 2) && parent_window)
	{
		gtk_window_set_type_hint (GTK_WINDOW (win), GDK_WINDOW_TYPE_HINT_DIALOG);
		gtk_window_set_transient_for (GTK_WINDOW (win), GTK_WINDOW (parent_window));
		gtk_window_set_destroy_with_parent (GTK_WINDOW (win), TRUE);
	}

	return win;
}

/* pass NULL as selection to paste to both clipboard & X11 text */
void
gtkutil_copy_to_clipboard (GtkWidget *widget, GdkAtom selection,
                           const gchar *str)
{
	GtkWidget *win;
	GtkClipboard *clip, *clip2;

	win = gtk_widget_get_toplevel (GTK_WIDGET (widget));
	if (gtk_widget_is_toplevel (win))
	{
		int len = strlen (str);

		if (selection)
		{
			clip = gtk_widget_get_clipboard (win, selection);
			gtk_clipboard_set_text (clip, str, len);
		} else
		{
			/* copy to both primary X selection and clipboard */
			clip = gtk_widget_get_clipboard (win, GDK_SELECTION_PRIMARY);
			clip2 = gtk_widget_get_clipboard (win, GDK_SELECTION_CLIPBOARD);
			gtk_clipboard_set_text (clip, str, len);
			gtk_clipboard_set_text (clip2, str, len);
		}
	}
}

/* Treeview util functions */

GtkWidget *
gtkutil_treeview_new (GtkWidget *box, GtkTreeModel *model,
                      GtkTreeCellDataFunc mapper, ...)
{
	GtkWidget *win, *view;
	GtkCellRenderer *renderer = NULL;
	GtkTreeViewColumn *col;
	va_list args;
	int col_id = 0;
	GType type;
	char *title, *attr;

	win = gtk_scrolled_window_new (0, 0);
	gtk_container_add (GTK_CONTAINER (box), win);
	if (GTK_IS_BOX (box))
	{
		gtk_box_set_child_packing (GTK_BOX (box), win, TRUE, TRUE, 0,
		                           GTK_PACK_START);
	}
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (win),
									  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand (win, TRUE);
	gtk_widget_set_hexpand (win, TRUE);
	gtk_widget_show (win);

	view = gtk_tree_view_new_with_model (model);
	/* the view now has a ref on the model, we can unref it */
	g_object_unref (G_OBJECT (model));
	gtk_container_add (GTK_CONTAINER (win), view);

	va_start (args, mapper);
	for (col_id = va_arg (args, int); col_id != -1; col_id = va_arg (args, int))
	{
		type = gtk_tree_model_get_column_type (model, col_id);
		switch (type)
		{
			case G_TYPE_BOOLEAN:
				renderer = gtk_cell_renderer_toggle_new ();
				attr = "active";
				break;
			case G_TYPE_STRING:	/* fall through */
			default:
				renderer = gtk_cell_renderer_text_new ();
				attr = "text";
				break;
		}

		title = va_arg (args, char *);
		if (mapper)	/* user-specified function to set renderer attributes */
		{
			col = gtk_tree_view_column_new_with_attributes (title, renderer, NULL);
			gtk_tree_view_column_set_cell_data_func (col, renderer, mapper,
			                                         GINT_TO_POINTER (col_id), NULL);
		} else
		{
			/* just set the typical attribute for this type of renderer */
			col = gtk_tree_view_column_new_with_attributes (title, renderer,
			                                                attr, col_id, NULL);
		}
		gtk_tree_view_append_column (GTK_TREE_VIEW (view), col);
		if (title == NULL)
			gtk_tree_view_column_set_visible (col, FALSE);
	}

	va_end (args);

	return view;
}

gboolean
gtkutil_treemodel_string_to_iter (GtkTreeModel *model, gchar *pathstr, GtkTreeIter *iter_ret)
{
	GtkTreePath *path = gtk_tree_path_new_from_string (pathstr);
	gboolean success;

	success = gtk_tree_model_get_iter (model, iter_ret, path);
	gtk_tree_path_free (path);
	return success;
}

gboolean
gtkutil_treeview_get_selected (GtkTreeView *view, GtkTreeIter *iter_ret, ...)
{
	GtkTreeModel *store;
	GtkTreeSelection *select;
	gboolean has_selected;
	va_list args;
	
	select = gtk_tree_view_get_selection (view);
	has_selected = gtk_tree_selection_get_selected (select, &store, iter_ret);

	if (has_selected) {
		va_start (args, iter_ret);
		gtk_tree_model_get_valist (store, iter_ret, args);
		va_end (args);
	}

	return has_selected;
}

gboolean
gtkutil_tray_icon_supported (GtkWindow *window)
{
#ifdef GDK_WINDOWING_X11
	GdkScreen *screen = gtk_window_get_screen (window);
	GdkDisplay *display = gdk_screen_get_display (screen);
	if (!GDK_IS_X11_DISPLAY (display))
		return FALSE;
	int screen_number = gdk_screen_get_number (screen);
	Display *xdisplay = gdk_x11_display_get_xdisplay (display);
	char *selection_name = g_strdup_printf ("_NET_SYSTEM_TRAY_S%d", screen_number);
	Atom selection_atom = XInternAtom (xdisplay, selection_name, False);
	Window tray_window = None;

	XGrabServer (xdisplay);

	tray_window = XGetSelectionOwner (xdisplay, selection_atom);

	XUngrabServer (xdisplay);
	XFlush (xdisplay);
	g_free (selection_name);

	return (tray_window != None);
#else
	return TRUE;
#endif
}

#if defined (WIN32) || defined (__APPLE__)
gboolean
gtkutil_find_font (const char *fontname)
{
	int i;
	int n_families;
	const char *family_name;
	PangoFontMap *fontmap;
	PangoFontFamily *family;
	PangoFontFamily **families;

	fontmap = pango_cairo_font_map_get_default ();
	pango_font_map_list_families (fontmap, &families, &n_families);

	for (i = 0; i < n_families; i++)
	{
		family = families[i];
		family_name = pango_font_family_get_name (family);

		if (!g_ascii_strcasecmp (family_name, fontname))
		{
			g_free (families);
			return TRUE;
		}
	}

	g_free (families);
	return FALSE;
}
#endif

GtkWidget *
gtkutil_box_new (GtkOrientation orientation, gboolean homogeneous, gint spacing)
{
	GtkWidget *box = gtk_box_new (orientation, spacing);

	gtk_box_set_homogeneous (GTK_BOX (box), homogeneous);
	return box;
}

GtkWidget *
gtkutil_grid_new (guint rows, guint columns, gboolean homogeneous)
{
	GtkWidget *grid = gtk_grid_new ();

	gtk_grid_set_row_homogeneous (GTK_GRID (grid), homogeneous);
	gtk_grid_set_column_homogeneous (GTK_GRID (grid), homogeneous);
	return grid;
}

static GtkAlign
gtkutil_align_from_options (GtkutilAttachOptions options, GtkAlign default_align)
{
	if (options & GTKUTIL_ATTACH_FILL)
		return GTK_ALIGN_FILL;

	return default_align;
}

static gboolean
gtkutil_expansion_from_options (GtkutilAttachOptions options, gboolean default_expand)
{
	if (options & GTKUTIL_ATTACH_EXPAND)
		return TRUE;

	return default_expand;
}

void
gtkutil_grid_attach (GtkWidget *table, GtkWidget *child,
		     guint left_attach, guint right_attach,
		     guint top_attach, guint bottom_attach,
		     GtkutilAttachOptions xoptions, GtkutilAttachOptions yoptions,
		     guint xpad, guint ypad)
{
	gtk_widget_set_hexpand (child, gtkutil_expansion_from_options (xoptions, FALSE));
	gtk_widget_set_vexpand (child, gtkutil_expansion_from_options (yoptions, FALSE));
	gtk_widget_set_halign (child, gtkutil_align_from_options (xoptions, GTK_ALIGN_CENTER));
	gtk_widget_set_valign (child, gtkutil_align_from_options (yoptions, GTK_ALIGN_CENTER));
	gtk_widget_set_margin_start (child, xpad);
	gtk_widget_set_margin_end (child, xpad);
	gtk_widget_set_margin_top (child, ypad);
	gtk_widget_set_margin_bottom (child, ypad);
	gtk_grid_attach (GTK_GRID (table), child, left_attach, top_attach,
			 right_attach - left_attach, bottom_attach - top_attach);
}

void
gtkutil_grid_attach_defaults (GtkWidget *table, GtkWidget *child,
			      guint left_attach, guint right_attach,
			      guint top_attach, guint bottom_attach)
{
	gtk_widget_set_hexpand (child, TRUE);
	gtk_widget_set_vexpand (child, TRUE);
	gtk_widget_set_halign (child, GTK_ALIGN_FILL);
	gtk_widget_set_valign (child, GTK_ALIGN_FILL);
	gtk_grid_attach (GTK_GRID (table), child, left_attach, top_attach,
			 right_attach - left_attach, bottom_attach - top_attach);
}
