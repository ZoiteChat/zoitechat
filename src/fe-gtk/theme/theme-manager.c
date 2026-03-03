#include "../fe-gtk.h"
#include "theme-manager.h"

#include <gtk/gtk.h>

#include "theme-application.h"
#include "theme-policy.h"
#include "theme-runtime.h"
#include "theme-access.h"
#include "theme-css.h"
#include "../gtkutil.h"
#include "../maingui.h"
#include "../setup.h"
#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

typedef struct
{
	guint id;
	char *component_id;
	ThemeChangedCallback callback;
	gpointer userdata;
} ThemeListener;

static GHashTable *theme_manager_listeners;
static guint theme_manager_next_listener_id = 1;
static guint theme_manager_setup_listener_id;
static const char theme_manager_window_destroy_handler_key[] = "theme-manager-window-destroy-handler";

static void
theme_listener_free (gpointer data)
{
	ThemeListener *listener = data;

	if (!listener)
		return;

	g_free (listener->component_id);
	g_free (listener);
}

static void
theme_manager_setup_apply_listener (const ThemeChangedEvent *event, gpointer userdata)
{
	(void) userdata;
	theme_manager_dispatch_setup_apply (event);
}

static ThemeChangedReason
theme_manager_synthesize_preference_reasons (const struct zoitechatprefs *old_prefs,
					      const struct zoitechatprefs *new_prefs,
					      gboolean color_change)
{
	ThemeChangedReason reasons = THEME_CHANGED_REASON_NONE;

	if (!old_prefs || !new_prefs)
		return reasons;

	if (strcmp (old_prefs->hex_text_background, new_prefs->hex_text_background) != 0)
		reasons |= THEME_CHANGED_REASON_PIXMAP;
	if (old_prefs->hex_gui_tab_dots != new_prefs->hex_gui_tab_dots ||
	    old_prefs->hex_gui_tab_layout != new_prefs->hex_gui_tab_layout)
		reasons |= THEME_CHANGED_REASON_LAYOUT;
	if (old_prefs->hex_identd_server != new_prefs->hex_identd_server ||
	    old_prefs->hex_identd_port != new_prefs->hex_identd_port)
		reasons |= THEME_CHANGED_REASON_IDENTD;
	if (color_change ||
	    old_prefs->hex_gui_ulist_color != new_prefs->hex_gui_ulist_color ||
	    old_prefs->hex_away_size_max != new_prefs->hex_away_size_max ||
	    old_prefs->hex_away_track != new_prefs->hex_away_track)
		reasons |= THEME_CHANGED_REASON_USERLIST;

	if (reasons != THEME_CHANGED_REASON_NONE)
		reasons |= THEME_CHANGED_REASON_WIDGET_STYLE;

	return reasons;
}

static void
theme_manager_auto_dark_mode_changed (GtkSettings *settings, GParamSpec *pspec, gpointer data)
{
	gboolean color_change = FALSE;
	static gboolean in_handler = FALSE;

	(void) settings;
	(void) pspec;
	(void) data;

	if (prefs.hex_gui_dark_mode != ZOITECHAT_DARK_MODE_AUTO)
		return;
	if (in_handler)
		return;

	in_handler = TRUE;

	fe_set_auto_dark_mode_state (theme_policy_system_prefers_dark ());
	theme_manager_commit_preferences (prefs.hex_gui_dark_mode, &color_change);
	if (color_change)
		theme_manager_dispatch_changed (THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST | THEME_CHANGED_REASON_MODE);

	in_handler = FALSE;
}

static guint theme_manager_auto_refresh_source = 0;
static ThemeManagerIdleAddFunc theme_manager_idle_add_func = g_idle_add;

static gboolean
theme_manager_run_auto_refresh (gpointer data)
{
	theme_manager_auto_refresh_source = 0;
	theme_manager_auto_dark_mode_changed (NULL, NULL, data);
	return G_SOURCE_REMOVE;
}

