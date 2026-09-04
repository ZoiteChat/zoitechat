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

/* ZoiteChat's own emoji picker.  Replaces GTK's built-in "show-emoji-icon"
 * entry machinery so that every build offers the same catalog (bundled at
 * compile time from pinned Unicode/CLDR data, see src/common/emoji-data.h)
 * regardless of the GTK runtime version or distribution emoji data.
 * Selecting an emoji inserts the plain Unicode sequence at the cursor -
 * nothing about what is sent over IRC changes. */

#include <string.h>

#include "fe-gtk.h"

#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/emoji-data.h"

#include "emoji-picker.h"
#include "preferences-persistence.h"

#define EMOJI_CATEGORY_RECENT -1
#define EMOJI_RECENTS_MAX 32
#define EMOJI_SEARCH_MAX_RESULTS 256
#define EMOJI_GRID_COLUMNS 9

typedef struct
{
	GtkWidget *button;
	GtkWidget *target;						/* weak pointer to the input entry */
	GtkWidget *popover;
	GtkWidget *search_entry;
	GtkWidget *stack;
	GtkWidget *grid;							/* GtkFlowBox */
	GtkWidget *scroller;
	GtkWidget *empty_label;
	GtkWidget *category_buttons[EMOJI_GROUP_COUNT + 1];	/* [0] = recents */
	GtkWidget *tone_buttons[EMOJI_TONE_COUNT];
	int category;								/* EMOJI_CATEGORY_RECENT or EmojiGroup */
	gboolean populated;
	gboolean updating_buttons;
} EmojiPicker;

/* ------------------------------------------------------------------------ *
 * Recently used emoji, shared by all windows and persisted across runs
 * ------------------------------------------------------------------------ */

static GPtrArray *recent_emoji = NULL;

static char *
emoji_recents_filename (void)
{
	return g_build_filename (get_xdir (), "emoji-recents.conf", NULL);
}

static void
emoji_recents_load (void)
{
	char *filename, *contents = NULL;
	char **lines;
	gsize i;

	if (recent_emoji)
		return;

	recent_emoji = g_ptr_array_new_with_free_func (g_free);

	filename = emoji_recents_filename ();
	if (g_file_get_contents (filename, &contents, NULL, NULL))
	{
		lines = g_strsplit (contents, "\n", -1);
		for (i = 0; lines[i] && recent_emoji->len < EMOJI_RECENTS_MAX; i++)
		{
			g_strstrip (lines[i]);
			/* only keep sequences that exist in the bundled catalog */
			if (*lines[i] && emoji_data_find_by_sequence (lines[i], NULL))
				g_ptr_array_add (recent_emoji, g_strdup (lines[i]));
		}
		g_strfreev (lines);
		g_free (contents);
	}
	g_free (filename);
}

static void
emoji_recents_save (void)
{
	GString *contents = g_string_new (NULL);
	char *filename;
	gsize i;

	for (i = 0; i < recent_emoji->len; i++)
	{
		g_string_append (contents, g_ptr_array_index (recent_emoji, i));
		g_string_append_c (contents, '\n');
	}

	filename = emoji_recents_filename ();
	g_file_set_contents (filename, contents->str, contents->len, NULL);
	g_free (filename);
	g_string_free (contents, TRUE);
}

static void
emoji_recents_add (const char *sequence)
{
	GPtrArray *updated;
	gsize i;

	emoji_recents_load ();

	updated = g_ptr_array_new_with_free_func (g_free);
	g_ptr_array_add (updated, g_strdup (sequence));
	for (i = 0; i < recent_emoji->len && updated->len < EMOJI_RECENTS_MAX; i++)
	{
		const char *old = g_ptr_array_index (recent_emoji, i);

		if (strcmp (old, sequence) != 0)
			g_ptr_array_add (updated, g_strdup (old));
	}
	g_ptr_array_free (recent_emoji, TRUE);
	recent_emoji = updated;

	emoji_recents_save ();
}

/* ------------------------------------------------------------------------ *
 * Appearance helpers
 * ------------------------------------------------------------------------ */

