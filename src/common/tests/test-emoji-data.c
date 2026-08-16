/* ZoiteChat
 * Copyright (C) 2026 deepend-tildeclub.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <string.h>

#include <glib.h>

#include "../emoji-data.h"

#define GRINNING_FACE "\360\237\230\200"			/* 😀 */
#define DUCK "\360\237\246\206"						/* 🦆 */
#define THUMBS_UP "\360\237\221\215"				/* 👍 */
#define WAVE_DARK "\360\237\221\213\360\237\217\277"	/* 👋🏿 */
#define MEDIUM_TONE_MODIFIER "\360\237\217\275"	/* U+1F3FD */
#define KEYCAP_HASH "#\357\270\217\342\203\243"	/* #️⃣ */

static const EmojiEntry *
find_by_name (const char *name)
{
	gsize i;

	for (i = 0; i < emoji_data_count (); i++)
	{
		const EmojiEntry *entry = emoji_data_entry (i);

		if (!strcmp (entry->name, name))
			return entry;
	}

	return NULL;
}

static void
test_catalog_integrity (void)
{
	gsize i;

	g_assert_cmpuint (emoji_data_count (), >=, 1800);

	for (i = 0; i < emoji_data_count (); i++)
	{
		const EmojiEntry *entry = emoji_data_entry (i);

		g_assert_nonnull (entry->sequence);
		g_assert_true (*entry->sequence != '\0');
		g_assert_true (g_utf8_validate (entry->sequence, -1, NULL));
		g_assert_nonnull (entry->name);
		g_assert_true (*entry->name != '\0');
		g_assert_true (g_utf8_validate (entry->name, -1, NULL));
		g_assert_nonnull (entry->search);
		g_assert_true (*entry->search != '\0');
		g_assert_nonnull (entry->aliases);
		g_assert_cmpuint (entry->group, <, EMOJI_GROUP_COUNT);
		g_assert_cmpint (entry->tone_set, >=, -1);
	}
}

static void
test_cldr_order (void)
{
	const EmojiEntry *first = emoji_data_entry (0);

	g_assert_cmpstr (first->sequence, ==, GRINNING_FACE);
	g_assert_cmpuint (first->group, ==, EMOJI_GROUP_SMILEYS_EMOTION);
}

static void
test_groups (void)
{
	int group;
	gboolean seen[EMOJI_GROUP_COUNT] = { FALSE };
	gsize i;

	for (group = 0; group < EMOJI_GROUP_COUNT; group++)
	{
		g_assert_true (*emoji_data_group_display_name (group) != '\0');
		g_assert_true (*emoji_data_group_icon_sequence (group) != '\0');
	}

	for (i = 0; i < emoji_data_count (); i++)
		seen[emoji_data_entry (i)->group] = TRUE;
	for (group = 0; group < EMOJI_GROUP_COUNT; group++)
		g_assert_true (seen[group]);
}

static void
test_search_by_keyword (void)
{
	const EmojiEntry *duck = find_by_name ("duck");
	char **tokens = emoji_search_tokenize ("Duck");

	g_assert_nonnull (duck);
	g_assert_cmpstr (duck->sequence, ==, DUCK);
	g_assert_nonnull (tokens);
	g_assert_true (emoji_entry_matches (duck, tokens));
	g_strfreev (tokens);

	tokens = emoji_search_tokenize ("bird");
	g_assert_true (emoji_entry_matches (duck, tokens));
	g_strfreev (tokens);

	tokens = emoji_search_tokenize ("no-such-emoji-keyword");
	g_assert_false (emoji_entry_matches (duck, tokens));
	g_strfreev (tokens);
}

static void
test_search_by_alias (void)
{
	const EmojiEntry *thumbs = find_by_name ("thumbs up");
	char **tokens;

	g_assert_nonnull (thumbs);
	g_assert_nonnull (strstr (thumbs->aliases, "thumbsup"));

	tokens = emoji_search_tokenize (":thumbsup:");
	g_assert_nonnull (tokens);
	g_assert_true (emoji_entry_matches (thumbs, tokens));
	g_strfreev (tokens);
}

