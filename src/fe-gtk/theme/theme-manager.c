#include "../fe-gtk.h"
#include "theme-manager.h"

#include <gtk/gtk.h>
#include <string.h>

#include "theme-application.h"
#include "theme-policy.h"
#include "theme-runtime.h"
#include "theme-access.h"
#include "theme-css.h"
#include "theme-gtk3.h"
#include "../gtkutil.h"
#include "../maingui.h"
#include "../setup.h"
#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

void theme_runtime_reset_mode_colors (gboolean dark_mode);

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
static const char theme_manager_window_csd_headerbar_key[] = "theme-manager-window-csd-headerbar";

typedef struct
{
	gboolean initialized;
	gboolean resolved_dark_preference;
	char gtk3_theme_id[sizeof prefs.hex_gui_gtk3_theme];
	int gtk3_variant;
} ThemeManagerAutoRefreshCache;

static ThemeManagerAutoRefreshCache theme_manager_auto_refresh_cache;

static void theme_manager_apply_platform_window_theme (GtkWidget *window);

static void
theme_manager_apply_to_toplevel_windows (void)
{
	GList *toplevels;
	GList *iter;

	toplevels = gtk_window_list_toplevels ();
	for (iter = toplevels; iter != NULL; iter = iter->next)
	{
		GtkWidget *window = GTK_WIDGET (iter->data);

		if (!GTK_IS_WINDOW (window) || gtk_widget_get_mapped (window) == FALSE)
			continue;

		theme_manager_apply_platform_window_theme (window);
	}
	g_list_free (toplevels);
}

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
	    old_prefs->hex_text_color_nicks != new_prefs->hex_text_color_nicks ||
	    old_prefs->hex_away_size_max != new_prefs->hex_away_size_max ||
	    old_prefs->hex_away_track != new_prefs->hex_away_track)
		reasons |= THEME_CHANGED_REASON_USERLIST;

	if (reasons != THEME_CHANGED_REASON_NONE)
		reasons |= THEME_CHANGED_REASON_WIDGET_STYLE;

	return reasons;
}

static gboolean
theme_manager_should_refresh_gtk3 (void)
{
	return prefs.hex_gui_gtk3_variant == THEME_GTK3_VARIANT_FOLLOW_SYSTEM;
}

static void
theme_manager_auto_dark_mode_changed (GtkSettings *settings, GParamSpec *pspec, gpointer data)
{
	gboolean color_change = FALSE;
	gboolean should_refresh_gtk3;
	gboolean gtk3_refresh;
	gboolean resolved_dark_preference;
	static gboolean in_handler = FALSE;

	(void) settings;
	(void) pspec;
	(void) data;

	resolved_dark_preference = theme_policy_system_prefers_dark ();
	gtk3_refresh = theme_manager_should_refresh_gtk3 ();
	should_refresh_gtk3 = gtk3_refresh || prefs.hex_gui_dark_mode == ZOITECHAT_DARK_MODE_AUTO;

	if (theme_manager_auto_refresh_cache.initialized &&
	    theme_manager_auto_refresh_cache.resolved_dark_preference == resolved_dark_preference &&
	    theme_manager_auto_refresh_cache.gtk3_variant == prefs.hex_gui_gtk3_variant &&
	    g_strcmp0 (theme_manager_auto_refresh_cache.gtk3_theme_id, prefs.hex_gui_gtk3_theme) == 0)
		return;

	theme_manager_auto_refresh_cache.initialized = TRUE;
	theme_manager_auto_refresh_cache.resolved_dark_preference = resolved_dark_preference;
	theme_manager_auto_refresh_cache.gtk3_variant = prefs.hex_gui_gtk3_variant;
	g_strlcpy (theme_manager_auto_refresh_cache.gtk3_theme_id,
		   prefs.hex_gui_gtk3_theme,
		   sizeof (theme_manager_auto_refresh_cache.gtk3_theme_id));

	if (prefs.hex_gui_dark_mode != ZOITECHAT_DARK_MODE_AUTO && !gtk3_refresh)
		return;
	if (in_handler)
		return;

	in_handler = TRUE;

	if (prefs.hex_gui_dark_mode == ZOITECHAT_DARK_MODE_AUTO)
	{
		fe_set_auto_dark_mode_state (resolved_dark_preference);
		theme_manager_commit_preferences (prefs.hex_gui_dark_mode, &color_change);
		if (color_change)
			theme_manager_dispatch_changed (THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST | THEME_CHANGED_REASON_MODE);
	}

	if (should_refresh_gtk3)
		theme_gtk3_apply_current (NULL);

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
	theme_gtk3_init ();
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
	gboolean changed = FALSE;

	if (!color)
		return;

	(void) mode;
	theme_runtime_user_set_color (token, color);

	theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_LIGHT, &changed);
	if (palette_changed)
		*palette_changed = changed;

	if (changed)
		theme_manager_dispatch_changed (THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST);

	theme_application_reload_input_style ();
}

void
theme_manager_reset_mode_colors (unsigned int mode, gboolean *palette_changed)
{
	gboolean changed;

	(void) mode;
	theme_runtime_reset_mode_colors (FALSE);
	theme_runtime_apply_mode (ZOITECHAT_DARK_MODE_LIGHT, &changed);
	changed = TRUE;
	if (palette_changed)
		*palette_changed = changed;
	theme_manager_dispatch_changed (THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST);

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

	if ((reasons & (THEME_CHANGED_REASON_MODE |
			    THEME_CHANGED_REASON_THEME_PACK |
			    THEME_CHANGED_REASON_WIDGET_STYLE)) != 0)
	{
		theme_manager_apply_to_toplevel_windows ();
	}

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
	theme_gtk3_invalidate_provider_cache ();
	if (prefs.hex_gui_gtk3_theme[0])
		theme_gtk3_refresh (prefs.hex_gui_gtk3_theme, (ThemeGtk3Variant) prefs.hex_gui_gtk3_variant, NULL);
	theme_application_apply_mode (prefs.hex_gui_dark_mode, NULL);
	theme_manager_dispatch_changed (THEME_CHANGED_REASON_THEME_PACK | THEME_CHANGED_REASON_PALETTE | THEME_CHANGED_REASON_WIDGET_STYLE | THEME_CHANGED_REASON_USERLIST | THEME_CHANGED_REASON_MODE);
}