/* One application-wide provider so every emoji cell prefers a color emoji
 * font without changing the fonts of surrounding widgets.  Emoji families
 * that are not installed are simply skipped by fontconfig/Pango. */
static void
emoji_picker_ensure_css (void)
{
	static gboolean done = FALSE;
	GtkCssProvider *provider;

	if (done)
		return;
	done = TRUE;

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider,
		".zc-emoji-cell {"
		"  font-family: \"Noto Color Emoji\", \"Segoe UI Emoji\","
		"    \"Apple Color Emoji\", \"Twemoji Mozilla\", \"EmojiOne Color\";"
		"  font-size: 15pt;"
		"  padding: 2px;"
		"}"
		".zc-emoji-cell-small {"
		"  font-family: \"Noto Color Emoji\", \"Segoe UI Emoji\","
		"    \"Apple Color Emoji\", \"Twemoji Mozilla\", \"EmojiOne Color\";"
		"}",
		-1, NULL);
	gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
		GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_object_unref (provider);
}

static GtkWidget *
emoji_label_new (const char *sequence, gboolean small_cell)
{
	GtkWidget *label = gtk_label_new (sequence);

	gtk_style_context_add_class (gtk_widget_get_style_context (label),
										  small_cell ? "zc-emoji-cell-small" : "zc-emoji-cell");

	return label;
}

static void
emoji_picker_set_a11y_name (GtkWidget *widget, const char *name)
{
	AtkObject *accessible = gtk_widget_get_accessible (widget);

	if (accessible)
		atk_object_set_name (accessible, name);
}

static EmojiTone
emoji_picker_current_tone (void)
{
	int tone = prefs.hex_gui_emoji_skin_tone;

	if (tone < EMOJI_TONE_NONE || tone >= EMOJI_TONE_COUNT)
		return EMOJI_TONE_NONE;

	return (EmojiTone) tone;
}

/* "waving hand" / "waving hand (Dark skin tone)", with :shortcodes: on a
 * second line - used for both tooltips and accessible names. */
static char *
emoji_picker_describe (const EmojiEntry *entry, EmojiTone tone)
{
	GString *desc = g_string_new (NULL);
	char **aliases;
	gsize i;

	if (tone != EMOJI_TONE_NONE && entry->tone_set >= 0)
		g_string_append_printf (desc, "%s (%s)", entry->name,
										emoji_data_tone_display_name (tone));
	else
		g_string_append (desc, entry->name);

	if (*entry->aliases)
	{
		g_string_append_c (desc, '\n');
		aliases = g_strsplit (entry->aliases, " ", -1);
		for (i = 0; aliases[i]; i++)
		{
			if (i > 0)
				g_string_append_c (desc, ' ');
			g_string_append_printf (desc, ":%s:", aliases[i]);
		}
		g_strfreev (aliases);
	}

	return g_string_free (desc, FALSE);
}

/* ------------------------------------------------------------------------ *
 * Insertion
 * ------------------------------------------------------------------------ */

static void
emoji_picker_insert (EmojiPicker *picker, const char *sequence)
{
	GtkEditable *editable;
	int pos;

	if (!picker->target || !sequence || !*sequence)
		return;

	gtk_popover_popdown (GTK_POPOVER (picker->popover));

	editable = GTK_EDITABLE (picker->target);
	gtk_editable_delete_selection (editable);
	pos = gtk_editable_get_position (editable);
	gtk_editable_insert_text (editable, sequence, strlen (sequence), &pos);

	/* focus the entry without letting GtkEntry select everything */
	gtk_editable_set_editable (editable, FALSE);
	gtk_widget_grab_focus (picker->target);
	gtk_editable_set_editable (editable, TRUE);
	gtk_editable_set_position (editable, pos);

	emoji_recents_add (sequence);
}

static void
emoji_picker_activate_child (EmojiPicker *picker, GtkFlowBoxChild *child)
{
	emoji_picker_insert (picker,
		g_object_get_data (G_OBJECT (child), "emoji-sequence"));
}

static void
emoji_grid_child_activated_cb (GtkFlowBox *grid, GtkFlowBoxChild *child,
										 gpointer userdata)
{
	emoji_picker_activate_child (userdata, child);
}

