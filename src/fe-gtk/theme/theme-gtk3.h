#ifndef ZOITECHAT_THEME_GTK3_H
#define ZOITECHAT_THEME_GTK3_H

#include <glib.h>

typedef enum
{
	THEME_GTK3_VARIANT_FOLLOW_SYSTEM = 0,
	THEME_GTK3_VARIANT_PREFER_LIGHT = 1,
	THEME_GTK3_VARIANT_PREFER_DARK = 2
} ThemeGtk3Variant;

void theme_gtk3_init (void);
gboolean theme_gtk3_apply_current (GError **error);
gboolean theme_gtk3_apply (const char *theme_id, ThemeGtk3Variant variant, GError **error);
gboolean theme_gtk3_refresh (const char *theme_id, ThemeGtk3Variant variant, GError **error);
ThemeGtk3Variant theme_gtk3_variant_for_theme (const char *theme_id);
void theme_gtk3_invalidate_provider_cache (void);
void theme_gtk3_disable (void);
gboolean theme_gtk3_is_active (void);

#endif
