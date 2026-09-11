/* Regression coverage shared by Meson and the Windows CI build.
 * Distributed under GPL-2.0-or-later; see COPYING. */
#include "../fe-gtk.h"
#include "../../common/zoitechat.h"
#include "../../common/emoji-data.h"
#include "../emoji-font.h"
#include "../emoji-picker.h"
#include "../preferences-persistence.h"
#include <pango/pangofc-fontmap.h>
#include <glib/gstdio.h>

struct zoitechatprefs prefs;
static char *config_dir;

static void
test_theme_svg_loader (void)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf;

	/* Exercise the theme asset that failed in the packaged Windows runtime. */
	pixbuf = gdk_pixbuf_new_from_resource (
		"/org/gtk/libgtk/theme/Adwaita/assets/bullet-symbolic.svg", &error);
	g_assert_no_error (error);
	g_assert_nonnull (pixbuf);
	g_assert_cmpint (gdk_pixbuf_get_width (pixbuf), >, 0);
	g_assert_cmpint (gdk_pixbuf_get_height (pixbuf), >, 0);
	g_object_unref (pixbuf);
}

char *get_xdir (void) { return config_dir; }
PreferencesPersistenceResult preferences_persistence_save_all (void)
{
	PreferencesPersistenceResult result = {0};
	result.success = TRUE;
	return result;
}

static GtkWidget *
find_widget (GtkWidget *widget, GType type)
{
	GList *children, *item;
	GtkWidget *found = NULL;

	if (G_TYPE_CHECK_INSTANCE_TYPE (widget, type))
		return widget;
	if (!GTK_IS_CONTAINER (widget))
		return NULL;
	children = gtk_container_get_children (GTK_CONTAINER (widget));
	for (item = children; item && !found; item = item->next)
		found = find_widget (item->data, type);
	g_list_free (children);
	return found;
}

typedef struct { GtkWidget *window, *entry, *button, *popover, *search; } PickerFixture;

static void
fixture_setup (PickerFixture *f, gconstpointer data)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	f->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	f->entry = gtk_entry_new ();
	f->button = emoji_picker_button_new (f->entry);
	gtk_container_add (GTK_CONTAINER (f->window), box);
	gtk_box_pack_start (GTK_BOX (box), f->entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), f->button, FALSE, FALSE, 0);
	gtk_widget_show_all (f->window);
	while (gtk_events_pending ()) gtk_main_iteration ();
	f->popover = GTK_WIDGET (gtk_menu_button_get_popover (GTK_MENU_BUTTON (f->button)));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (f->button), TRUE);
	f->search = find_widget (f->popover, GTK_TYPE_SEARCH_ENTRY);
	g_assert_nonnull (f->search);
}

static void
fixture_teardown (PickerFixture *f, gconstpointer data)
{
	gtk_widget_destroy (f->window);
	while (gtk_events_pending ()) gtk_main_iteration ();
}

static void
insert_duck (PickerFixture *f)
{
	gtk_entry_set_text (GTK_ENTRY (f->search), "bird duck");
	/* Do not wait for GTK's debounced search-changed signal. */
	g_signal_emit_by_name (f->search, "activate");
}

static void
test_search_insertion (PickerFixture *f, gconstpointer data)
{
	gtk_entry_set_text (GTK_ENTRY (f->entry), "before after");
	gtk_editable_select_region (GTK_EDITABLE (f->entry), 7, 12);
	insert_duck (f);
	g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f->entry)), ==, "before \360\237\246\206");
	g_assert_cmpint (gtk_editable_get_position (GTK_EDITABLE (f->entry)), ==, 8);
}

static void
test_read_only (PickerFixture *f, gconstpointer data)
{
	gtk_entry_set_text (GTK_ENTRY (f->entry), "unchanged");
	gtk_editable_set_editable (GTK_EDITABLE (f->entry), FALSE);
	insert_duck (f);
	g_signal_emit_by_name (f->search, "stop-search");
	g_assert_false (gtk_editable_get_editable (GTK_EDITABLE (f->entry)));
	g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f->entry)), ==, "unchanged");
}

static void
test_sequence_limit (PickerFixture *f, gconstpointer data)
{
	gtk_entry_set_max_length (GTK_ENTRY (f->entry), 1);
	gtk_entry_set_text (GTK_ENTRY (f->search), "flag canada");
	g_signal_emit_by_name (f->search, "activate");
	g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f->entry)), ==, "");
	gtk_entry_set_max_length (GTK_ENTRY (f->entry), 2);
	g_signal_emit_by_name (f->search, "activate");
	g_assert_cmpstr (gtk_entry_get_text (GTK_ENTRY (f->entry)), ==, "\360\237\207\250\360\237\207\246");
}

static void
test_entry_action (PickerFixture *f, gconstpointer data)
{
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (f->button), FALSE);
	if (g_signal_lookup ("insert-emoji", GTK_TYPE_ENTRY))
	{
		g_signal_emit_by_name (f->entry, "insert-emoji");
		g_assert_true (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (f->button)));
		g_assert_true (gtk_widget_get_visible (f->popover));
	}
}

int
main (int argc, char **argv)
{
	char *recents;
	gtk_test_init (&argc, &argv, NULL);
	g_object_set (gtk_settings_get_default (), "gtk-enable-animations", FALSE, NULL);
	config_dir = g_dir_make_tmp ("zoitechat-emoji-tests-XXXXXX", NULL);
	g_assert_nonnull (config_dir);
	g_test_add_func ("/emoji-ui/theme-svg-loader", test_theme_svg_loader);
	g_test_add ("/emoji-ui/search-insertion", PickerFixture, NULL, fixture_setup, test_search_insertion, fixture_teardown);
	g_test_add ("/emoji-ui/read-only", PickerFixture, NULL, fixture_setup, test_read_only, fixture_teardown);
	g_test_add ("/emoji-ui/sequence-limit", PickerFixture, NULL, fixture_setup, test_sequence_limit, fixture_teardown);
	g_test_add ("/emoji-ui/entry-action", PickerFixture, NULL, fixture_setup, test_entry_action, fixture_teardown);
	argc = g_test_run ();
	recents = g_build_filename (config_dir, "emoji-recents.conf", NULL);
	g_unlink (recents);
	g_rmdir (config_dir);
	g_free (recents);
	g_free (config_dir);
	return argc;
}
