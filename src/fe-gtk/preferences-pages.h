/* ZoiteChat - native editors for the everyday preferences pages.
 * Licensed under the GNU General Public License, version 2 or later.
 */
#ifndef ZOITECHAT_PREFERENCES_PAGES_H
#define ZOITECHAT_PREFERENCES_PAGES_H

#include <gtk/gtk.h>

struct zoitechatprefs;

typedef enum
{
        PREFERENCES_ALERT_DESKTOP = 1 << 0,
        PREFERENCES_ALERT_TRAY = 1 << 1,
        PREFERENCES_ALERT_ATTENTION = 1 << 2,
        PREFERENCES_ALERT_TRAY_BEHAVIOR = 1 << 3,
        PREFERENCES_ALERT_HIDDEN_ONLY = 1 << 4
} PreferencesAlertCapabilities;

typedef void (*PreferencesSoundChanged) (gpointer user_data);

/* All editors write only to the window's staged preferences. The caller owns
 * both the preferences and the staged Beep event's allocated filename. */
GtkWidget *preferences_text_page_new (struct zoitechatprefs *staged,
                                      const char *const *languages);
GtkWidget *preferences_tabs_page_new (struct zoitechatprefs *staged);
GtkWidget *preferences_notifications_page_new (struct zoitechatprefs *staged,
        PreferencesAlertCapabilities capabilities, char **beep_file,
        PreferencesSoundChanged sound_changed, gpointer user_data);
void preferences_notifications_refresh_sound (GtkWidget *page);
GtkWidget *preferences_history_page_new (struct zoitechatprefs *staged,
                                         const char *config_dir);
gboolean preferences_history_page_stage (GtkWidget *page, GError **error);

typedef struct
{
        const char *code;
        const char *name;
} PreferencesSpellLanguage;
typedef void (*PreferencesTypingPreview) (GtkWidget *entry, const struct zoitechatprefs *staged);
GtkWidget *preferences_typing_page_new (struct zoitechatprefs *staged, GtkWidget *preview_entry,
        const PreferencesSpellLanguage *languages, guint count, PreferencesTypingPreview update_preview);
gboolean preferences_typing_page_stage (GtkWidget *page, GError **error);

/* These columns retain the existing keybindings.conf editor's model and order. */
enum
{
        PREFERENCES_SHORTCUT_LABEL, PREFERENCES_SHORTCUT_ACCEL,
        PREFERENCES_SHORTCUT_ACTION, PREFERENCES_SHORTCUT_DATA1,
        PREFERENCES_SHORTCUT_DATA2, PREFERENCES_SHORTCUT_CUSTOM,
        PREFERENCES_SHORTCUT_N_COLUMNS
};
typedef struct { const char *id; const char *help; } PreferencesShortcutAction;
typedef GdkModifierType (*PreferencesShortcutModifiers) (GdkModifierType modifiers);
typedef void (*PreferencesShortcutsRestore) (gpointer data);
GtkWidget *preferences_shortcuts_page_new (GtkListStore *store,
        const PreferencesShortcutAction *actions, guint count,
        PreferencesShortcutModifiers normalize, PreferencesShortcutsRestore restore, gpointer data);
void preferences_shortcuts_reveal_path (GtkWidget *page, GtkTreePath *raw_path);
void preferences_shortcuts_find_action (GtkWidget *page, const char *action_id);

#endif