static void
theme_manager_queue_auto_refresh (GtkSettings *settings, GParamSpec *pspec, gpointer data)
{
	(void) settings;
	(void) pspec;

	if (theme_manager_auto_refresh_source != 0)
		return;

	theme_manager_auto_refresh_source = theme_manager_idle_add_func (theme_manager_run_auto_refresh, data);
}

void
theme_manager_init (void)
{
	GtkSettings *settings;

	if (!theme_manager_listeners)
		theme_manager_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
									 theme_listener_free);

	if (!theme_manager_setup_listener_id)
		theme_manager_setup_listener_id = theme_listener_register ("setup.apply", theme_manager_setup_apply_listener, NULL);

	settings = gtk_settings_get_default ();
	if (settings)
		fe_set_auto_dark_mode_state (theme_policy_system_prefers_dark ());

	theme_application_apply_mode (prefs.hex_gui_dark_mode, NULL);
	zoitechat_set_theme_post_apply_callback (theme_manager_handle_theme_applied);

	if (settings)
	{
		g_signal_connect (settings, "notify::gtk-application-prefer-dark-theme",
					  G_CALLBACK (theme_manager_queue_auto_refresh), NULL);
		g_signal_connect (settings, "notify::gtk-theme-name",
					  G_CALLBACK (theme_manager_queue_auto_refresh), NULL);
	}
}

gboolean
theme_manager_apply_mode (unsigned int mode, gboolean *palette_changed)
{
	return theme_application_apply_mode (mode, palette_changed);
}

void
theme_manager_set_mode (unsigned int mode, gboolean *palette_changed)
{
	theme_application_apply_mode (mode, palette_changed);
}

void
theme_manager_set_token_color (unsigned int mode, ThemeSemanticToken token, const GdkRGBA *color, gboolean *palette_changed)
{
	gboolean dark;
	gboolean changed = FALSE;

	if (!color)
		return;

	dark = theme_policy_is_dark_mode_active (mode);
	if (dark)
		theme_runtime_dark_set_color (token, color);
	else
		theme_runtime_user_set_color (token, color);

	changed = theme_runtime_apply_mode (mode, NULL);
	if (palette_changed)
		*palette_changed = changed;

	theme_application_reload_input_style ();
}

void
theme_manager_commit_preferences (unsigned int old_mode, gboolean *color_change)
{
	gboolean palette_changed = FALSE;

	theme_application_apply_mode (prefs.hex_gui_dark_mode, &palette_changed);
	if (color_change && (prefs.hex_gui_dark_mode != old_mode || palette_changed))
		*color_change = TRUE;

	if (prefs.hex_gui_dark_mode == ZOITECHAT_DARK_MODE_AUTO)
		fe_set_auto_dark_mode_state (theme_policy_is_dark_mode_active (ZOITECHAT_DARK_MODE_AUTO));
}

void
theme_manager_save_preferences (void)
{
	theme_runtime_save ();
}

gboolean
theme_changed_event_has_reason (const ThemeChangedEvent *event, ThemeChangedReason reason)
{
	if (!event)
		return FALSE;

	return (event->reasons & reason) != 0;
}

void
theme_manager_apply_and_dispatch (unsigned int mode, ThemeChangedReason reasons, gboolean *palette_changed)
{
	theme_application_apply_mode (mode, palette_changed);
	theme_manager_dispatch_changed (reasons);
}

void
theme_manager_dispatch_changed (ThemeChangedReason reasons)
{
	GHashTableIter iter;
	gpointer key;
	gpointer value;
	ThemeChangedEvent event;

	event.reasons = reasons;

	if (!theme_manager_listeners)
		return;

	g_hash_table_iter_init (&iter, theme_manager_listeners);
	while (g_hash_table_iter_next (&iter, &key, &value))
	{
		ThemeListener *listener = value;

		if (listener->callback)
			listener->callback (&event, listener->userdata);
	}
}

