#include "theme-gtk3.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <gio/gio.h>
#include <string.h>

#include "theme-policy.h"
#include "../../common/gtk3-theme-service.h"
#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

static GPtrArray *theme_gtk3_providers_base;
static GPtrArray *theme_gtk3_providers_variant;
static GHashTable *theme_gtk3_provider_cache;
static gboolean theme_gtk3_active;
static char *theme_gtk3_current_id;
static ThemeGtk3Variant theme_gtk3_current_variant;

typedef struct
{
	GHashTable *defaults;
	char **icon_search_path;
	gint icon_search_path_count;
	gboolean icon_search_path_captured;
} ThemeGtk3SettingsState;

static ThemeGtk3SettingsState theme_gtk3_settings_state;

static gboolean settings_apply_property (GtkSettings *settings, const char *property_name, const char *raw_value);

static gboolean
theme_gtk3_theme_name_is_dark (const char *name)
{
	char *lower;
	gboolean dark;

	if (!name || !name[0])
		return FALSE;

	lower = g_ascii_strdown (name, -1);
	dark = strstr (lower, "dark") != NULL;
	g_free (lower);
	return dark;
}

static ThemeGtk3Variant
theme_gtk3_infer_variant (const ZoitechatGtk3Theme *theme)
{
	char *css_dir;
	char *light_css;
	gboolean has_light_css;
	ThemeGtk3Variant variant;

	if (!theme)
		return THEME_GTK3_VARIANT_PREFER_LIGHT;

	css_dir = zoitechat_gtk3_theme_pick_css_dir (theme->path);
	light_css = css_dir ? g_build_filename (theme->path, css_dir, "gtk.css", NULL) : NULL;
	has_light_css = light_css && g_file_test (light_css, G_FILE_TEST_IS_REGULAR);
	g_free (light_css);
	g_free (css_dir);

	variant = THEME_GTK3_VARIANT_PREFER_LIGHT;
	if ((theme->has_dark_variant && !has_light_css) ||
	    theme_gtk3_theme_name_is_dark (theme->id) ||
	    theme_gtk3_theme_name_is_dark (theme->display_name))
		variant = THEME_GTK3_VARIANT_PREFER_DARK;

	return variant;
}

static void
settings_value_free (gpointer data)
{
	GValue *value = data;

	if (!value)
		return;

	g_value_unset (value);
	g_free (value);
}

static GValue *
settings_value_dup (const GValue *source)
{
	GValue *copy;

	copy = g_new0 (GValue, 1);
	g_value_init (copy, G_VALUE_TYPE (source));
	g_value_copy (source, copy);
	return copy;
}

static GHashTable *
settings_defaults_table (void)
{
	if (!theme_gtk3_settings_state.defaults)
	{
		theme_gtk3_settings_state.defaults = g_hash_table_new_full (
			g_str_hash,
			g_str_equal,
			g_free,
			settings_value_free);
	}

	return theme_gtk3_settings_state.defaults;
}




static void
settings_rescan_icon_theme (void)
{
	GtkIconTheme *icon_theme;

	icon_theme = gtk_icon_theme_get_default ();
	if (!icon_theme)
		return;

	gtk_icon_theme_rescan_if_needed (icon_theme);
}

static void
theme_gtk3_reset_widgets (void)
{
	GdkScreen *screen = gdk_screen_get_default ();

	if (screen)
		gtk_style_context_reset_widgets (screen);
}

static void
settings_capture_icon_search_path (void)
{
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();

	if (!icon_theme || theme_gtk3_settings_state.icon_search_path_captured)
		return;

	gtk_icon_theme_get_search_path (icon_theme, &theme_gtk3_settings_state.icon_search_path, &theme_gtk3_settings_state.icon_search_path_count);
	theme_gtk3_settings_state.icon_search_path_captured = TRUE;
}

static void
settings_append_icon_search_path (const char *path)
{
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();

	if (!icon_theme || !path || !g_file_test (path, G_FILE_TEST_IS_DIR))
		return;

	settings_capture_icon_search_path ();
	gtk_icon_theme_append_search_path (icon_theme, path);
	gtk_icon_theme_rescan_if_needed (icon_theme);
}