/* ------------------------------------------------------------------------ *
 * Grid contents
 * ------------------------------------------------------------------------ */

static void
emoji_grid_add (EmojiPicker *picker, const EmojiEntry *entry,
					 const char *sequence, EmojiTone tone)
{
	GtkWidget *child, *label;
	char *desc;

	label = emoji_label_new (sequence, FALSE);
	child = gtk_flow_box_child_new ();
	gtk_container_add (GTK_CONTAINER (child), label);
	g_object_set_data_full (G_OBJECT (child), "emoji-sequence",
									g_strdup (sequence), g_free);

	desc = emoji_picker_describe (entry, tone);
	gtk_widget_set_tooltip_text (child, desc);
	emoji_picker_set_a11y_name (child, desc);
	g_free (desc);

	gtk_container_add (GTK_CONTAINER (picker->grid), child);
}

static void
emoji_picker_refresh (EmojiPicker *picker)
{
	const char *text = gtk_entry_get_text (GTK_ENTRY (picker->search_entry));
	EmojiTone tone = emoji_picker_current_tone ();
	GtkAdjustment *vadj;
	gboolean any = FALSE;
	gsize i, results = 0;

	gtk_container_foreach (GTK_CONTAINER (picker->grid),
								  (GtkCallback) gtk_widget_destroy, NULL);

	if (text && *text)
	{
		char **tokens = emoji_search_tokenize (text);

		if (tokens)
		{
			for (i = 0; i < emoji_data_count () &&
							results < EMOJI_SEARCH_MAX_RESULTS; i++)
			{
				const EmojiEntry *entry = emoji_data_entry (i);

				if (!emoji_entry_matches (entry, tokens))
					continue;
				emoji_grid_add (picker, entry,
									 emoji_entry_sequence_for_tone (entry, tone), tone);
				results++;
			}
			g_strfreev (tokens);
			any = results > 0;
		}
		gtk_label_set_text (GTK_LABEL (picker->empty_label),
								  _("No emoji found"));
	}
	else if (picker->category == EMOJI_CATEGORY_RECENT)
	{
		emoji_recents_load ();
		for (i = 0; i < recent_emoji->len; i++)
		{
			const char *sequence = g_ptr_array_index (recent_emoji, i);
			EmojiTone seq_tone;
			const EmojiEntry *entry =
				emoji_data_find_by_sequence (sequence, &seq_tone);

			if (!entry)
				continue;
			emoji_grid_add (picker, entry, sequence, seq_tone);
			any = TRUE;
		}
		gtk_label_set_text (GTK_LABEL (picker->empty_label),
								  _("No recently used emoji"));
	}
	else
	{
		for (i = 0; i < emoji_data_count (); i++)
		{
			const EmojiEntry *entry = emoji_data_entry (i);

			if (entry->group != picker->category)
				continue;
			emoji_grid_add (picker, entry,
								 emoji_entry_sequence_for_tone (entry, tone), tone);
			any = TRUE;
		}
	}

	gtk_widget_show_all (picker->grid);
	gtk_stack_set_visible_child_name (GTK_STACK (picker->stack),
												 any ? "grid" : "empty");

	vadj = gtk_scrolled_window_get_vadjustment (
		GTK_SCROLLED_WINDOW (picker->scroller));
	gtk_adjustment_set_value (vadj, gtk_adjustment_get_lower (vadj));
}

/* ------------------------------------------------------------------------ *
 * Categories
 * ------------------------------------------------------------------------ */

static void
emoji_picker_sync_category_buttons (EmojiPicker *picker)
{
	gsize i;

	picker->updating_buttons = TRUE;
	for (i = 0; i < G_N_ELEMENTS (picker->category_buttons); i++)
	{
		int category = (int) i - 1;

		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (picker->category_buttons[i]),
			category == picker->category);
	}
	picker->updating_buttons = FALSE;
}

