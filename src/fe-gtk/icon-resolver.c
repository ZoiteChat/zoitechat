#include <string.h>

#include "fe-gtk.h"
#include "icon-resolver.h"
#include "../common/cfgfiles.h"

typedef struct
{
	IconResolverRole role;
	int item;
	const char *custom_icon_name;
	const char *system_icon_name;
	const char *resource_name;
} IconRegistryEntry;

typedef struct
{
	const char *stock_name;
	const char *system_icon_name;
} StockIconMap;

typedef struct
{
	const char *key;
	const char *custom_icon_name;
} MenuMap;

static const IconRegistryEntry icon_registry[] = {
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_NEW, "zc-menu-new", "document-new", "new" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_NETWORK_LIST, "zc-menu-network-list", "view-list", "network-list" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_LOAD_PLUGIN, "zc-menu-load-plugin", "document-open", "load-plugin" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_DETACH, "zc-menu-detach", "edit-redo", "detach" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_CLOSE, "zc-menu-close", "window-close", "close" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_QUIT, "zc-menu-quit", "application-exit", "quit" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_DISCONNECT, "zc-menu-disconnect", "network-disconnect", "disconnect" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_CONNECT, "zc-menu-connect", "network-connect", "connect" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_JOIN, "zc-menu-join", "go-jump", "join" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_PREFERENCES, "zc-menu-preferences", "preferences-system", "preferences" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_CLEAR, "zc-menu-clear", "edit-clear", "clear" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_COPY, "zc-menu-copy", "edit-copy", "copy" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_DELETE, "zc-menu-delete", "edit-delete", "delete" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_ADD, "zc-menu-add", "list-add", "add" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_REMOVE, "zc-menu-remove", "list-remove", "remove" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_SPELL_CHECK, "zc-menu-spell-check", "tools-check-spelling", "spell-check" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_SAVE, "zc-menu-save", "document-save", "save" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_SAVE_AS, "zc-menu-save-as", "document-save-as", "save-as" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_REFRESH, "zc-menu-refresh", "view-refresh", "refresh" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_SEARCH, "zc-menu-search", "edit-find", "search" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_FIND, "zc-menu-find", "edit-find", "find" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_PREVIOUS, "zc-menu-previous", "go-previous", "previous" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_NEXT, "zc-menu-next", "go-next", "next" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_HELP, "zc-menu-help", "help-browser", "help" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_ABOUT, "zc-menu-about", "help-about", "about" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_EMOJI, "zc-menu-emoji", "face-smile", "emoji" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_UPDATE, "zc-menu-update", "software-update-available", "update" },
	{ ICON_RESOLVER_ROLE_MENU_ACTION, ICON_RESOLVER_MENU_ACTION_CHANLIST, "zc-menu-chanlist", "network-workgroup", "chanlist" },
	{ ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_NORMAL, NULL, "zoitechat", "tray_normal" },
	{ ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_FILEOFFER, NULL, "mail-attachment", "tray_fileoffer" },
	{ ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_HIGHLIGHT, NULL, "dialog-warning", "tray_highlight" },
	{ ICON_RESOLVER_ROLE_TRAY_STATE, ICON_RESOLVER_TRAY_STATE_MESSAGE, NULL, "mail-unread", "tray_message" },
	{ ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_CHANNEL, NULL, "folder", "tree_channel" },
	{ ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_DIALOG, NULL, "mail-message-new", "tree_dialog" },
	{ ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_SERVER, NULL, "network-server", "tree_server" },
	{ ICON_RESOLVER_ROLE_TREE_TYPE, ICON_RESOLVER_TREE_TYPE_UTIL, NULL, "applications-utilities", "tree_util" },
	{ ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_VOICE, NULL, "emblem-ok", "ulist_voice" },
	{ ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_HALFOP, NULL, "emblem-shared", "ulist_halfop" },
	{ ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_OP, NULL, "emblem-default", "ulist_op" },
	{ ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_OWNER, NULL, "emblem-favorite", "ulist_owner" },
	{ ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_FOUNDER, NULL, "emblem-favorite", "ulist_founder" },
	{ ICON_RESOLVER_ROLE_USERLIST_RANK, ICON_RESOLVER_USERLIST_RANK_NETOP, NULL, "emblem-system", "ulist_netop" }
};

static const StockIconMap stock_icon_map[] = {
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
	{ "gtk-spell-check", "tools-check-spelling" }
};

static const MenuMap stock_menu_map[] = {
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
	{ "gtk-convert", "zc-menu-emoji" }
};

static const MenuMap icon_menu_map[] = {
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
	{ "network-workgroup", "zc-menu-chanlist" }
};

static const IconRegistryEntry *
icon_registry_find (IconResolverRole role, int item)
{
	size_t i;

	for (i = 0; i < G_N_ELEMENTS (icon_registry); i++)
	{
		if (icon_registry[i].role == role && icon_registry[i].item == item)
			return &icon_registry[i];
	}

	return NULL;
}

static const IconRegistryEntry *
icon_registry_find_custom (const char *custom_icon_name)
{
	size_t i;

	if (!custom_icon_name)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (icon_registry); i++)
	{
		if (icon_registry[i].custom_icon_name && strcmp (icon_registry[i].custom_icon_name, custom_icon_name) == 0)
			return &icon_registry[i];
	}

	return NULL;
}

static const char *
menu_map_lookup (const MenuMap *map, size_t map_len, const char *key)
{
	size_t i;

	if (!key)
		return NULL;

	for (i = 0; i < map_len; i++)
	{
		if (strcmp (key, map[i].key) == 0)
			return map[i].custom_icon_name;
	}

	return NULL;
}