static void
settings_apply_icon_paths (const char *theme_root)
{
	char *icons_dir;
	char *theme_parent;

	if (!theme_root)
		return;

	icons_dir = g_build_filename (theme_root, "icons", NULL);
	theme_parent = g_path_get_dirname (theme_root);
	settings_append_icon_search_path (icons_dir);
	settings_append_icon_search_path (theme_root);
	settings_append_icon_search_path (theme_parent);
	g_free (theme_parent);
	g_free (icons_dir);
}

static void
settings_restore_icon_search_path (void)
{
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();

	if (!icon_theme || !theme_gtk3_settings_state.icon_search_path_captured)
		return;

	gtk_icon_theme_set_search_path (icon_theme, (const char * const *) theme_gtk3_settings_state.icon_search_path, theme_gtk3_settings_state.icon_search_path_count);
	gtk_icon_theme_rescan_if_needed (icon_theme);
	g_strfreev (theme_gtk3_settings_state.icon_search_path);
	theme_gtk3_settings_state.icon_search_path = NULL;
	theme_gtk3_settings_state.icon_search_path_count = 0;
	theme_gtk3_settings_state.icon_search_path_captured = FALSE;
}

static void
theme_gtk3_parsing_error_cb (GtkCssProvider *provider, GtkCssSection *section, const GError *error, gpointer user_data)
{
	(void) provider;
	(void) section;
	(void) error;
	(void) user_data;
	g_signal_stop_emission_by_name (provider, "parsing-error");
}

static GHashTable *
theme_gtk3_provider_cache_table (void)
{
	if (!theme_gtk3_provider_cache)
	{
		theme_gtk3_provider_cache = g_hash_table_new_full (
			g_str_hash,
			g_str_equal,
			g_free,
			g_object_unref);
	}

	return theme_gtk3_provider_cache;
}

static char *
theme_gtk3_provider_cache_key (const char *theme_root, const char *css_dir, gboolean prefer_dark)
{
	return g_strdup_printf ("%s\n%s\n%d", theme_root, css_dir, prefer_dark ? 1 : 0);
}

static GtkCssProvider *
theme_gtk3_provider_cache_load (const char *path, GError **error)
{
	GtkCssProvider *provider;

	provider = gtk_css_provider_new ();
	g_signal_connect (provider, "parsing-error", G_CALLBACK (theme_gtk3_parsing_error_cb), NULL);
	if (!gtk_css_provider_load_from_path (provider, path, error))
	{
		g_object_unref (provider);
		return NULL;
	}

	return provider;
}

static GtkCssProvider *
theme_gtk3_provider_cache_get_or_load (const char *theme_root, const char *css_dir, gboolean prefer_dark, GError **error)
{
	GHashTable *cache;
	char *key;
	char *css_path;
	GtkCssProvider *provider;

	cache = theme_gtk3_provider_cache_table ();
	key = theme_gtk3_provider_cache_key (theme_root, css_dir, prefer_dark);
	provider = g_hash_table_lookup (cache, key);
	if (provider)
	{
		g_object_ref (provider);
		g_free (key);
		return provider;
	}

	css_path = g_build_filename (theme_root, css_dir, prefer_dark ? "gtk-dark.css" : "gtk.css", NULL);
	provider = theme_gtk3_provider_cache_load (css_path, error);
	g_free (css_path);
	if (!provider)
	{
		g_free (key);
		return NULL;
	}

	g_hash_table_insert (cache, key, g_object_ref (provider));
	return provider;
}

void
theme_gtk3_invalidate_provider_cache (void)
{
	if (theme_gtk3_provider_cache)
		g_hash_table_remove_all (theme_gtk3_provider_cache);
}

static void
settings_apply_for_variant (ThemeGtk3Variant variant)
{
	GtkSettings *settings = gtk_settings_get_default ();
	gboolean dark = FALSE;
	GValue current = G_VALUE_INIT;
	GParamSpec *property;

	if (!settings)
		return;

	if (variant == THEME_GTK3_VARIANT_PREFER_DARK)
		dark = TRUE;
	else if (variant == THEME_GTK3_VARIANT_FOLLOW_SYSTEM)
		dark = theme_policy_system_prefers_dark ();

	property = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), "gtk-application-prefer-dark-theme");
	if (!property)
		return;

	g_value_init (&current, G_PARAM_SPEC_VALUE_TYPE (property));
	g_object_get_property (G_OBJECT (settings), "gtk-application-prefer-dark-theme", &current);
	if (g_value_get_boolean (&current) != dark)
		g_object_set (settings, "gtk-application-prefer-dark-theme", dark, NULL);
	g_value_unset (&current);
}

