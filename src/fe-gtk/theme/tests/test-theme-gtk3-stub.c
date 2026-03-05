#include <glib.h>

#include "../theme-gtk3.h"

static int apply_current_calls;

void
test_theme_gtk3_stub_reset (void)
{
	apply_current_calls = 0;
}

int
test_theme_gtk3_stub_apply_current_calls (void)
{
	return apply_current_calls;
}

void
theme_gtk3_init (void)
{
}

gboolean
theme_gtk3_apply_current (GError **error)
{
	(void) error;
	apply_current_calls++;
	return TRUE;
}

gboolean
theme_gtk3_apply (const char *theme_id, ThemeGtk3Variant variant, GError **error)
{
	(void) theme_id;
	(void) variant;
	(void) error;
	return TRUE;
}

gboolean
theme_gtk3_refresh (const char *theme_id, ThemeGtk3Variant variant, GError **error)
{
	(void) theme_id;
	(void) variant;
	(void) error;
	return TRUE;
}

ThemeGtk3Variant
theme_gtk3_variant_for_theme (const char *theme_id)
{
	(void) theme_id;
	return THEME_GTK3_VARIANT_PREFER_LIGHT;
}

void
theme_gtk3_invalidate_provider_cache (void)
{
}

void
theme_gtk3_disable (void)
{
}

gboolean
theme_gtk3_is_active (void)
{
	return FALSE;
}
