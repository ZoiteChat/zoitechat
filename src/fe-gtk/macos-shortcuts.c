/* macOS text editing shortcuts. GPL-2.0-or-later; see COPYING. */
#include "macos-shortcuts.h"
#include "config.h"
#include "../common/zoitechat.h"
#include "xtext.h"
#include <gdk/gdkkeysyms.h>

static void (*preferences_callback) (void);

static void
copy_text (GtkTextView *view)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
	GtkTextIter start, end;
	if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
	{
		gchar *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
		gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (view),
		                        GDK_SELECTION_CLIPBOARD), text, -1);
		g_free (text);
	}
}

static void
cut_text (GtkTextView *view)
{
	copy_text (view);
	gtk_text_buffer_delete_selection (gtk_text_view_get_buffer (view), TRUE,
	                                  gtk_text_view_get_editable (view));
	gtk_text_view_reset_im_context (view);
	gtk_text_view_scroll_mark_onscreen (view,
		gtk_text_buffer_get_insert (gtk_text_view_get_buffer (view)));
}

typedef struct
{
	GtkTextView *view;
	GtkTextBuffer *buffer;
} TextPaste;

static void
text_paste_received (GtkClipboard *clipboard, const gchar *text, gpointer userdata)
{
	TextPaste *paste = userdata;
	GtkTextView *view = paste->view;
	GtkTextBuffer *buffer = paste->buffer;

	/* Clipboard replies may arrive after a view was closed or switched to
	 * another buffer. Retain the request's objects, but never edit a new one. */
	if (text && gtk_widget_get_realized (GTK_WIDGET (view)) &&
	    gtk_text_view_get_buffer (view) == buffer)
	{
		gboolean editable = gtk_text_view_get_editable (view);
		gtk_text_buffer_begin_user_action (buffer);
		gtk_text_buffer_delete_selection (buffer, TRUE, editable);
		gtk_text_buffer_insert_interactive_at_cursor (buffer, text, -1, editable);
		gtk_text_buffer_end_user_action (buffer);
		gtk_text_view_reset_im_context (view);
		gtk_text_view_scroll_mark_onscreen (view, gtk_text_buffer_get_insert (buffer));
		g_signal_emit_by_name (buffer, "paste-done", clipboard);
	}
	g_object_unref (buffer);
	g_object_unref (view);
	g_free (paste);
}

static void
paste_text (GtkTextView *view)
{
	TextPaste *paste = g_new (TextPaste, 1);
	paste->view = g_object_ref (view);
	paste->buffer = g_object_ref (gtk_text_view_get_buffer (view));
	/* ZoiteChat's editors consume text. GTK3's Quartz rich-text fallback
	 * enumerates pasteboard UTIs without MIME names (a NULL atom assertion)
	 * and its rich-text request callback is unimplemented. Request UTF-8
	 * directly, retaining interactive selection and read-only behavior. */
	gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (view),
	                            GDK_SELECTION_CLIPBOARD), text_paste_received, paste);
}

static void
open_preferences (GtkWindow *window)
{
	if (preferences_callback)
		preferences_callback ();
}

static void
minimize_window (GtkWindow *window)
{
	gtk_window_iconify (window);
}

void
macos_shortcuts_minimize_active (GtkWindow *fallback)
{
	GList *windows = gtk_window_list_toplevels ();
	GList *item;
	GtkWindow *target = fallback;

	for (item = windows; item; item = item->next)
	{
		if (gtk_window_is_active (GTK_WINDOW (item->data)))
		{
			target = GTK_WINDOW (item->data);
			break;
		}
	}
	if (target)
		minimize_window (target);
	g_list_free (windows);
}

static void
add_movement (GtkBindingSet *bindings, guint keyval, GdkModifierType mod,
              GtkMovementStep step, gint count)
{
	gtk_binding_entry_add_signal (bindings, keyval, mod, "move-cursor", 3,
	                              GTK_TYPE_MOVEMENT_STEP, step,
	                              G_TYPE_INT, count, G_TYPE_BOOLEAN, FALSE);
	gtk_binding_entry_add_signal (bindings, keyval, mod | GDK_SHIFT_MASK,
	                              "move-cursor", 3, GTK_TYPE_MOVEMENT_STEP, step,
	                              G_TYPE_INT, count, G_TYPE_BOOLEAN, TRUE);
}

