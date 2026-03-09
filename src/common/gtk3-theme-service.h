#ifndef ZOITECHAT_GTK3_THEME_SERVICE_H
#define ZOITECHAT_GTK3_THEME_SERVICE_H

#include "zoitechat.h"

typedef enum
{
	ZOITECHAT_GTK3_THEME_SOURCE_SYSTEM = 0,
	ZOITECHAT_GTK3_THEME_SOURCE_USER = 1
} ZoitechatGtk3ThemeSource;

typedef struct
{
	char *id;
	char *display_name;
	char *path;
	gboolean has_dark_variant;
	char *thumbnail_path;
	ZoitechatGtk3ThemeSource source;
} ZoitechatGtk3Theme;

char *zoitechat_gtk3_theme_service_get_user_themes_dir (void);
GPtrArray *zoitechat_gtk3_theme_service_discover (void);
void zoitechat_gtk3_theme_free (ZoitechatGtk3Theme *theme);
ZoitechatGtk3Theme *zoitechat_gtk3_theme_find_by_id (const char *theme_id);
gboolean zoitechat_gtk3_theme_service_import (const char *source_path, char **imported_id, GError **error);
gboolean zoitechat_gtk3_theme_service_remove_user_theme (const char *theme_id, GError **error);
char *zoitechat_gtk3_theme_pick_css_dir_for_minor (const char *theme_root, int preferred_minor);
char *zoitechat_gtk3_theme_pick_css_dir (const char *theme_root);
GPtrArray *zoitechat_gtk3_theme_build_inheritance_chain (const char *theme_root);

#endif
