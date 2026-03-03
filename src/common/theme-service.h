#ifndef ZOITECHAT_THEME_SERVICE_H
#define ZOITECHAT_THEME_SERVICE_H

#include <glib.h>

char *zoitechat_theme_service_get_themes_dir (void);
GStrv zoitechat_theme_service_discover_themes (void);
gboolean zoitechat_theme_service_apply (const char *theme_name, GError **error);
void zoitechat_theme_service_set_post_apply_callback (void (*callback) (void));
void zoitechat_theme_service_run_post_apply_callback (void);

#endif