static void
emoji_category_toggled_cb (GtkToggleButton *button, gpointer userdata)
{
	EmojiPicker *picker = userdata;
	int category = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
																		"emoji-category"));

	if (picker->updating_buttons)
		return;

	if (!gtk_toggle_button_get_active (button))
	{
		/* the active category cannot be untoggled */
		if (picker->category == category)
			emoji_picker_sync_category_buttons (picker);
		return;
	}

	picker->category = category;
	emoji_picker_sync_category_buttons (picker);

	/* leave search mode; clearing the entry triggers the refresh */
	if (*gtk_entry_get_text (GTK_ENTRY (picker->search_entry)))
		gtk_entry_set_text (GTK_ENTRY (picker->search_entry), "");
	else
		emoji_picker_refresh (picker);
}

static GtkWidget *
emoji_category_button_new (EmojiPicker *picker, int category,
									const char *icon_sequence, const char *name)
{
	GtkWidget *button = gtk_toggle_button_new ();

	gtk_container_add (GTK_CONTAINER (button),
							 emoji_label_new (icon_sequence, TRUE));
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_tooltip_text (button, name);
	emoji_picker_set_a11y_name (button, name);
	g_object_set_data (G_OBJECT (button), "emoji-category",
							 GINT_TO_POINTER (category));
	g_signal_connect (G_OBJECT (button), "toggled",
							G_CALLBACK (emoji_category_toggled_cb), picker);

	return button;
}

/* ------------------------------------------------------------------------ *
 * Skin tones
 * ------------------------------------------------------------------------ */

#define EMOJI_TONE_SAMPLE "\342\234\213"		/* ✋ raised hand */

static void
emoji_tone_toggled_cb (GtkToggleButton *button, gpointer userdata)
{
	EmojiPicker *picker = userdata;
	int tone = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button),
																  "emoji-tone"));

	if (!gtk_toggle_button_get_active (button))
		return;
	if (prefs.hex_gui_emoji_skin_tone == tone)
		return;

	prefs.hex_gui_emoji_skin_tone = tone;
	preferences_persistence_save_all ();
	emoji_picker_refresh (picker);
}

static GtkWidget *
emoji_tone_row_new (EmojiPicker *picker)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	GtkWidget *button;
	GSList *group = NULL;
	const EmojiEntry *sample = emoji_data_find_by_sequence (EMOJI_TONE_SAMPLE,
																			  NULL);
	int tone;

	gtk_widget_set_halign (box, GTK_ALIGN_CENTER);

	for (tone = EMOJI_TONE_NONE; tone < EMOJI_TONE_COUNT; tone++)
	{
		const char *sequence = EMOJI_TONE_SAMPLE;

		if (sample)
			sequence = emoji_entry_sequence_for_tone (sample, tone);

		button = gtk_radio_button_new (group);
		group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (button));
		gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (button), FALSE);
		gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
		gtk_container_add (GTK_CONTAINER (button),
								 emoji_label_new (sequence, TRUE));
		gtk_widget_set_tooltip_text (button,
											  emoji_data_tone_display_name (tone));
		emoji_picker_set_a11y_name (button,
											 emoji_data_tone_display_name (tone));
		g_object_set_data (G_OBJECT (button), "emoji-tone",
								 GINT_TO_POINTER (tone));
		picker->tone_buttons[tone] = button;

		if (emoji_picker_current_tone () == tone)
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);

		g_signal_connect (G_OBJECT (button), "toggled",
								G_CALLBACK (emoji_tone_toggled_cb), picker);

		gtk_box_pack_start (GTK_BOX (box), button, FALSE, FALSE, 0);
	}

	return box;
}

/* the pref can also change externally (/set gui_emoji_skin_tone) */
static void
emoji_picker_sync_tone_buttons (EmojiPicker *picker)
{
	EmojiTone tone = emoji_picker_current_tone ();

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (picker->tone_buttons[tone]), TRUE);
}

/* ------------------------------------------------------------------------ *
 * Search
 * ------------------------------------------------------------------------ */

static void
emoji_search_changed_cb (GtkSearchEntry *entry, gpointer userdata)
{
	emoji_picker_refresh (userdata);
}

static void
emoji_search_activate_cb (GtkEntry *entry, gpointer userdata)
{
	EmojiPicker *picker = userdata;
	GtkFlowBoxChild *child =
		gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (picker->grid), 0);

	/* Enter inserts the first hit */
	if (child)
		emoji_picker_activate_child (picker, child);
}