static gboolean
settings_theme_root_is_searchable (const char *theme_root)
{
	char *parent;
	char *user_data_themes;
	char *home_themes;
	const gchar *const *system_data_dirs;
	guint i;
	gboolean searchable = FALSE;

	if (!theme_root || !theme_root[0])
		return FALSE;

	parent = g_path_get_dirname (theme_root);
	user_data_themes = g_build_filename (g_get_user_data_dir (), "themes", NULL);
	home_themes = g_build_filename (g_get_home_dir (), ".themes", NULL);

	if (g_strcmp0 (parent, user_data_themes) == 0 ||
	    g_strcmp0 (parent, home_themes) == 0 ||
	    g_strcmp0 (parent, "/usr/share/themes") == 0)
		searchable = TRUE;

	system_data_dirs = g_get_system_data_dirs ();
	for (i = 0; !searchable && system_data_dirs && system_data_dirs[i]; i++)
	{
		char *system_themes = g_build_filename (system_data_dirs[i], "themes", NULL);
		if (g_strcmp0 (parent, system_themes) == 0)
			searchable = TRUE;
		g_free (system_themes);
	}

	g_free (home_themes);
	g_free (user_data_themes);
	g_free (parent);
	return searchable;
}


static gboolean
settings_theme_link_search_path (const char *theme_root, const char *theme_name)
{
	char *themes_root;
	char *link_path;
	gboolean ok = TRUE;

	if (!theme_root || !theme_name || !theme_name[0])
		return FALSE;

	themes_root = g_build_filename (g_get_user_data_dir (), "themes", NULL);
	if (g_mkdir_with_parents (themes_root, 0700) != 0)
	{
		g_free (themes_root);
		return FALSE;
	}

	link_path = g_build_filename (themes_root, theme_name, NULL);
	if (!g_file_test (link_path, G_FILE_TEST_EXISTS))
	{
		GFile *link_file = g_file_new_for_path (link_path);
		GError *link_error = NULL;
		ok = g_file_make_symbolic_link (link_file, theme_root, NULL, &link_error);
		g_clear_error (&link_error);
		g_object_unref (link_file);
	}

	g_free (link_path);
	g_free (themes_root);
	return ok;
}

static void
settings_apply_theme_name (const char *theme_root)
{
	GtkSettings *settings;
	char *theme_name;

	if (!theme_root)
		return;

	settings = gtk_settings_get_default ();
	if (!settings)
		return;

	theme_name = g_path_get_basename (theme_root);
	if (theme_name && theme_name[0])
	{
		gboolean searchable = settings_theme_root_is_searchable (theme_root);
		if (!searchable)
			searchable = settings_theme_link_search_path (theme_root, theme_name);
		if (searchable)
			settings_apply_property (settings, "gtk-theme-name", theme_name);
	}
	g_free (theme_name);
}

static gboolean
settings_value_equal_typed (const GValue *a, const GValue *b, GType property_type)
{
	if (property_type == G_TYPE_BOOLEAN)
		return g_value_get_boolean (a) == g_value_get_boolean (b);
	if (property_type == G_TYPE_STRING)
		return g_strcmp0 (g_value_get_string (a), g_value_get_string (b)) == 0;
	if (property_type == G_TYPE_INT)
		return g_value_get_int (a) == g_value_get_int (b);
	if (property_type == G_TYPE_UINT)
		return g_value_get_uint (a) == g_value_get_uint (b);
	if (property_type == G_TYPE_FLOAT)
		return g_value_get_float (a) == g_value_get_float (b);
	if (property_type == G_TYPE_DOUBLE)
		return g_value_get_double (a) == g_value_get_double (b);
	if (G_TYPE_IS_ENUM (property_type))
		return g_value_get_enum (a) == g_value_get_enum (b);
	if (G_TYPE_IS_FLAGS (property_type))
		return g_value_get_flags (a) == g_value_get_flags (b);
	return FALSE;
}

