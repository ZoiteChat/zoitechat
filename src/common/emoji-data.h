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

/* Bundled emoji catalog, generated at build time from pinned Unicode,
 * CLDR and gemoji data (see data/emoji/ and tools/gen-emoji-data.py).
 * Every entry is a fully-qualified Unicode sequence; nothing here depends
 * on the GTK runtime's emoji data.  The catalog only decides what users
 * can find and insert - rendering stays with the platform's fonts, and
 * IRC always receives the plain Unicode sequence. */

#ifndef ZOITECHAT_EMOJI_DATA_H
#define ZOITECHAT_EMOJI_DATA_H

#include <glib.h>

typedef enum
{
	EMOJI_GROUP_SMILEYS_EMOTION,
	EMOJI_GROUP_PEOPLE_BODY,
	EMOJI_GROUP_ANIMALS_NATURE,
	EMOJI_GROUP_FOOD_DRINK,
	EMOJI_GROUP_TRAVEL_PLACES,
	EMOJI_GROUP_ACTIVITIES,
	EMOJI_GROUP_OBJECTS,
	EMOJI_GROUP_SYMBOLS,
	EMOJI_GROUP_FLAGS,
	EMOJI_GROUP_COUNT
} EmojiGroup;

typedef enum
{
	EMOJI_TONE_NONE,
	EMOJI_TONE_LIGHT,
	EMOJI_TONE_MEDIUM_LIGHT,
	EMOJI_TONE_MEDIUM,
	EMOJI_TONE_MEDIUM_DARK,
	EMOJI_TONE_DARK,
	EMOJI_TONE_COUNT
} EmojiTone;

typedef struct
{
	const char *sequence;
	const char *name;
	const char *search;
	const char *aliases;
	guint8 group;
	gint16 tone_set;
} EmojiEntry;

typedef struct
{
	const char *sequence[EMOJI_TONE_COUNT - 1];
} EmojiToneSet;

gsize emoji_data_count (void);
const EmojiEntry *emoji_data_entry (gsize index);

const EmojiEntry *emoji_data_find_by_sequence (const char *sequence,
															  EmojiTone *tone_out);

const char *emoji_data_group_display_name (EmojiGroup group);
const char *emoji_data_group_icon_sequence (EmojiGroup group);
const char *emoji_data_tone_display_name (EmojiTone tone);

const char *emoji_entry_sequence_for_tone (const EmojiEntry *entry,
														 EmojiTone tone);

char **emoji_search_tokenize (const char *text);
gboolean emoji_entry_matches (const EmojiEntry *entry,
										char *const *needle_tokens);

const char *emoji_data_unicode_version (void);
const char *emoji_data_cldr_version (void);

#endif
