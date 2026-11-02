/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_CUSTOMIZATION_BRIDGE_H
#define ZOITECHAT_QT_CUSTOMIZATION_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
enum { QT_CONFIG_REPLACE, QT_CONFIG_CTCP, QT_CONFIG_COMMANDS, QT_CONFIG_USERMENU,
       QT_CONFIG_BUTTONS, QT_CONFIG_DIALOG_BUTTONS, QT_CONFIG_POPUP, QT_CONFIG_TAB_POPUP, QT_CONFIG_URL_HANDLERS, QT_CONFIG_COUNT };
typedef struct { const char *name; const char *command; } QtConfigRow;
typedef void (*QtConfigVisitor)(const char *, const char *, void *);
int qt_core_config_read(int kind, QtConfigVisitor visitor, void *data, char *revision, int capacity);
int qt_core_config_save(int kind, const QtConfigRow *rows, int count, const char *revision, char *error, int capacity);
const char *qt_core_replace_word(const char *word);
int qt_core_custom_run(void *session, const char *command);
int qt_core_custom_run_target(void *session, const char *command, const char *target, const char *all_targets);
int qt_core_custom_pref(const char *name);
typedef struct {
    void *identity;
    const char *path, *label, *command, *uncheck_command, *group;
    int main_menu, position, checked, enabled, key, modifiers;
    const char *icon; /* borrowed filename or theme name */
    const char *markup; /* original marked-up label, or NULL */
} QtDynamicMenu;
char *qt_core_menu_added(void *identity); /* g_free the stripped label */
void qt_core_menu_removed(void *identity);
void qt_core_menus_foreach(void (*visitor)(const QtDynamicMenu *, void *), void *data);
int qt_core_menu_activate(void *session, void *identity, const char *expected_command, int checked);
int qt_core_menu_activate_target(void *session, void *identity, const char *expected_command, int checked,
                                 const char *target, const char *all_targets);
void qt_fe_customization_changed(int kind);
void qt_fe_menus_changed(void);
#ifdef __cplusplus
}
#endif
#endif