static gboolean
settings_parse_long (const char *text, glong min_value, glong max_value, glong *value)
{
	char *end = NULL;
	gint64 parsed;

	if (!text)
		return FALSE;

	parsed = g_ascii_strtoll (text, &end, 10);
	if (end == text || *end != '\0')
		return FALSE;
	if (parsed < min_value || parsed > max_value)
		return FALSE;

	*value = (glong) parsed;
	return TRUE;
}

static void
settings_remember_default (GtkSettings *settings, const char *property_name, GParamSpec *property)
{
	GHashTable *defaults;
	GValue current = G_VALUE_INIT;

	if (!settings || !property_name || !property)
		return;

	defaults = settings_defaults_table ();
	if (g_hash_table_contains (defaults, property_name))
		return;

	g_value_init (&current, G_PARAM_SPEC_VALUE_TYPE (property));
	g_object_get_property (G_OBJECT (settings), property_name, &current);
	g_hash_table_insert (defaults, g_strdup (property_name), settings_value_dup (&current));
	g_value_unset (&current);
}

static gboolean
settings_apply_property (GtkSettings *settings, const char *property_name, const char *raw_value)
{
	GParamSpec *property;
	GValue value = G_VALUE_INIT;
	GValue current = G_VALUE_INIT;
	GType property_type;
	gboolean ok = FALSE;
	gboolean changed = TRUE;

	property = g_object_class_find_property (G_OBJECT_GET_CLASS (settings), property_name);
	if (!property)
		return FALSE;

	settings_remember_default (settings, property_name, property);
	property_type = G_PARAM_SPEC_VALUE_TYPE (property);
	g_value_init (&value, property_type);

	if (property_type == G_TYPE_BOOLEAN)
	{
		if (g_ascii_strcasecmp (raw_value, "true") == 0 ||
		    g_ascii_strcasecmp (raw_value, "yes") == 0 ||
		    g_strcmp0 (raw_value, "1") == 0)
		{
			g_value_set_boolean (&value, TRUE);
			ok = TRUE;
		}
		else if (g_ascii_strcasecmp (raw_value, "false") == 0 ||
		         g_ascii_strcasecmp (raw_value, "no") == 0 ||
		         g_strcmp0 (raw_value, "0") == 0)
		{
			g_value_set_boolean (&value, FALSE);
			ok = TRUE;
		}
	}
	else if (property_type == G_TYPE_STRING)
	{
		g_value_set_string (&value, raw_value);
		ok = TRUE;
	}
	else if (property_type == G_TYPE_INT)
	{
		glong parsed;
		if (settings_parse_long (raw_value, G_MININT, G_MAXINT, &parsed))
		{
			g_value_set_int (&value, (gint) parsed);
			ok = TRUE;
		}
	}
	else if (property_type == G_TYPE_UINT)
	{
		glong parsed;
		if (settings_parse_long (raw_value, 0, G_MAXUINT, &parsed))
		{
			g_value_set_uint (&value, (guint) parsed);
			ok = TRUE;
		}
	}
	else if (property_type == G_TYPE_DOUBLE)
	{
		char *end = NULL;
		double parsed = g_ascii_strtod (raw_value, &end);
		if (end != raw_value && *end == '\0')
		{
			g_value_set_double (&value, parsed);
			ok = TRUE;
		}
	}
	else if (property_type == G_TYPE_FLOAT)
	{
		char *end = NULL;
		double parsed = g_ascii_strtod (raw_value, &end);
		if (end != raw_value && *end == '\0')
		{
			g_value_set_float (&value, (gfloat) parsed);
			ok = TRUE;
		}
	}
	else if (G_TYPE_IS_ENUM (property_type))
	{
		GEnumClass *enum_class = g_type_class_ref (property_type);
		GEnumValue *enum_value = g_enum_get_value_by_nick (enum_class, raw_value);
		if (!enum_value)
			enum_value = g_enum_get_value_by_name (enum_class, raw_value);
		if (!enum_value)
		{
			glong parsed;
			if (settings_parse_long (raw_value, G_MININT, G_MAXINT, &parsed))
				enum_value = g_enum_get_value (enum_class, (gint) parsed);
		}
		if (enum_value)
		{
			g_value_set_enum (&value, enum_value->value);
			ok = TRUE;
		}
		g_type_class_unref (enum_class);
	}
	else if (G_TYPE_IS_FLAGS (property_type))
	{
		GFlagsClass *flags_class = g_type_class_ref (property_type);
		char **tokens = g_strsplit_set (raw_value, ",|", -1);
		guint flags_value = 0;
		guint i = 0;
		for (; tokens && tokens[i]; i++)
		{
			char *token = g_strstrip (tokens[i]);
			GFlagsValue *flag_value;
			if (!token[0])
				continue;
			flag_value = g_flags_get_value_by_nick (flags_class, token);
			if (!flag_value)
				flag_value = g_flags_get_value_by_name (flags_class, token);
			if (!flag_value)
			{
				glong parsed;
				if (!settings_parse_long (token, 0, G_MAXUINT, &parsed))
				{
					ok = FALSE;
					break;
				}
				flags_value |= (guint) parsed;
				ok = TRUE;
				continue;
			}
			flags_value |= flag_value->value;
			ok = TRUE;
		}
		if (ok)
			g_value_set_flags (&value, flags_value);
		g_strfreev (tokens);
		g_type_class_unref (flags_class);
	}


	if (ok)
	{
		g_value_init (&current, property_type);
		g_object_get_property (G_OBJECT (settings), property_name, &current);
		changed = !settings_value_equal_typed (&current, &value, property_type);
		g_value_unset (&current);
	}

	if (ok && changed)
		g_object_set_property (G_OBJECT (settings), property_name, &value);

	g_value_unset (&value);
	return ok;
}