static void
test_multi_token_search (void)
{
	const EmojiEntry *flag = find_by_name ("flag: Canada");
	char **tokens = emoji_search_tokenize ("flag canada");

	g_assert_nonnull (flag);
	g_assert_cmpuint (flag->group, ==, EMOJI_GROUP_FLAGS);
	g_assert_nonnull (tokens);
	g_assert_true (emoji_entry_matches (flag, tokens));
	g_strfreev (tokens);
}

static void
test_skin_tones (void)
{
	const EmojiEntry *thumbs = find_by_name ("thumbs up");
	const EmojiEntry *duck = find_by_name ("duck");
	const char *toned;

	g_assert_nonnull (thumbs);
	g_assert_cmpint (thumbs->tone_set, >=, 0);

	toned = emoji_entry_sequence_for_tone (thumbs, EMOJI_TONE_MEDIUM);
	g_assert_cmpstr (toned, !=, thumbs->sequence);
	g_assert_nonnull (strstr (toned, MEDIUM_TONE_MODIFIER));
	g_assert_cmpstr (emoji_entry_sequence_for_tone (thumbs, EMOJI_TONE_NONE),
						  ==, thumbs->sequence);

	g_assert_nonnull (duck);
	g_assert_cmpint (duck->tone_set, ==, -1);
	g_assert_cmpstr (emoji_entry_sequence_for_tone (duck, EMOJI_TONE_DARK),
						  ==, duck->sequence);
}

static void
test_find_by_sequence (void)
{
	EmojiTone tone;
	const EmojiEntry *entry;

	entry = emoji_data_find_by_sequence (GRINNING_FACE, &tone);
	g_assert_nonnull (entry);
	g_assert_cmpstr (entry->name, ==, "grinning face");
	g_assert_cmpuint (tone, ==, EMOJI_TONE_NONE);

	entry = emoji_data_find_by_sequence (WAVE_DARK, &tone);
	g_assert_nonnull (entry);
	g_assert_cmpstr (entry->name, ==, "waving hand");
	g_assert_cmpuint (tone, ==, EMOJI_TONE_DARK);

	g_assert_null (emoji_data_find_by_sequence ("not an emoji", &tone));
	g_assert_null (emoji_data_find_by_sequence ("", NULL));
	g_assert_null (emoji_data_find_by_sequence (NULL, NULL));
}

static void
test_grapheme_sequences_intact (void)
{
	const EmojiEntry *keycap = find_by_name ("keycap: #");
	const EmojiEntry *family = find_by_name ("family: man, woman, boy");

	g_assert_nonnull (keycap);
	g_assert_cmpstr (keycap->sequence, ==, KEYCAP_HASH);

	g_assert_nonnull (family);
	g_assert_nonnull (strstr (family->sequence, "\342\200\215"));
}

static void
test_tokenize_edge_cases (void)
{
	g_assert_null (emoji_search_tokenize (NULL));
	g_assert_null (emoji_search_tokenize (""));
	g_assert_null (emoji_search_tokenize ("   "));
	g_assert_null (emoji_search_tokenize (":::"));
}

static void
test_versions (void)
{
	g_assert_true (*emoji_data_unicode_version () != '\0');
	g_assert_true (*emoji_data_cldr_version () != '\0');
}

int
main (int argc, char **argv)
{
	g_test_init (&argc, &argv, NULL);
	g_test_add_func ("/emoji-data/catalog-integrity", test_catalog_integrity);
	g_test_add_func ("/emoji-data/cldr-order", test_cldr_order);
	g_test_add_func ("/emoji-data/groups", test_groups);
	g_test_add_func ("/emoji-data/search-by-keyword", test_search_by_keyword);
	g_test_add_func ("/emoji-data/search-by-alias", test_search_by_alias);
	g_test_add_func ("/emoji-data/multi-token-search", test_multi_token_search);
	g_test_add_func ("/emoji-data/skin-tones", test_skin_tones);
	g_test_add_func ("/emoji-data/find-by-sequence", test_find_by_sequence);
	g_test_add_func ("/emoji-data/grapheme-sequences-intact",
						  test_grapheme_sequences_intact);
	g_test_add_func ("/emoji-data/tokenize-edge-cases",
						  test_tokenize_edge_cases);
	g_test_add_func ("/emoji-data/versions", test_versions);
	return g_test_run ();
}