guint
theme_listener_register (const char *component_id, ThemeChangedCallback callback, gpointer userdata)
{
	ThemeListener *listener;
	guint id;

	if (!callback)
		return 0;

	if (!theme_manager_listeners)
		theme_manager_listeners = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
								     theme_listener_free);

	id = theme_manager_next_listener_id++;
	if (theme_manager_next_listener_id == 0)
		theme_manager_next_listener_id = 1;

	listener = g_new0 (ThemeListener, 1);
	listener->id = id;
	listener->component_id = g_strdup (component_id ? component_id : "theme.listener");
	listener->callback = callback;
	listener->userdata = userdata;

	g_hash_table_insert (theme_manager_listeners, GUINT_TO_POINTER (id), listener);

	return id;
}

void
theme_listener_unregister (guint listener_id)
{
	if (!theme_manager_listeners || listener_id == 0)
		return;

	g_hash_table_remove (theme_manager_listeners, GUINT_TO_POINTER (listener_id));
}

void
theme_manager_handle_theme_applied (void)
{
	theme_runtime_load ();
	theme_runtime_apply_mode (prefs.hex_gui_dark_mode, NULL);
	theme_manager_dispatch_changed (THEME_CHANGED_REASON_THEME_PACK | THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST | THEME_CHANGED_REASON_MODE);
}

static void
theme_manager_apply_platform_window_theme (GtkWidget *window)
{
#ifdef G_OS_WIN32
	GtkStyleContext *context;
	gboolean dark;

	if (!window)
		return;

	context = gtk_widget_get_style_context (window);
	dark = theme_runtime_is_dark_active ();
	if (context)
	{
		gtk_style_context_remove_class (context, "zoitechat-dark");
		gtk_style_context_remove_class (context, "zoitechat-light");
		gtk_style_context_add_class (context, dark ? "zoitechat-dark" : "zoitechat-light");
	}
	fe_win32_apply_native_titlebar (window, dark);
#else
	(void) window;
#endif
}

static void
theme_manager_window_destroy_cb (GtkWidget *window, gpointer userdata)
{
	(void) userdata;
	g_object_set_data (G_OBJECT (window), theme_manager_window_destroy_handler_key, NULL);
}

void
theme_manager_apply_to_window (GtkWidget *window)
{
	if (!window)
		return;

	theme_manager_apply_platform_window_theme (window);
}

void
theme_manager_attach_window (GtkWidget *window)
{
	gulong *handler_id;

	if (!window)
		return;

	handler_id = g_object_get_data (G_OBJECT (window), theme_manager_window_destroy_handler_key);
	if (!handler_id)
	{
		handler_id = g_new (gulong, 1);
		*handler_id = g_signal_connect (G_OBJECT (window), "destroy", G_CALLBACK (theme_manager_window_destroy_cb), NULL);
		g_object_set_data_full (G_OBJECT (window), theme_manager_window_destroy_handler_key, handler_id, g_free);
	}

	theme_manager_apply_to_window (window);
}

void
theme_manager_detach_window (GtkWidget *window)
{
	gulong *handler_id;

	if (!window)
		return;

	handler_id = g_object_get_data (G_OBJECT (window), theme_manager_window_destroy_handler_key);
	if (handler_id)
	{
		g_signal_handler_disconnect (G_OBJECT (window), *handler_id);
		g_object_set_data (G_OBJECT (window), theme_manager_window_destroy_handler_key, NULL);
	}
}

void
theme_manager_apply_palette_widget (GtkWidget *widget, const GdkRGBA *bg, const GdkRGBA *fg,
			       const PangoFontDescription *font_desc)
{
	theme_css_apply_palette_widget (widget, bg, fg, font_desc);
}