static void
settings_restore_defaults (void)
{
	GtkSettings *settings = gtk_settings_get_default ();
	GHashTableIter iter;
	gpointer key;
	gpointer value;

	if (settings && theme_gtk3_settings_state.defaults)
	{
		g_hash_table_iter_init (&iter, theme_gtk3_settings_state.defaults);
		while (g_hash_table_iter_next (&iter, &key, &value))
			g_object_set_property (G_OBJECT (settings), (const char *) key, (const GValue *) value);

		g_hash_table_remove_all (theme_gtk3_settings_state.defaults);
	}

	settings_rescan_icon_theme ();
	settings_restore_icon_search_path ();
}

static void
settings_cleanup (void)
{
	if (theme_gtk3_settings_state.defaults)
	{
		g_hash_table_destroy (theme_gtk3_settings_state.defaults);
		theme_gtk3_settings_state.defaults = NULL;
	}

	if (theme_gtk3_settings_state.icon_search_path_captured)
		settings_restore_icon_search_path ();
}

static void
settings_apply_from_file (const char *theme_root, const char *css_dir)
{
	GtkSettings *settings;
	GPtrArray *settings_paths;
	char *selected_path;
	char *fallback_path;
	guint layer;

	settings = gtk_settings_get_default ();
	if (!settings)
		return;

	if (!css_dir)
		return;

	settings_apply_icon_paths (theme_root);
	settings_paths = g_ptr_array_new_with_free_func (g_free);
	selected_path = g_build_filename (theme_root, css_dir, "settings.ini", NULL);
	fallback_path = g_build_filename (theme_root, "gtk-3.0", "settings.ini", NULL);
	if (g_strcmp0 (css_dir, "gtk-3.0") != 0)
		g_ptr_array_add (settings_paths, fallback_path);
	else
		g_free (fallback_path);
	g_ptr_array_add (settings_paths, selected_path);

	for (layer = 0; layer < settings_paths->len; layer++)
	{
		GKeyFile *keyfile;
		char **keys;
		gsize n_keys = 0;
		gsize i;
		const char *settings_path = g_ptr_array_index (settings_paths, layer);

		if (!g_file_test (settings_path, G_FILE_TEST_IS_REGULAR))
			continue;

		keyfile = g_key_file_new ();
		if (!g_key_file_load_from_file (keyfile, settings_path, G_KEY_FILE_NONE, NULL))
		{
			g_key_file_unref (keyfile);
			continue;
		}

		keys = g_key_file_get_keys (keyfile, "Settings", &n_keys, NULL);
		for (i = 0; keys && i < n_keys; i++)
		{
			char *raw_value;

			raw_value = g_key_file_get_value (keyfile, "Settings", keys[i], NULL);
			if (!raw_value)
				continue;

			settings_apply_property (settings, keys[i], raw_value);
			g_free (raw_value);
		}

		g_strfreev (keys);
		g_key_file_unref (keyfile);
	}

	settings_rescan_icon_theme ();
	g_ptr_array_unref (settings_paths);
}

