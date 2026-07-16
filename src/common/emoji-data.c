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
#include <glib/gi18n.h>

#include "emoji-data.h"

#include "emoji-data-table.h"

gsize
emoji_data_count (void)
{
	return G_N_ELEMENTS (emoji_entries_table);
}

const EmojiEntry *
emoji_data_entry (gsize index)
{
	g_return_val_if_fail (index < G_N_ELEMENTS (emoji_entries_table), NULL);

	return &emoji_entries_table[index];
}

static GHashTable *
emoji_sequence_index (void)
{
	static GHashTable *table = NULL;
	gsize i;
	int tone;

	if (table)
		return table;

	table = g_hash_table_new (g_str_hash, g_str_equal);
	for (i = 0; i < G_N_ELEMENTS (emoji_entries_table); i++)
	{
		const EmojiEntry *entry = &emoji_entries_table[i];

		g_hash_table_insert (table, (gpointer) entry->sequence,
									GSIZE_TO_POINTER (i * EMOJI_TONE_COUNT + 1));
		if (entry->tone_set < 0)
			continue;
		for (tone = EMOJI_TONE_LIGHT; tone < EMOJI_TONE_COUNT; tone++)
		{
			const char *seq =
				emoji_tone_sets_table[entry->tone_set].sequence[tone - 1];

			g_hash_table_insert (table, (gpointer) seq,
										GSIZE_TO_POINTER (i * EMOJI_TONE_COUNT + tone + 1));
		}
	}

	return table;
}

const EmojiEntry *
emoji_data_find_by_sequence (const char *sequence, EmojiTone *tone_out)
{
	gpointer value;
	gsize packed;

	if (tone_out)
		*tone_out = EMOJI_TONE_NONE;
	if (!sequence || !*sequence)
		return NULL;

	value = g_hash_table_lookup (emoji_sequence_index (), sequence);
	if (!value)
		return NULL;

	packed = GPOINTER_TO_SIZE (value) - 1;
	if (tone_out)
		*tone_out = (EmojiTone) (packed % EMOJI_TONE_COUNT);

	return &emoji_entries_table[packed / EMOJI_TONE_COUNT];
}

const char *
emoji_data_group_display_name (EmojiGroup group)
{
	static const char *const names[EMOJI_GROUP_COUNT] =
	{
		N_("Smileys & Emotion"),
		N_("People & Body"),
		N_("Animals & Nature"),
		N_("Food & Drink"),
		N_("Travel & Places"),
		N_("Activities"),
		N_("Objects"),
		N_("Symbols"),
		N_("Flags"),
	};

	g_return_val_if_fail (group < EMOJI_GROUP_COUNT, "");

	return _(names[group]);
}

const char *
emoji_data_group_icon_sequence (EmojiGroup group)
{
	static const char *const icons[EMOJI_GROUP_COUNT] =
	{
		"\360\237\230\200",					/* 😀 */
		"\360\237\221\213",					/* 👋 */
		"\360\237\220\273",					/* 🐻 */
		"\360\237\215\224",					/* 🍔 */
		"\360\237\232\227",					/* 🚗 */
		"\342\232\275",						/* ⚽ */
		"\360\237\222\241",					/* 💡 */
		"\360\237\224\243",					/* 🔣 */
		"\360\237\217\201",					/* 🏁 */
	};

	g_return_val_if_fail (group < EMOJI_GROUP_COUNT, "");

	return icons[group];
}

const char *
emoji_data_tone_display_name (EmojiTone tone)
{
	static const char *const names[EMOJI_TONE_COUNT] =
	{
		N_("No skin tone"),
		N_("Light skin tone"),
		N_("Medium-light skin tone"),
		N_("Medium skin tone"),
		N_("Medium-dark skin tone"),
		N_("Dark skin tone"),
	};

	g_return_val_if_fail (tone < EMOJI_TONE_COUNT, "");

	return _(names[tone]);
}

const char *
emoji_entry_sequence_for_tone (const EmojiEntry *entry, EmojiTone tone)
{
	g_return_val_if_fail (entry != NULL, NULL);

	if (tone == EMOJI_TONE_NONE || tone >= EMOJI_TONE_COUNT ||
		 entry->tone_set < 0)
		return entry->sequence;

	return emoji_tone_sets_table[entry->tone_set].sequence[tone - 1];
}

char **
emoji_search_tokenize (const char *text)
{
	GPtrArray *tokens;
	char *folded;
	char **split;
	gsize i;

	if (!text)
		return NULL;

	folded = g_utf8_casefold (text, -1);
	split = g_strsplit_set (folded, " \t\n", -1);
	g_free (folded);

	tokens = g_ptr_array_new ();
	for (i = 0; split[i]; i++)
	{
		const char *token = split[i];
		gsize len;

		while (*token == ':')
			token++;
		len = strlen (token);
		while (len > 0 && token[len - 1] == ':')
			len--;
		if (len > 0)
			g_ptr_array_add (tokens, g_strndup (token, len));
	}
	g_strfreev (split);

	if (tokens->len == 0)
	{
		g_ptr_array_free (tokens, TRUE);
		return NULL;
	}

	g_ptr_array_add (tokens, NULL);

	return (char **) g_ptr_array_free (tokens, FALSE);
}

gboolean
emoji_entry_matches (const EmojiEntry *entry, char *const *needle_tokens)
{
	gsize i;

	g_return_val_if_fail (entry != NULL, FALSE);

	if (!needle_tokens || !needle_tokens[0])
		return FALSE;

	for (i = 0; needle_tokens[i]; i++)
	{
		if (!strstr (entry->search, needle_tokens[i]))
			return FALSE;
	}

	return TRUE;
}

const char *
emoji_data_unicode_version (void)
{
	return emoji_data_unicode_version_str;
}

const char *
emoji_data_cldr_version (void)
{
	return emoji_data_cldr_version_str;
}