static void
delete_text_view_backwards (GtkTextView *view, gboolean word)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer (view);
	GtkTextIter start, end;
	gboolean editable = gtk_text_view_get_editable (view);

	/* GTK3 leaves GTK_DELETE_DISPLAY_LINE_ENDS unimplemented in TextView.
	 * Preserve selection deletion and per-range editability instead of
	 * moving the cursor first or deleting protected text unconditionally. */
	if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
		gtk_text_buffer_delete_selection (buffer, TRUE, editable);
	else
	{
		gtk_text_buffer_get_iter_at_mark (buffer, &end, gtk_text_buffer_get_insert (buffer));
		start = end;
		if (word)
			gtk_text_iter_backward_word_start (&start);
		else
			gtk_text_view_backward_display_line_start (view, &start);
		gtk_text_buffer_begin_user_action (buffer);
		gtk_text_buffer_delete_interactive (buffer, &start, &end, editable);
		gtk_text_buffer_end_user_action (buffer);
	}
	gtk_text_view_reset_im_context (view);
	gtk_text_view_scroll_mark_onscreen (view, gtk_text_buffer_get_insert (buffer));
}

static void
delete_display_line_start (GtkTextView *view)
{
	delete_text_view_backwards (view, FALSE);
}

static void
delete_word_start (GtkTextView *view)
{
	delete_text_view_backwards (view, TRUE);
}

static void
install_text_bindings (GtkSettings *settings, GParamSpec *property,
                       GtkCssProvider *provider)
{
	GtkWidget *widgets[2];
	const char *selectors[] = {"entry", "textview"};
	const char *names[] = {"zoitechat-macos-entry", "zoitechat-macos-textview"};
	GdkScreen *screen = gdk_screen_get_default ();
	GString *css = g_string_new (NULL);
	guint i, j;

	/* CSS key themes take precedence over class bindings. Put our small
	 * sets first, retaining the theme's remaining bindings as fallbacks.
	 * Remove our previous provider before reading a changed key theme. */
	g_object_ref (provider);
	gtk_style_context_remove_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider));
	widgets[0] = gtk_entry_new ();
	widgets[1] = gtk_text_view_new ();
	for (i = 0; i < G_N_ELEMENTS (widgets); i++)
	{
		GPtrArray *fallbacks = NULL;
		GtkStyleContext *context = gtk_widget_get_style_context (widgets[i]);

		g_object_ref_sink (widgets[i]);
		gtk_style_context_get (context, gtk_style_context_get_state (context),
		                       "-gtk-key-bindings", &fallbacks, NULL);
		g_string_append_printf (css, "%s { -gtk-key-bindings: %s", selectors[i], names[i]);
		for (j = 0; fallbacks && j < fallbacks->len; j++)
		{
			GtkBindingSet *binding = g_ptr_array_index (fallbacks, j);
			g_string_append_printf (css, ", %s", binding->set_name);
		}
		g_string_append (css, "; }\n");
		if (fallbacks)
			g_ptr_array_unref (fallbacks);
		gtk_widget_destroy (widgets[i]);
		g_object_unref (widgets[i]);
	}
	gtk_css_provider_load_from_data (provider, css->str, css->len, NULL);
	gtk_style_context_add_provider_for_screen (screen, GTK_STYLE_PROVIDER (provider),
	                                          GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_string_free (css, TRUE);
	g_object_unref (provider);
}