static void
theme_gtk3_remove_provider (void)
{
	GdkScreen *screen = gdk_screen_get_default ();
	guint i;

	if (screen && theme_gtk3_providers_variant)
	{
		for (i = 0; i < theme_gtk3_providers_variant->len; i++)
		{
			GtkCssProvider *provider = g_ptr_array_index (theme_gtk3_providers_variant, i);
			gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider));
		}
	}
	if (screen && theme_gtk3_providers_base)
	{
		for (i = 0; i < theme_gtk3_providers_base->len; i++)
		{
			GtkCssProvider *provider = g_ptr_array_index (theme_gtk3_providers_base, i);
			gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider));
		}
	}

	if (theme_gtk3_providers_variant)
		g_ptr_array_unref (theme_gtk3_providers_variant);
	if (theme_gtk3_providers_base)
		g_ptr_array_unref (theme_gtk3_providers_base);
	theme_gtk3_providers_variant = NULL;
	theme_gtk3_providers_base = NULL;
	settings_restore_defaults ();
	theme_gtk3_reset_widgets ();
	theme_gtk3_active = FALSE;
}

static gboolean
load_css_with_variant (ZoitechatGtk3Theme *theme, ThemeGtk3Variant variant, GError **error)
{
	gboolean prefer_dark = FALSE;
	GdkScreen *screen;
	GPtrArray *chain;
	guint i;

	if (variant == THEME_GTK3_VARIANT_PREFER_DARK)
		prefer_dark = TRUE;
	else if (variant == THEME_GTK3_VARIANT_FOLLOW_SYSTEM)
		prefer_dark = theme_policy_system_prefers_dark ();

	settings_apply_theme_name (theme->path);

	chain = zoitechat_gtk3_theme_build_inheritance_chain (theme->path);
	if (!chain || chain->len == 0)
	{
		if (chain)
			g_ptr_array_unref (chain);
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "GTK3 theme CSS not found."), FALSE;
	}

	theme_gtk3_providers_base = g_ptr_array_new_with_free_func (g_object_unref);
	theme_gtk3_providers_variant = g_ptr_array_new_with_free_func (g_object_unref);

	screen = gdk_screen_get_default ();
	for (i = 0; i < chain->len; i++)
	{
		const char *theme_root = g_ptr_array_index (chain, i);
		char *css_dir = zoitechat_gtk3_theme_pick_css_dir_for_minor (theme_root, gtk_get_minor_version ());
		char *variant_css;
		GtkCssProvider *provider;
		GtkCssProvider *variant_provider;

		if (!css_dir)
			continue;

		provider = theme_gtk3_provider_cache_get_or_load (theme_root, css_dir, FALSE, error);
		if (!provider)
		{
			g_free (css_dir);
			g_ptr_array_unref (chain);
			return FALSE;
		}
		if (screen)
			gtk_style_context_add_provider_for_screen (screen,
				GTK_STYLE_PROVIDER (provider),
				GTK_STYLE_PROVIDER_PRIORITY_USER + (gint) (i * 2));
		g_ptr_array_add (theme_gtk3_providers_base, provider);

		variant_css = g_build_filename (theme_root, css_dir, "gtk-dark.css", NULL);
		if (prefer_dark && g_file_test (variant_css, G_FILE_TEST_IS_REGULAR))
		{
			variant_provider = theme_gtk3_provider_cache_get_or_load (theme_root, css_dir, TRUE, error);
			if (!variant_provider)
			{
				g_free (variant_css);
				g_free (css_dir);
				g_ptr_array_unref (chain);
				return FALSE;
			}
			if (screen)
				gtk_style_context_add_provider_for_screen (screen,
					GTK_STYLE_PROVIDER (variant_provider),
					GTK_STYLE_PROVIDER_PRIORITY_USER + (gint) (i * 2) + 1);
			g_ptr_array_add (theme_gtk3_providers_variant, variant_provider);
		}
		g_free (variant_css);

		settings_apply_from_file (theme_root, css_dir);
		g_free (css_dir);
	}

	g_ptr_array_unref (chain);
	settings_apply_for_variant (variant);
	theme_gtk3_reset_widgets ();
	theme_gtk3_active = TRUE;
	return TRUE;
}

