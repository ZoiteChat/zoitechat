/* Shared headless font coverage and ordinary text regression tests.
 * Distributed under GPL-2.0-or-later; see COPYING. */
#include "../emoji-font.h"
#include "../../common/emoji-data.h"
#include <pango/pangofc-fontmap.h>

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
					PangoFontDescription *actual = pango_font_describe (run->item->analysis.font);
					g_assert_cmpstr (pango_font_description_get_family (actual), ==, ZOITECHAT_EMOJI_FAMILY);
					pango_font_description_free (actual);
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
			PangoFontDescription *actual = pango_font_describe (run->item->analysis.font);
			g_assert_cmpstr (pango_font_description_get_family (actual), !=, ZOITECHAT_EMOJI_FAMILY);
			pango_font_description_free (actual);
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
				PangoFontDescription *actual = pango_font_describe (run->item->analysis.font);
				g_assert_cmpstr (pango_font_description_get_family (actual), ==, ZOITECHAT_EMOJI_FAMILY);
				pango_font_description_free (actual);
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
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/emoji-font/catalog", test_font_catalog);
	g_test_add_func ("/emoji-font/plain-text", test_plain_text_font);
	g_test_add_func ("/emoji-font/implicit-emoji", test_implicit_emoji_font);
	return g_test_run ();
}