void
macos_shortcuts_init (void (*callback) (void))
{
	GType types[] = {GTK_TYPE_ENTRY, GTK_TYPE_TEXT_VIEW};
	const char *names[] = {"zoitechat-macos-entry", "zoitechat-macos-textview"};
	GtkCssProvider *provider;
	gpointer window_class;
	GtkBindingSet *window_bindings;
	static gboolean initialized = FALSE;
	guint i;

	preferences_callback = callback;
	if (initialized)
		return;
	initialized = TRUE;

	/* Use Command for these three actions on macOS, preserving GTK's
	 * selection, password-field and read-only checks. Entry subclasses
	 * (including the spell-checking input and search fields) inherit these. */
	for (i = 0; i < G_N_ELEMENTS (types); i++)
	{
		gpointer klass = g_type_class_ref (types[i]);
		GtkBindingSet *bindings = gtk_binding_set_new (names[i]);

		gtk_binding_entry_skip (bindings, GDK_KEY_c, GDK_CONTROL_MASK);
		gtk_binding_entry_skip (bindings, GDK_KEY_x, GDK_CONTROL_MASK);
		gtk_binding_entry_skip (bindings, GDK_KEY_v, GDK_CONTROL_MASK);
		if (types[i] == GTK_TYPE_TEXT_VIEW)
		{
			g_signal_new_class_handler ("zoitechat-macos-copy-text", types[i],
			                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			                           G_CALLBACK (copy_text), NULL, NULL, NULL,
			                           G_TYPE_NONE, 0);
			g_signal_new_class_handler ("zoitechat-macos-cut-text", types[i],
			                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			                           G_CALLBACK (cut_text), NULL, NULL, NULL,
			                           G_TYPE_NONE, 0);
			g_signal_new_class_handler ("zoitechat-macos-paste-text", types[i],
			                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			                           G_CALLBACK (paste_text), NULL, NULL, NULL,
			                           G_TYPE_NONE, 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_v, GDK_META_MASK,
			                              "zoitechat-macos-paste-text", 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_c, GDK_META_MASK,
			                              "zoitechat-macos-copy-text", 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_x, GDK_META_MASK,
			                              "zoitechat-macos-cut-text", 0);
		}
		else
		{
			gtk_binding_entry_add_signal (bindings, GDK_KEY_c, GDK_META_MASK,
			                              "copy-clipboard", 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_x, GDK_META_MASK,
			                              "cut-clipboard", 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_v, GDK_META_MASK,
			                              "paste-clipboard", 0);
		}
		/* Two movements select the whole buffer without changing its text. */
		gtk_binding_entry_add_signal (bindings, GDK_KEY_a, GDK_META_MASK,
		                              "move-cursor", 3, GTK_TYPE_MOVEMENT_STEP,
		                              GTK_MOVEMENT_BUFFER_ENDS, G_TYPE_INT, -1,
		                              G_TYPE_BOOLEAN, FALSE);
		gtk_binding_entry_add_signal (bindings, GDK_KEY_a, GDK_META_MASK,
		                              "move-cursor", 3, GTK_TYPE_MOVEMENT_STEP,
		                              GTK_MOVEMENT_BUFFER_ENDS, G_TYPE_INT, 1,
		                              G_TYPE_BOOLEAN, TRUE);
		add_movement (bindings, GDK_KEY_Left, GDK_META_MASK,
		              GTK_MOVEMENT_DISPLAY_LINE_ENDS, -1);
		add_movement (bindings, GDK_KEY_Right, GDK_META_MASK,
		              GTK_MOVEMENT_DISPLAY_LINE_ENDS, 1);
		add_movement (bindings, GDK_KEY_Left, GDK_MOD1_MASK,
		              GTK_MOVEMENT_WORDS, -1);
		add_movement (bindings, GDK_KEY_Right, GDK_MOD1_MASK,
		              GTK_MOVEMENT_WORDS, 1);
		add_movement (bindings, GDK_KEY_Up, GDK_META_MASK,
		              GTK_MOVEMENT_BUFFER_ENDS, -1);
		add_movement (bindings, GDK_KEY_Down, GDK_META_MASK,
		              GTK_MOVEMENT_BUFFER_ENDS, 1);
		if (types[i] == GTK_TYPE_TEXT_VIEW)
		{
			g_signal_new_class_handler ("zoitechat-macos-delete-word-start", types[i],
			                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			                           G_CALLBACK (delete_word_start),
			                           NULL, NULL, NULL, G_TYPE_NONE, 0);
			g_signal_new_class_handler ("zoitechat-macos-delete-line-start", types[i],
			                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			                           G_CALLBACK (delete_display_line_start),
			                           NULL, NULL, NULL, G_TYPE_NONE, 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_BackSpace, GDK_META_MASK,
			                              "zoitechat-macos-delete-line-start", 0);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_BackSpace, GDK_MOD1_MASK,
			                              "zoitechat-macos-delete-word-start", 0);
		}
		else
		{
			gtk_binding_entry_add_signal (bindings, GDK_KEY_BackSpace, GDK_MOD1_MASK,
			                              "delete-from-cursor", 2, GTK_TYPE_DELETE_TYPE,
			                              GTK_DELETE_WORD_ENDS, G_TYPE_INT, -1);
			gtk_binding_entry_add_signal (bindings, GDK_KEY_BackSpace, GDK_META_MASK,
			                              "delete-from-cursor", 2, GTK_TYPE_DELETE_TYPE,
			                              GTK_DELETE_DISPLAY_LINE_ENDS, G_TYPE_INT, -1);
		}
		g_type_class_unref (klass);
	}
	provider = gtk_css_provider_new ();
	install_text_bindings (gtk_settings_get_default (), NULL, provider);
	g_signal_connect_object (gtk_settings_get_default (), "notify::gtk-key-theme-name",
	                         G_CALLBACK (install_text_bindings), provider, 0);
	g_signal_connect_object (gtk_settings_get_default (), "notify::gtk-theme-name",
	                         G_CALLBACK (install_text_bindings), provider, 0);
	g_object_unref (provider);

	/* Window-level bindings also work in dialogs without a main menu bar.
	 * The native menu uses the same actions when Quartz handles the key. */
	window_class = g_type_class_ref (GTK_TYPE_WINDOW);
	window_bindings = gtk_binding_set_by_class (window_class);
	g_signal_new_class_handler ("zoitechat-macos-minimize", GTK_TYPE_WINDOW,
	                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
	                           G_CALLBACK (minimize_window), NULL, NULL, NULL,
	                           G_TYPE_NONE, 0);
	g_signal_new_class_handler ("zoitechat-macos-preferences", GTK_TYPE_WINDOW,
	                           G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
	                           G_CALLBACK (open_preferences), NULL, NULL, NULL,
	                           G_TYPE_NONE, 0);
	gtk_binding_entry_add_signal (window_bindings, GDK_KEY_m, GDK_META_MASK,
	                              "zoitechat-macos-minimize", 0);
	gtk_binding_entry_add_signal (window_bindings, GDK_KEY_comma, GDK_META_MASK,
	                              "zoitechat-macos-preferences", 0);
	g_type_class_unref (window_class);
}

