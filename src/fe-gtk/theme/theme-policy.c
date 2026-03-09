#include "theme-policy.h"

#include "../fe-gtk.h"
#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

gboolean
theme_policy_system_prefers_dark (void)
{
	GtkSettings *settings = gtk_settings_get_default ();
	gboolean prefer_dark = FALSE;
	char *theme_name = NULL;
#ifdef G_OS_WIN32
	gboolean have_win_pref = FALSE;

	if (fe_win32_high_contrast_is_enabled ())
		return FALSE;

	have_win_pref = fe_win32_try_get_system_dark (&prefer_dark);
	if (!have_win_pref)
#endif
	if (settings && g_object_class_find_property (G_OBJECT_GET_CLASS (settings),
	                                              "gtk-application-prefer-dark-theme"))
	{
		g_object_get (settings, "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
	}

	if (settings && !prefer_dark)
	{
		g_object_get (settings, "gtk-theme-name", &theme_name, NULL);
		if (theme_name)
		{
			char *lower = g_ascii_strdown (theme_name, -1);
			if (g_str_has_suffix (lower, "-dark") || g_strrstr (lower, "dark"))
				prefer_dark = TRUE;
			g_free (lower);
			g_free (theme_name);
		}
	}

	return prefer_dark;
}

gboolean
theme_policy_is_dark_mode_active (unsigned int mode)
{
	if (mode == ZOITECHAT_DARK_MODE_AUTO)
		return theme_policy_system_prefers_dark ();

	return fe_dark_mode_is_enabled_for (mode);
}

gboolean
theme_policy_is_app_dark_mode_active (void)
{
	return theme_policy_is_dark_mode_active (prefs.hex_gui_dark_mode);
}
