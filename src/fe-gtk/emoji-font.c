/* ZoiteChat -- shared, process-private emoji font rendering.
 * Distributed under GPL-2.0-or-later; see COPYING. */

#include "fe-gtk.h"
#include "emoji-font.h"

#include <fontconfig/fontconfig.h>
#include <pango/pangocairo.h>
#include <pango/pangofc-fontmap.h>
#ifdef G_OS_WIN32
#include <glib/gwin32.h>
#endif

static char *
emoji_font_path (void)
{
	const char *override = g_getenv ("ZOITECHAT_EMOJI_FONT");

	if (override && *override)
		return g_strdup (override);

#ifdef G_OS_WIN32
	{
		char *base = g_win32_get_package_installation_directory_of_module (NULL);
		char *path;

		if (!base)
			return NULL;
		path = g_build_filename (base, "share", "fonts", "zoitechat",
			"NotoColorEmoji.ttf", NULL);
		g_free (base);
		return path;
	}
#else
	return g_build_filename (ZOITECHAT_DATADIR, "fonts", "zoitechat",
		"NotoColorEmoji.ttf", NULL);
#endif
}

PangoFontMap *
emoji_font_get_map (void)
{
	static PangoFontMap *map;
	static gboolean attempted;
	FcConfig *config;
	char *path, *escaped, *rules;
	gboolean registered;

	/* GTK widgets are used on the main thread. Keep the map alive for every
	 * layout's lifetime, including detached chat windows and shutdown. */
	if (attempted)
		return map;
	attempted = TRUE;

	path = emoji_font_path ();
	if (!path || !g_file_test (path, G_FILE_TEST_IS_REGULAR))
	{
		g_warning ("Bundled emoji font is missing: %s", path ? path : "(unknown path)");
		g_free (path);
		return NULL;
	}

	/* Keep the system configuration for normal fonts and user preferences.
	 * Give ONLY our application font a private family so an older system
	 * Noto cannot win the match. Never install fonts globally or override
	 * digits, '#' or '*' in the widget's ordinary text font. */
	config = FcInitLoadConfigAndFonts ();
	if (!config)
	{
		g_warning ("Unable to load font configuration for emoji rendering");
		g_free (path);
		return NULL;
	}
	escaped = g_markup_escape_text (path, -1);
	rules = g_strdup_printf (
		"<fontconfig>"
		"<match target='scan'><test name='file'><string>%s</string></test>"
		"<edit name='family' mode='assign'><string>" ZOITECHAT_EMOJI_FAMILY
		"</string></edit></match>"
		"<match target='pattern'><test name='family'><string>emoji</string></test>"
		"<edit name='family' mode='prepend' binding='strong'><string>"
		ZOITECHAT_EMOJI_FAMILY "</string></edit></match>"
		"</fontconfig>", escaped);
	registered = FcConfigParseAndLoadFromMemory (config, (const FcChar8 *) rules, FcTrue) &&
		FcConfigAppFontAddFile (config, (const FcChar8 *) path);
	g_free (rules);
	g_free (escaped);
	g_free (path);

	if (registered)
	{
		/* GDI cannot load Noto's CBDT/CBLC color glyphs. FreeType works on
		 * Windows 7 too, and is shipped in all of our GTK dependency bundles. */
		map = pango_cairo_font_map_new_for_font_type (CAIRO_FONT_TYPE_FT);
		if (map && PANGO_IS_FC_FONT_MAP (map))
			pango_fc_font_map_set_config (PANGO_FC_FONT_MAP (map), config);
		else
			g_clear_object (&map);
	}
	FcConfigDestroy (config);
	if (!map)
		g_warning ("Unable to initialize bundled emoji rendering; using system fonts");
	return map;
}

void
emoji_font_apply (GtkWidget *widget)
{
	PangoFontMap *map;

	g_return_if_fail (GTK_IS_WIDGET (widget));
	map = emoji_font_get_map ();
	if (map && gtk_widget_get_font_map (widget) != map)
		gtk_widget_set_font_map (widget, map);
}