void
theme_manager_apply_entry_palette (GtkWidget *widget, const PangoFontDescription *font_desc)
{
	ThemeWidgetStyleValues style_values;

	if (!widget || !font_desc)
		return;

	theme_get_widget_style_values (&style_values);
	gtkutil_apply_palette (widget, &style_values.background, &style_values.foreground, font_desc);
}

ThemePaletteBehavior
theme_manager_get_userlist_palette_behavior (const PangoFontDescription *font_desc)
{
	ThemePaletteBehavior behavior;
	gboolean dark_mode_active = theme_policy_is_dark_mode_active (prefs.hex_gui_dark_mode);

	behavior.font_desc = font_desc;
	behavior.apply_background = prefs.hex_gui_ulist_style || dark_mode_active;
	behavior.apply_foreground = dark_mode_active;

	return behavior;
}

ThemePaletteBehavior
theme_manager_get_channel_tree_palette_behavior (const PangoFontDescription *font_desc)
{
	ThemePaletteBehavior behavior;
	gboolean dark_mode_active = theme_policy_is_dark_mode_active (prefs.hex_gui_dark_mode);

	behavior.font_desc = font_desc;
	behavior.apply_background = dark_mode_active || prefs.hex_gui_dark_mode == ZOITECHAT_DARK_MODE_LIGHT;
	behavior.apply_foreground = dark_mode_active || prefs.hex_gui_dark_mode == ZOITECHAT_DARK_MODE_LIGHT;

	return behavior;
}

void
theme_manager_apply_userlist_palette (GtkWidget *widget, const PangoFontDescription *font_desc,
				       gboolean prefer_background, gboolean prefer_foreground)
{
	ThemePaletteBehavior behavior;

	behavior.font_desc = font_desc;
	behavior.apply_background = prefer_background;
	behavior.apply_foreground = prefer_foreground;
	theme_manager_apply_userlist_style (widget, behavior);
}

void
theme_manager_apply_userlist_style (GtkWidget *widget, ThemePaletteBehavior behavior)
{
	ThemeWidgetStyleValues style_values;
	const GdkRGBA *background = NULL;
	const GdkRGBA *foreground = NULL;

	if (!widget)
		return;

	theme_get_widget_style_values (&style_values);
	if (behavior.apply_background)
		background = &style_values.background;
	if (behavior.apply_foreground)
		foreground = &style_values.foreground;

	gtkutil_apply_palette (widget, background, foreground, behavior.font_desc);
}

void
theme_manager_apply_channel_tree_style (GtkWidget *widget, ThemePaletteBehavior behavior)
{
	theme_manager_apply_userlist_style (widget, behavior);
}

void
theme_manager_apply_input_style (gboolean enabled, const PangoFontDescription *font_desc)
{
	theme_css_reload_input_style (enabled, font_desc);
}

void
theme_manager_reload_input_style (void)
{
	theme_application_reload_input_style ();
}

void
theme_manager_refresh_auto_mode (void)
{
	theme_manager_queue_auto_refresh (NULL, NULL, NULL);
}

ThemeChangedEvent
theme_manager_on_preferences_changed (const struct zoitechatprefs *old_prefs,
				      const struct zoitechatprefs *new_prefs,
				      unsigned int old_mode,
				      gboolean *color_change)
{
	ThemeChangedEvent event;
	gboolean had_color_change = color_change && *color_change;

	theme_manager_commit_preferences (old_mode, color_change);
	event.reasons = theme_manager_synthesize_preference_reasons (old_prefs, new_prefs,
						     had_color_change || (color_change && *color_change));

	return event;
}

void
theme_manager_dispatch_setup_apply (const ThemeChangedEvent *event)
{
	if (!event)
		return;

	setup_apply_real (event);
}

void
theme_manager_set_idle_add_func (ThemeManagerIdleAddFunc idle_add_func)
{
	theme_manager_idle_add_func = idle_add_func ? idle_add_func : g_idle_add;
	theme_manager_auto_refresh_source = 0;
}
