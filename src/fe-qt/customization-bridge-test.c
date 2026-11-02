/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "../common/outbound.h"
#include "customization-bridge.h"
#include "../common/userlist.h"
#include <stdio.h>
#include <string.h>
GSList *replace_list, *ctcp_list, *command_list, *usermenu_list, *menu_list, *button_list, *dlgbutton_list, *popup_list, *tabmenu_list, *urlhandler_list;
static char *directory;
static session context;
static int changed, menus_changed, commands, executions;
static char last[8192], last_target[4096], last_all[8192], last_host[256], last_account[256];
static struct User selected_user;
struct User *userlist_find(session *sess, const char *name) { (void)sess; return !strcmp(name, "Alice") ? &selected_user : NULL; }
char *get_xdir(void) { return directory; }
int is_session(session *sess) { return sess == &context; }
void qt_core_activate(void *sess) { (void)sess; }
char *server_get_network(server *serv, gboolean fallback) { (void)serv; (void)fallback; return "Network"; }
int cfg_get_bool(char *name) { return !strcmp(name, "known") ? 1 : -1; }
void qt_fe_customization_changed(int kind) { (void)kind; ++changed; }
void qt_fe_menus_changed(void) { ++menus_changed; }
void zoitechat_exec(const char *cmd) { ++executions; g_strlcpy(last, cmd, sizeof(last)); }
int handle_command(session *sess, char *cmd, int special) { (void)sess; (void)special; ++commands; g_strlcpy(last, cmd, sizeof(last)); return 1; }
int auto_insert(char *dest, gsize size, unsigned char *src, char *word[], char *word_eol[],
                char *a, char *c, char *d, char *e, char *h, char *n, char *s, char *u)
{
    (void)word; (void)word_eol; (void)d;
    g_strlcpy(last_target, s, sizeof(last_target)); g_strlcpy(last_all, a, sizeof(last_all));
    g_strlcpy(last_host, h, sizeof(last_host)); g_strlcpy(last_account, u, sizeof(last_account));
    g_snprintf(dest, size, "%s %s %s %s", src, c, e, n); return 1;
}
void list_addentry(GSList **list, char *cmd, char *name)
{
    size_t n = strlen(name) + 1, c = strlen(cmd) + 1;
    struct popup *entry = g_malloc(sizeof(*entry) + n + c);
    entry->name = (char *)(entry + 1); entry->cmd = entry->name + n;
    memcpy(entry->name, name, n); memcpy(entry->cmd, cmd, c); *list = g_slist_append(*list, entry);
}
void list_free(GSList **list) { g_slist_free_full(*list, g_free); *list = NULL; }
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #test); return 1; } } while (0)
struct IconResult { void *identity; char *value; char *markup; };
static void icon_row(const QtDynamicMenu *row, void *data)
{
    struct IconResult *result = data;
    if (row->identity == result->identity) { g_free(result->value); result->value = g_strdup(row->icon); g_free(result->markup); result->markup = g_strdup(row->markup); }
}
int main(void)
{
    GError *error = NULL;
    char version[65], old[65], message[1024], *disk = NULL;
    server serv = {0}; context.server = &serv;
    g_strlcpy(context.channel, "#test", sizeof(context.channel)); g_strlcpy(serv.nick, "nickname", sizeof(serv.nick));
    directory = g_dir_make_tmp("zoitechat-qt-custom-test-XXXXXX", &error); CHECK(directory && !error);
    CHECK(qt_core_config_read(QT_CONFIG_REPLACE, NULL, NULL, version, sizeof(version)));
    g_strlcpy(old, version, sizeof(old));
    QtConfigRow rows[] = {{"teh", "the"}, {"café", "coffee"}};
    CHECK(qt_core_config_save(QT_CONFIG_REPLACE, rows, 2, version, message, sizeof(message)) && changed == 1);
    CHECK(!strcmp(qt_core_replace_word("teh"), "the") && qt_core_replace_word("Teh") == NULL);
    char *path = g_build_filename(directory, "replace.conf", NULL);
    CHECK(g_file_get_contents(path, &disk, NULL, &error) && strstr(disk, "NAME café\nCMD coffee\n")); g_free(disk);
    CHECK(!qt_core_config_save(QT_CONFIG_REPLACE, rows, 1, old, message, sizeof(message)) && g_slist_length(replace_list) == 2);
    CHECK(qt_core_config_read(QT_CONFIG_REPLACE, NULL, NULL, version, sizeof(version)));
    CHECK(g_file_set_contents(path, "# externally changed\n", -1, &error));
    CHECK(!qt_core_config_save(QT_CONFIG_REPLACE, rows, 1, version, message, sizeof(message)) && changed == 1);
    CHECK(qt_core_config_read(QT_CONFIG_REPLACE, NULL, NULL, version, sizeof(version)));
    QtConfigRow invalid[] = {{"bad\nname", "command"}};
    CHECK(!qt_core_config_save(QT_CONFIG_REPLACE, invalid, 1, version, message, sizeof(message)));
    QtConfigRow spaced[] = {{"bad name", "command"}};
    CHECK(!qt_core_config_save(QT_CONFIG_REPLACE, spaced, 1, version, message, sizeof(message)));
    char long_command[385]; memset(long_command, 'x', sizeof(long_command)); long_command[384] = 0;
    QtConfigRow oversized[] = {{"name", long_command}};
    CHECK(!qt_core_config_save(QT_CONFIG_REPLACE, oversized, 1, version, message, sizeof(message)));
    CHECK(qt_core_config_read(QT_CONFIG_USERMENU, NULL, NULL, version, sizeof(version)));
    QtConfigRow bad_sub[] = {{"SUB", "Tools"}, {"Echo", "echo test"}};
    CHECK(!qt_core_config_save(QT_CONFIG_USERMENU, bad_sub, 2, version, message, sizeof(message)));
    QtConfigRow good_sub[] = {{"SUB", "Tools"}, {"Echo", "echo test"}, {"ENDSUB", ""}, {"SEP", ""}, {"TOGGLE Test", "known"}};
    CHECK(qt_core_config_save(QT_CONFIG_USERMENU, good_sub, 5, version, message, sizeof(message)));
    CHECK(qt_core_config_read(QT_CONFIG_COMMANDS, NULL, NULL, version, sizeof(version)));
    QtConfigRow duplicates[] = {{"ALIAS", "echo one"}, {"ALIAS", "echo two"}};
    CHECK(qt_core_config_save(QT_CONFIG_COMMANDS, duplicates, 2, version, message, sizeof(message)) && g_slist_length(command_list) == 2);
    CHECK(qt_core_custom_run(&context, "/echo %c"));
    CHECK(!strcmp(last, "echo %c #test Network nickname"));
    CHECK(!qt_core_custom_run(NULL, "echo test") && !qt_core_custom_run(&context, "echo one\necho two"));
    CHECK(qt_core_custom_pref("known") == 1 && qt_core_custom_pref("bad name") == -1);
    context.type = SESS_CHANNEL; selected_user.hostname = "ident@example.test"; selected_user.account = "alice-account";
    CHECK(qt_core_custom_run_target(&context, "echo %s %a %h %u", "Alice", "Alice Bob"));
    CHECK(!strcmp(last_target, "Alice") && !strcmp(last_all, "Alice Bob") && !strcmp(last_host, "example.test") && !strcmp(last_account, "alice-account"));
    CHECK(!qt_core_custom_run_target(&context, "echo %s", "Alice\nquit", "Alice"));
    const char *extra_files[] = {"buttons.conf", "dlgbuttons.conf", "popup.conf", "tabmenu.conf", "urlhandlers.conf"};
    for (int kind = QT_CONFIG_BUTTONS; kind < QT_CONFIG_COUNT; ++kind) {
        QtConfigRow custom[] = {{"Inspect", "echo %s"}};
        CHECK(qt_core_config_read(kind, NULL, NULL, version, sizeof(version)));
        CHECK(qt_core_config_save(kind, custom, 1, version, message, sizeof(message)));
        char *file = g_build_filename(directory, extra_files[kind - QT_CONFIG_BUTTONS], NULL);
        CHECK(g_file_get_contents(file, &disk, NULL, NULL) && strstr(disk, "CMD echo %s")); g_free(disk); disk = NULL;
        g_remove(file); g_free(file);
    }
    menu_entry toggle = {0}; toggle.path = "Window"; toggle.label = "Toggle"; toggle.cmd = "echo on"; toggle.ucmd = "echo off"; toggle.enable = 1;
    menu_list = g_slist_append(menu_list, &toggle);
    CHECK(qt_core_menu_activate(&context, &toggle, "echo on", 1) && toggle.state == 1);
    CHECK(qt_core_menu_activate(&context, &toggle, "echo on", 0) && toggle.state == 0 && g_str_has_prefix(last, "echo off"));
    int before = commands;
    CHECK(!qt_core_menu_activate(&context, &toggle, "changed", 1) && commands == before);
    toggle.enable = 0; CHECK(!qt_core_menu_activate(&context, &toggle, "echo on", 1)); toggle.enable = 1;
    g_slist_free(menu_list); menu_list = NULL;
    CHECK(!qt_core_menu_activate(&context, &toggle, "echo on", 1) && commands == before);
    menu_entry first = {0}, second = {0}; first.path = second.path = "Window"; first.label = "One"; second.label = "Two";
    first.group = second.group = "One"; first.cmd = "echo one"; second.cmd = "echo two"; first.enable = second.enable = 1; first.state = 1;
    menu_list = g_slist_append(NULL, &first); menu_list = g_slist_append(menu_list, &second);
    char *icon_file = g_build_filename(directory, "menu icon.png", NULL);
    CHECK(g_file_set_contents(icon_file, "fixture", -1, NULL));
    struct IconResult icon = {&first, NULL, NULL}; first.icon = "menu icon.png";
    qt_core_menus_foreach(icon_row, &icon); CHECK(!g_strcmp0(icon.value, icon_file));
    first.icon = icon_file; qt_core_menus_foreach(icon_row, &icon); CHECK(!g_strcmp0(icon.value, icon_file));
    first.icon = "gtk-open"; qt_core_menus_foreach(icon_row, &icon); CHECK(!g_strcmp0(icon.value, "gtk-open"));
    first.icon = NULL; qt_core_menus_foreach(icon_row, &icon); CHECK(!icon.value);
    first.markup = 1; first.label = "<b>_Styled &amp; Label</b>";
    char *plain = qt_core_menu_added(&first);
    CHECK(plain && !strcmp(plain, "_Styled & Label"));
    first.label = plain;
    qt_core_menus_foreach(icon_row, &icon);
    CHECK(icon.markup && !strcmp(icon.markup, "<b>_Styled &amp; Label</b>"));
    qt_core_menu_removed(&first); qt_core_menus_foreach(icon_row, &icon); CHECK(!icon.markup);
    g_free(plain);
    first.label = "<b>invalid"; CHECK(!qt_core_menu_added(&first));
    first.markup = 0; first.label = "One"; CHECK(!qt_core_menu_added(&first));
    g_remove(icon_file); g_free(icon_file);
    CHECK(qt_core_menu_activate(&context, &second, "echo two", 1) && !first.state && second.state);
    CHECK(menus_changed >= 3 && executions == 0);
    char *saved_directory = directory; directory = g_build_filename(saved_directory, "missing", "nested", NULL);
    CHECK(qt_core_config_read(QT_CONFIG_REPLACE, NULL, NULL, version, sizeof(version)));
    CHECK(!qt_core_config_save(QT_CONFIG_REPLACE, rows, 1, version, message, sizeof(message)) && g_slist_length(replace_list) == 2);
    g_free(directory); directory = saved_directory;
    g_slist_free(menu_list); list_free(&replace_list); list_free(&ctcp_list); list_free(&command_list); list_free(&usermenu_list);
    list_free(&button_list); list_free(&dlgbutton_list); list_free(&popup_list); list_free(&tabmenu_list); list_free(&urlhandler_list);
    const char *files[] = {"replace.conf", "usermenu.conf", "commands.conf"};
    for (unsigned i = 0; i < G_N_ELEMENTS(files); ++i) { char *file = g_build_filename(directory, files[i], NULL); g_remove(file); g_free(file); }
    g_rmdir(directory); g_free(directory); g_free(path);
    return 0;
}