gboolean
macos_shortcuts_remap_default (guint *keyval, GdkModifierType *mod,
                              const char *action, const char *data1,
                              const char *data2)
{
	/* Only exact legacy defaults qualify. Custom actions, parameters and
	 * alternate shortcuts must survive loading a configuration unchanged. */
	if (data2 && *data2)
		return FALSE;
	if (*keyval == GDK_KEY_Tab && *mod == GDK_META_MASK &&
	    !g_strcmp0 (action, "Complete nick/command") && !g_strcmp0 (data1, "Up"))
	{
		*mod = GDK_CONTROL_MASK;
		return TRUE;
	}
	if ((!data1 || !*data1) && *mod == GDK_MOD1_MASK &&
	    ((*keyval == GDK_KEY_Left && !g_strcmp0 (action, "Move front tab left")) ||
	     (*keyval == GDK_KEY_Right && !g_strcmp0 (action, "Move front tab right"))))
	{
		*mod |= GDK_META_MASK;
		return TRUE;
	}
	if (*keyval == GDK_KEY_m && *mod == GDK_META_MASK &&
	    !g_strcmp0 (action, "Menu Shortcut") && !g_strcmp0 (data1, "reset-marker"))
	{
		*keyval = GDK_KEY_R;
		*mod |= GDK_SHIFT_MASK;
		return TRUE;
	}
	return FALSE;
}

gboolean
macos_shortcuts_copy_chat (GtkWidget *entry, GdkEventKey *event, GtkXText *xtext)
{
	gint start, end;
	GdkModifierType modifiers = event->state &
		(GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK |
		 GDK_META_MASK | GDK_SUPER_MASK | GDK_HYPER_MASK);

	/* XText leaves keyboard focus in the message input. Prefer an input
	 * selection; otherwise Command+C copies the selected chat text. Leave
	 * Command+Shift+C and all other shortcuts to their existing handlers. */
	if (gdk_keyval_to_lower (event->keyval) != GDK_KEY_c || modifiers != GDK_META_MASK ||
	    gtk_editable_get_selection_bounds (GTK_EDITABLE (entry), &start, &end))
		return FALSE;

	gtk_xtext_copy_selection (xtext);
	return TRUE;
}