static gboolean
theme_gtk3_apply_internal (const char *theme_id, ThemeGtk3Variant variant, gboolean force_reload, GError **error)
{
	ZoitechatGtk3Theme *theme;
	char *previous_id = g_strdup (theme_gtk3_current_id);
	ThemeGtk3Variant previous_variant = theme_gtk3_current_variant;
	gboolean had_previous = theme_gtk3_active && previous_id && previous_id[0];
	gboolean ok;

	if (!force_reload &&
		theme_gtk3_active &&
		g_strcmp0 (theme_gtk3_current_id, theme_id) == 0 &&
		theme_gtk3_current_variant == variant)
		return TRUE;

	theme = zoitechat_gtk3_theme_find_by_id (theme_id);
	if (!theme)
	{
		g_free (previous_id);
		return g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOENT, "GTK3 theme not found."), FALSE;
	}

	theme_gtk3_remove_provider ();
	if (force_reload)
		theme_gtk3_invalidate_provider_cache ();
	ok = load_css_with_variant (theme, variant, error);
	zoitechat_gtk3_theme_free (theme);

	if (ok)
	{
		g_free (theme_gtk3_current_id);
		theme_gtk3_current_id = g_strdup (theme_id);
		theme_gtk3_current_variant = variant;
		g_free (previous_id);
		return TRUE;
	}

	if (had_previous)
	{
		GError *restore_error = NULL;
		theme = zoitechat_gtk3_theme_find_by_id (previous_id);
		if (theme)
		{
			if (load_css_with_variant (theme, previous_variant, &restore_error))
			{
				g_free (theme_gtk3_current_id);
				theme_gtk3_current_id = g_strdup (previous_id);
				theme_gtk3_current_variant = previous_variant;
			}
			zoitechat_gtk3_theme_free (theme);
		}
		g_clear_error (&restore_error);
	}

	g_free (previous_id);
	return ok;
}

gboolean
theme_gtk3_apply (const char *theme_id, ThemeGtk3Variant variant, GError **error)
{
	return theme_gtk3_apply_internal (theme_id, variant, FALSE, error);
}

gboolean
theme_gtk3_refresh (const char *theme_id, ThemeGtk3Variant variant, GError **error)
{
	return theme_gtk3_apply_internal (theme_id, variant, TRUE, error);
}

ThemeGtk3Variant
theme_gtk3_variant_for_theme (const char *theme_id)
{
	ZoitechatGtk3Theme *theme;
	ThemeGtk3Variant variant;

	theme = zoitechat_gtk3_theme_find_by_id (theme_id);
	if (!theme)
		return THEME_GTK3_VARIANT_PREFER_LIGHT;

	variant = theme_gtk3_infer_variant (theme);
	zoitechat_gtk3_theme_free (theme);
	return variant;
}

void
theme_gtk3_disable (void)
{
	theme_gtk3_remove_provider ();
	g_clear_pointer (&theme_gtk3_current_id, g_free);
	theme_gtk3_invalidate_provider_cache ();
	g_clear_pointer (&theme_gtk3_provider_cache, g_hash_table_destroy);
	settings_cleanup ();
}

void
theme_gtk3_init (void)
{
	theme_gtk3_apply_current (NULL);
}

gboolean
theme_gtk3_apply_current (GError **error)
{
	if (!prefs.hex_gui_gtk3_theme[0])
	{
		theme_gtk3_disable ();
		return TRUE;
	}

	return theme_gtk3_apply (prefs.hex_gui_gtk3_theme, (ThemeGtk3Variant) prefs.hex_gui_gtk3_variant, error);
}

gboolean
theme_gtk3_is_active (void)
{
	return theme_gtk3_active;
}