static gboolean
emoji_search_key_press_cb (GtkWidget *entry, GdkEventKey *event,
									gpointer userdata)
{
	EmojiPicker *picker = userdata;
	GtkFlowBoxChild *child;

	if (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_KP_Down)
	{
		child = gtk_flow_box_get_child_at_index (GTK_FLOW_BOX (picker->grid), 0);
		if (child)
		{
			gtk_widget_grab_focus (GTK_WIDGET (child));
			return TRUE;
		}
	}

	return FALSE;
}

/* ------------------------------------------------------------------------ *
 * Popover assembly
 * ------------------------------------------------------------------------ */

static void
emoji_picker_build_contents (EmojiPicker *picker)
{
	GtkWidget *vbox, *categories, *button;
	int group;

	emoji_picker_ensure_css ();

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_set (vbox, "margin", 6, NULL);

	picker->search_entry = gtk_search_entry_new ();
	gtk_entry_set_placeholder_text (GTK_ENTRY (picker->search_entry),
											  _("Search emoji"));
	emoji_picker_set_a11y_name (picker->search_entry, _("Search emoji"));
	g_signal_connect (G_OBJECT (picker->search_entry), "search-changed",
							G_CALLBACK (emoji_search_changed_cb), picker);
	g_signal_connect (G_OBJECT (picker->search_entry), "activate",
							G_CALLBACK (emoji_search_activate_cb), picker);
	g_signal_connect (G_OBJECT (picker->search_entry), "key-press-event",
							G_CALLBACK (emoji_search_key_press_cb), picker);
	gtk_box_pack_start (GTK_BOX (vbox), picker->search_entry, FALSE, FALSE, 0);

	categories = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_set_homogeneous (GTK_BOX (categories), TRUE);

	picker->category_buttons[0] = emoji_category_button_new (picker,
		EMOJI_CATEGORY_RECENT, "\360\237\225\230" /* 🕘 */,
		_("Recently Used"));
	gtk_box_pack_start (GTK_BOX (categories), picker->category_buttons[0],
							  TRUE, TRUE, 0);
	for (group = 0; group < EMOJI_GROUP_COUNT; group++)
	{
		button = emoji_category_button_new (picker, group,
			emoji_data_group_icon_sequence (group),
			emoji_data_group_display_name (group));
		picker->category_buttons[group + 1] = button;
		gtk_box_pack_start (GTK_BOX (categories), button, TRUE, TRUE, 0);
	}
	gtk_box_pack_start (GTK_BOX (vbox), categories, FALSE, FALSE, 0);

	picker->grid = gtk_flow_box_new ();
	gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (picker->grid),
												GTK_SELECTION_NONE);
	gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (picker->grid),
															 TRUE);
	gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (picker->grid),
														 EMOJI_GRID_COLUMNS);
	gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (picker->grid),
														 EMOJI_GRID_COLUMNS);
	gtk_flow_box_set_homogeneous (GTK_FLOW_BOX (picker->grid), TRUE);
	gtk_widget_set_valign (picker->grid, GTK_ALIGN_START);
	g_signal_connect (G_OBJECT (picker->grid), "child-activated",
							G_CALLBACK (emoji_grid_child_activated_cb), picker);

	picker->scroller = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (picker->scroller),
											  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_min_content_height (
		GTK_SCROLLED_WINDOW (picker->scroller), 300);
	gtk_container_add (GTK_CONTAINER (picker->scroller), picker->grid);

	picker->empty_label = gtk_label_new (_("No emoji found"));
	gtk_style_context_add_class (
		gtk_widget_get_style_context (picker->empty_label), "dim-label");

	picker->stack = gtk_stack_new ();
	gtk_widget_set_size_request (picker->stack, 380, -1);
	gtk_stack_add_named (GTK_STACK (picker->stack), picker->scroller, "grid");
	gtk_stack_add_named (GTK_STACK (picker->stack), picker->empty_label,
								"empty");
	gtk_box_pack_start (GTK_BOX (vbox), picker->stack, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox), emoji_tone_row_new (picker),
							  FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (picker->popover), vbox);
	gtk_widget_show_all (vbox);

	emoji_picker_sync_category_buttons (picker);
}