static gboolean
theme_manager_is_kde_wayland (void)
{
	const char *wayland_display;
	const char *desktop;
	char *desktop_lower;
	gboolean is_kde;

	wayland_display = g_getenv ("WAYLAND_DISPLAY");
	if (!wayland_display || !wayland_display[0])
		return FALSE;

	desktop = g_getenv ("XDG_CURRENT_DESKTOP");
	if (!desktop || !desktop[0])
		desktop = g_getenv ("XDG_SESSION_DESKTOP");
	if (!desktop || !desktop[0])
		return FALSE;

	desktop_lower = g_ascii_strdown (desktop, -1);
	is_kde = strstr (desktop_lower, "kde") != NULL || strstr (desktop_lower, "plasma") != NULL;
	g_free (desktop_lower);
	return is_kde;
}

static void
theme_manager_apply_wayland_kde_csd (GtkWidget *window)
{
	GtkWindow *gtk_window;
	GtkWidget *headerbar;
	gboolean enable_csd;

	if (!window || !GTK_IS_WINDOW (window))
		return;

	gtk_window = GTK_WINDOW (window);
	enable_csd = theme_gtk3_is_active () && theme_manager_is_kde_wayland ();
	headerbar = g_object_get_data (G_OBJECT (window), theme_manager_window_csd_headerbar_key);

	if (enable_csd)
	{
		if (!headerbar)
		{
			GtkWidget *icon_image;
			GdkPixbuf *icon_pixbuf;

			if (gtk_widget_get_realized (window))
				return;

			headerbar = gtk_header_bar_new ();
			gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (headerbar), TRUE);
			gtk_header_bar_set_decoration_layout (GTK_HEADER_BAR (headerbar), "menu:minimize,maximize,close");
			icon_pixbuf = gdk_pixbuf_new_from_resource_at_scale ("/icons/zoitechat.svg", 32, 32, TRUE, NULL);
			if (!icon_pixbuf)
				icon_pixbuf = gdk_pixbuf_new_from_resource_at_scale ("/icons/zoitechat.png", 32, 32, TRUE, NULL);
			icon_image = icon_pixbuf ? gtk_image_new_from_pixbuf (icon_pixbuf) : gtk_image_new_from_resource ("/icons/zoitechat.png");
			if (icon_pixbuf)
				g_object_unref (icon_pixbuf);
			gtk_header_bar_pack_start (GTK_HEADER_BAR (headerbar), icon_image);
			gtk_widget_show (icon_image);
			gtk_window_set_titlebar (gtk_window, headerbar);
			g_object_set_data (G_OBJECT (window), theme_manager_window_csd_headerbar_key, headerbar);
		}
		gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), gtk_window_get_title (gtk_window));
		gtk_widget_show (headerbar);
		{
			GdkScreen *screen = gdk_screen_get_default ();
			if (screen)
				gtk_style_context_reset_widgets (screen);
		}
		return;
	}

	if (headerbar)
	{
		if (gtk_widget_get_realized (window))
			return;
		gtk_window_set_titlebar (gtk_window, NULL);
		g_object_set_data (G_OBJECT (window), theme_manager_window_csd_headerbar_key, NULL);
	}

	{
		GdkScreen *screen = gdk_screen_get_default ();
		if (screen)
			gtk_style_context_reset_widgets (screen);
	}
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
	if (theme_gtk3_is_active ())
	{
		dark = prefs.hex_gui_gtk3_variant == THEME_GTK3_VARIANT_PREFER_DARK;
		if (prefs.hex_gui_gtk3_variant == THEME_GTK3_VARIANT_FOLLOW_SYSTEM)
			dark = theme_policy_system_prefers_dark ();
	}
	else
		dark = theme_runtime_is_dark_active ();
	if (context)
	{
		gtk_style_context_remove_class (context, "zoitechat-dark");
		gtk_style_context_remove_class (context, "zoitechat-light");
		gtk_style_context_add_class (context, dark ? "zoitechat-dark" : "zoitechat-light");
	}
	fe_win32_apply_native_titlebar (window, dark);
#else
	theme_manager_apply_wayland_kde_csd (window);
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

	theme_get_widget_style_values_for_widget (widget, &style_values);
	gtkutil_apply_palette (widget, &style_values.background, &style_values.foreground, font_desc);
}

ThemePaletteBehavior
theme_manager_get_userlist_palette_behavior (const PangoFontDescription *font_desc)
{
	ThemePaletteBehavior behavior;

	behavior.font_desc = font_desc;
	behavior.apply_background = TRUE;
	behavior.apply_foreground = (prefs.hex_gui_ulist_color || prefs.hex_text_color_nicks) ? FALSE : TRUE;

	return behavior;
}

ThemePaletteBehavior
theme_manager_get_channel_tree_palette_behavior (const PangoFontDescription *font_desc)
{
	ThemePaletteBehavior behavior;

	behavior.font_desc = font_desc;
	behavior.apply_background = TRUE;
	behavior.apply_foreground = TRUE;

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

	theme_get_widget_style_values_for_widget (widget, &style_values);
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
