/* Shared headless font coverage and ordinary text regression tests.
 * Distributed under GPL-2.0-or-later; see COPYING. */
#include "../emoji-font.h"
#include "../../common/emoji-data.h"
#include <pango/pangofc-fontmap.h>
#include <pango/pangofc-font.h>

static GHashTable *font_checksums;
static char *bundled_checksum;

static const char *
font_checksum (const char *path)
{
	const char *cached = g_hash_table_lookup (font_checksums, path);
	char *contents, *checksum;
	gsize length;

	if (cached)
		return cached;
	g_assert_true (g_file_get_contents (path, &contents, &length, NULL));
	checksum = g_compute_checksum_for_data (G_CHECKSUM_SHA256,
		(const guchar *) contents, length);
	g_free (contents);
	g_hash_table_insert (font_checksums, g_strdup (path), checksum);
	return checksum;
}

static void
assert_bundled_font (PangoFont *font, gboolean expected)
{
	FcPattern *pattern;
	FcChar8 *path;
	const char *checksum;

	/* Pango can describe a face by its embedded name rather than its
	 * Fontconfig alias. Verify the resolved file's bytes, not that name.
	 * Cache by filename so catalog coverage does not reread the font for
	 * every glyph. An older system Noto must still fail this assertion. */
	g_assert_true (PANGO_IS_FC_FONT (font));
#if PANGO_VERSION_CHECK(1, 48, 0)
	pattern = pango_fc_font_get_pattern (PANGO_FC_FONT (font));
#else
	pattern = PANGO_FC_FONT (font)->font_pattern;
#endif
	g_assert_cmpint (FcPatternGetString (pattern, FC_FILE, 0, &path), ==, FcResultMatch);
	checksum = font_checksum ((const char *) path);
	if (expected)
		g_assert_cmpstr (checksum, ==, bundled_checksum);
	else
		g_assert_cmpstr (checksum, !=, bundled_checksum);
}

static void
test_font_catalog (void)
{
	PangoFontMap *map = emoji_font_get_map ();
	PangoContext *context;
	PangoLayout *layout;
	PangoFontDescription *desc;
	gsize i;
	int tone;

	g_assert_nonnull (map);
	context = pango_font_map_create_context (map);
	g_assert_true (PANGO_IS_FC_FONT_MAP (map));
	layout = pango_layout_new (context);
	desc = pango_font_description_from_string (ZOITECHAT_EMOJI_FAMILY " 15");
	pango_layout_set_font_description (layout, desc);
	for (i = 0; i < emoji_data_count (); i++)
	{
		const EmojiEntry *entry = emoji_data_entry (i);
		for (tone = 0; tone < (entry->tone_set < 0 ? 1 : EMOJI_TONE_COUNT); tone++)
		{
			PangoLayoutIter *iter;
			pango_layout_set_text (layout, emoji_entry_sequence_for_tone (entry, tone), -1);
			g_assert_cmpint (pango_layout_get_unknown_glyphs_count (layout), ==, 0);
			iter = pango_layout_get_iter (layout);
			do
			{
				PangoLayoutRun *run = pango_layout_iter_get_run_readonly (iter);
				if (run)
				{
					assert_bundled_font (run->item->analysis.font, TRUE);
				}
			} while (pango_layout_iter_next_run (iter));
			pango_layout_iter_free (iter);
		}
	}
	pango_font_description_free (desc);
	g_object_unref (layout);
	g_object_unref (context);
}

static void
test_plain_text_font (void)
{
	PangoFontMap *map = emoji_font_get_map ();
	PangoContext *context;
	PangoLayout *layout;
	PangoLayoutIter *iter;
	PangoFontDescription *desc;

	g_assert_nonnull (map);
	context = pango_font_map_create_context (map);
	layout = pango_layout_new (context);
	pango_layout_set_text (layout, "/join #channel 0123456789 *", -1);
	desc = pango_font_description_from_string ("Monospace 12");
	pango_layout_set_font_description (layout, desc);
	iter = pango_layout_get_iter (layout);
	do
	{
		PangoLayoutRun *run = pango_layout_iter_get_run_readonly (iter);
		if (run)
		{
			assert_bundled_font (run->item->analysis.font, FALSE);
		}
	} while (pango_layout_iter_next_run (iter));
	pango_layout_iter_free (iter);
	pango_font_description_free (desc);
	g_object_unref (layout);
	g_object_unref (context);
}

static void
test_implicit_emoji_font (void)
{
	PangoFontMap *map = emoji_font_get_map ();
	PangoContext *context = pango_font_map_create_context (map);
	PangoLayout *layout = pango_layout_new (context);
	PangoFontDescription *desc = pango_font_description_from_string ("Monospace 12");
	const char *samples[] = { "\360\237\230\200", "#\357\270\217\342\203\243", "\360\237\253\252" };
	gsize i;

	pango_layout_set_font_description (layout, desc);
	for (i = 0; i < G_N_ELEMENTS (samples); i++)
	{
		PangoLayoutIter *iter;
		pango_layout_set_text (layout, samples[i], -1);
		g_assert_cmpint (pango_layout_get_unknown_glyphs_count (layout), ==, 0);
		iter = pango_layout_get_iter (layout);
		do
		{
			PangoLayoutRun *run = pango_layout_iter_get_run_readonly (iter);
			if (run)
			{
				assert_bundled_font (run->item->analysis.font, TRUE);
			}
		} while (pango_layout_iter_next_run (iter));
		pango_layout_iter_free (iter);
	}
	pango_font_description_free (desc);
	g_object_unref (layout);
	g_object_unref (context);
}

int
main (int argc, char **argv)
{
	const char *path;
	int result;

	g_test_init (&argc, &argv, NULL);
	path = g_getenv ("ZOITECHAT_EMOJI_FONT");
	g_assert_nonnull (path);
	font_checksums = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	bundled_checksum = g_strdup (font_checksum (path));
	g_test_add_func ("/emoji-font/catalog", test_font_catalog);
	g_test_add_func ("/emoji-font/plain-text", test_plain_text_font);
	g_test_add_func ("/emoji-font/implicit-emoji", test_implicit_emoji_font);
	result = g_test_run ();
	g_free (bundled_checksum);
	g_hash_table_destroy (font_checksums);
	return result;
}