static void
emoji_popover_show_cb (GtkWidget *popover, gpointer userdata)
{
	EmojiPicker *picker = userdata;

	if (!picker->populated)
	{
		picker->populated = TRUE;
		emoji_picker_build_contents (picker);
	}
	else if (*gtk_entry_get_text (GTK_ENTRY (picker->search_entry)))
	{
		gtk_entry_set_text (GTK_ENTRY (picker->search_entry), "");
	}

	emoji_picker_sync_tone_buttons (picker);
	emoji_picker_refresh (picker);
	gtk_widget_grab_focus (picker->search_entry);
}

static void
emoji_popover_closed_cb (GtkWidget *popover, gpointer userdata)
{
	EmojiPicker *picker = userdata;
	GtkEditable *editable;

	if (!picker->target)
		return;

	/* hand focus back to the input box without selecting its contents */
	editable = GTK_EDITABLE (picker->target);
	gtk_editable_set_editable (editable, FALSE);
	gtk_widget_grab_focus (picker->target);
	gtk_editable_set_editable (editable, TRUE);
}

static const char *
emoji_picker_button_icon_name (void)
{
	static const char *icon_names[] = {
		"face-smile-symbolic",
		"face-smile",
		"insert-emoticon-symbolic",
		"insert-emoticon",
		"zc-menu-emoji",
	};
	GtkIconTheme *theme = gtk_icon_theme_get_default ();
	gsize i;

	if (!theme)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS (icon_names); i++)
	{
		if (gtk_icon_theme_has_icon (theme, icon_names[i]))
			return icon_names[i];
	}

	return NULL;
}

static void
emoji_picker_free (gpointer data)
{
	EmojiPicker *picker = data;

	if (picker->target)
		g_object_remove_weak_pointer (G_OBJECT (picker->target),
												(gpointer *) &picker->target);
	g_free (picker);
}

GtkWidget *
emoji_picker_button_new (GtkWidget *target_entry)
{
	EmojiPicker *picker;
	const char *icon_name;

	g_return_val_if_fail (GTK_IS_ENTRY (target_entry), NULL);

	picker = g_new0 (EmojiPicker, 1);
	picker->target = target_entry;
	picker->category = EMOJI_GROUP_SMILEYS_EMOTION;
	g_object_add_weak_pointer (G_OBJECT (target_entry),
										(gpointer *) &picker->target);

	picker->button = gtk_menu_button_new ();
	gtk_button_set_relief (GTK_BUTTON (picker->button), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (picker->button, FALSE);
	gtk_widget_set_tooltip_text (picker->button, _("Insert Emoji"));
	emoji_picker_set_a11y_name (picker->button, _("Insert Emoji"));
	g_object_set_data_full (G_OBJECT (picker->button), "emoji-picker",
									picker, emoji_picker_free);

	icon_name = emoji_picker_button_icon_name ();
	if (icon_name)
	{
		gtk_button_set_image (GTK_BUTTON (picker->button),
			gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU));
	}
	else
	{
		/* no themed icon anywhere: fall back to an emoji label so the
		 * button never turns into an unusable placeholder */
		emoji_picker_ensure_css ();
		gtk_container_add (GTK_CONTAINER (picker->button),
								 emoji_label_new ("\360\237\231\202" /* 🙂 */, TRUE));
	}

	/* the popover shell is cheap; its contents are built on first open so
	 * startup cost stays unchanged */
	picker->popover = gtk_popover_new (picker->button);
	gtk_popover_set_position (GTK_POPOVER (picker->popover), GTK_POS_TOP);
	g_signal_connect (G_OBJECT (picker->popover), "show",
							G_CALLBACK (emoji_popover_show_cb), picker);
	g_signal_connect (G_OBJECT (picker->popover), "closed",
							G_CALLBACK (emoji_popover_closed_cb), picker);
	gtk_menu_button_set_popover (GTK_MENU_BUTTON (picker->button),
										  picker->popover);

	return picker->button;
}