const char *
icon_resolver_icon_name_from_stock (const char *stock_name)
{
	size_t i;

	if (!stock_name)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (stock_icon_map); i++)
	{
		if (strcmp (stock_name, stock_icon_map[i].stock_name) == 0)
			return stock_icon_map[i].system_icon_name;
	}

	return stock_name;
}

const char *
icon_resolver_menu_custom_icon_from_stock (const char *stock_name)
{
	return menu_map_lookup (stock_menu_map, G_N_ELEMENTS (stock_menu_map), stock_name);
}

const char *
icon_resolver_menu_custom_icon_from_icon_name (const char *icon_name)
{
	return menu_map_lookup (icon_menu_map, G_N_ELEMENTS (icon_menu_map), icon_name);
}

const char *
icon_resolver_icon_name_for_menu_custom (const char *custom_icon_name)
{
	const IconRegistryEntry *entry = icon_registry_find_custom (custom_icon_name);

	if (!entry)
		return NULL;

	return entry->system_icon_name;
}

gboolean
icon_resolver_menu_action_from_custom (const char *custom_icon_name, int *action_out)
{
	const IconRegistryEntry *entry = icon_registry_find_custom (custom_icon_name);

	if (!entry || entry->role != ICON_RESOLVER_ROLE_MENU_ACTION)
		return FALSE;

	if (action_out)
		*action_out = entry->item;

	return TRUE;
}

IconResolverThemeVariant
icon_resolver_detect_theme_variant (void)
{
	GtkSettings *settings;
	gboolean prefer_dark = FALSE;
	char *theme_name = NULL;
	char *theme_name_lower = NULL;
	IconResolverThemeVariant theme_variant = ICON_RESOLVER_THEME_LIGHT;

	settings = gtk_settings_get_default ();
	if (settings)
	{
		g_object_get (G_OBJECT (settings), "gtk-application-prefer-dark-theme", &prefer_dark, NULL);
		g_object_get (G_OBJECT (settings), "gtk-theme-name", &theme_name, NULL);
	}

	if (theme_name)
		theme_name_lower = g_ascii_strdown (theme_name, -1);
	if (prefer_dark || (theme_name_lower && g_strrstr (theme_name_lower, "dark")))
		theme_variant = ICON_RESOLVER_THEME_DARK;

	g_free (theme_name_lower);
	g_free (theme_name);

	return theme_variant;
}

static gboolean
resource_exists (const char *resource_path)
{
	return g_resources_get_info (resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL);
}

static char *
resolve_user_override (const IconRegistryEntry *entry, IconResolverThemeVariant variant)
{
	const char *variant_name = variant == ICON_RESOLVER_THEME_DARK ? "dark" : "light";
	char *path;

	path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "icons" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s.png",
	                        get_xdir (), variant_name, entry->resource_name);
	if (g_file_test (path, G_FILE_TEST_EXISTS))
		return path;
	g_free (path);

	path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "icons" G_DIR_SEPARATOR_S "%s-%s.png",
	                        get_xdir (), entry->resource_name, variant_name);
	if (g_file_test (path, G_FILE_TEST_EXISTS))
		return path;
	g_free (path);

	path = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "icons" G_DIR_SEPARATOR_S "%s.png",
	                        get_xdir (), entry->resource_name);
	if (g_file_test (path, G_FILE_TEST_EXISTS))
		return path;
	g_free (path);

	return NULL;
}

static char *
resolve_bundled_variant (const IconRegistryEntry *entry, IconResolverThemeVariant variant)
{
	const char *variant_name = variant == ICON_RESOLVER_THEME_DARK ? "dark" : "light";
	char *path;

	if (entry->role == ICON_RESOLVER_ROLE_MENU_ACTION)
	{
		path = g_strdup_printf ("/icons/menu/%s/%s.png", variant_name, entry->resource_name);
		if (resource_exists (path))
			return path;
		g_free (path);

		path = g_strdup_printf ("/icons/menu/%s/%s.svg", variant_name, entry->resource_name);
		if (resource_exists (path))
			return path;
		g_free (path);
	}
	else
	{
		path = g_strdup_printf ("/icons/%s-%s.png", entry->resource_name, variant_name);
		if (resource_exists (path))
			return path;
		g_free (path);
	}

	return NULL;
}

static char *
resolve_bundled_neutral (const IconRegistryEntry *entry)
{
	char *path;

	if (entry->role == ICON_RESOLVER_ROLE_MENU_ACTION)
	{
		path = g_strdup_printf ("/icons/menu/light/%s.png", entry->resource_name);
		if (resource_exists (path))
			return path;
		g_free (path);

		path = g_strdup_printf ("/icons/menu/light/%s.svg", entry->resource_name);
		if (resource_exists (path))
			return path;
		g_free (path);
	}
	else
	{
		path = g_strdup_printf ("/icons/%s.png", entry->resource_name);
		if (resource_exists (path))
			return path;
		g_free (path);
	}

	return NULL;
}

char *
icon_resolver_resolve_path (IconResolverRole role, int item, GtkIconSize size,
                            const char *context, IconResolverThemeVariant variant,
                            const char **system_icon_name)
{
	const IconRegistryEntry *entry;
	IconResolverThemeVariant effective_variant = variant;
	char *path;

	(void)size;
	(void)context;

	entry = icon_registry_find (role, item);
	if (!entry)
		return NULL;

	if (system_icon_name)
		*system_icon_name = entry->system_icon_name;

	if (effective_variant == ICON_RESOLVER_THEME_SYSTEM)
		effective_variant = icon_resolver_detect_theme_variant ();

	path = resolve_user_override (entry, effective_variant);
	if (path)
		return path;

	path = resolve_bundled_variant (entry, effective_variant);
	if (path)
		return path;

	return resolve_bundled_neutral (entry);
}
