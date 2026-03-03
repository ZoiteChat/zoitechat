#include "theme-application.h"

#include "../../common/fe.h"
#include "../../common/zoitechatc.h"
#include "theme-css.h"
#include "theme-runtime.h"
#include "../maingui.h"

#ifdef G_OS_WIN32
#include <gtk/gtk.h>

static void
theme_application_apply_windows_theme (gboolean dark)
{
	GtkSettings *settings = gtk_settings_get_default ();

	if (settings && g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
	                                              "gtk-application-prefer-dark-theme"))
	{
		g_object_set (settings, "gtk-application-prefer-dark-theme", dark, NULL);
	}

	{
		static GtkCssProvider *win_theme_provider = NULL;
		char *css = theme_css_build_toplevel_classes ();

		if (!win_theme_provider)
			win_theme_provider = gtk_css_provider_new ();

		gtk_css_provider_load_from_data (win_theme_provider, css, -1, NULL);
		theme_css_apply_app_provider (GTK_STYLE_PROVIDER (win_theme_provider));
		g_free (css);
	}
}
#endif

gboolean
theme_application_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	gboolean dark;

	theme_runtime_load ();
	dark = theme_runtime_apply_mode (mode, palette_changed);

#ifdef G_OS_WIN32
	theme_application_apply_windows_theme (dark);
#endif

	theme_application_reload_input_style ();

	return dark;
}

void
theme_application_reload_input_style (void)
{
	input_style = theme_application_update_input_style (input_style);
}

InputStyle *
theme_application_update_input_style (InputStyle *style)
{
	char buf[256];

	if (!style)
		style = g_new0 (InputStyle, 1);

	if (style->font_desc)
		pango_font_description_free (style->font_desc);
	style->font_desc = pango_font_description_from_string (prefs.hex_text_font);

	if (pango_font_description_get_size (style->font_desc) == 0)
	{
		g_snprintf (buf, sizeof (buf), _("Failed to open font:\n\n%s"), prefs.hex_text_font);
		fe_message (buf, FE_MSG_ERROR);
		pango_font_description_free (style->font_desc);
		style->font_desc = pango_font_description_from_string ("sans 11");
	}

	theme_css_reload_input_style (prefs.hex_gui_input_style, style->font_desc);

	return style;
}
