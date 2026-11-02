/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/outbound.h"
#include "../common/server.h"
#include "../common/util.h"
#include "../common/fe.h"
#include "../common/userlist.h"
#include "customization-bridge.h"
#include "qt-bridge.h"
#include <string.h>
#include <pango/pango.h>
static GSList **config_list(int kind)
{
    switch (kind) { case QT_CONFIG_REPLACE: return &replace_list; case QT_CONFIG_CTCP: return &ctcp_list;
        case QT_CONFIG_COMMANDS: return &command_list; case QT_CONFIG_USERMENU: return &usermenu_list;
        case QT_CONFIG_BUTTONS: return &button_list; case QT_CONFIG_DIALOG_BUTTONS: return &dlgbutton_list;
        case QT_CONFIG_POPUP: return &popup_list; case QT_CONFIG_TAB_POPUP: return &tabmenu_list;
        case QT_CONFIG_URL_HANDLERS: return &urlhandler_list; default: return NULL; }
}
static char *config_path(int kind)
{
    static const char *files[] = {"replace.conf", "ctcpreply.conf", "commands.conf", "usermenu.conf", "buttons.conf",
        "dlgbuttons.conf", "popup.conf", "tabmenu.conf", "urlhandlers.conf"};
    return kind >= 0 && kind < QT_CONFIG_COUNT ? g_build_filename(get_xdir(), files[kind], NULL) : NULL;
}
static GString *serialize(GSList *list)
{
    GString *text = g_string_new("");
    for (; list; list = list->next) {
        struct popup *row = list->data;
        g_string_append_printf(text, "NAME %s\nCMD %s\n\n", row->name, row->cmd);
    }
    return text;
}
static char *revision_for(int kind)
{
    GSList **list = config_list(kind);
    GChecksum *hash;
    GString *text;
    char *file, *disk = NULL, *result;
    gsize length = 0;
    if (!list) return NULL;
    hash = g_checksum_new(G_CHECKSUM_SHA256); text = serialize(*list);
    g_checksum_update(hash, (guchar *)text->str, text->len);
    file = config_path(kind);
    if (g_file_get_contents(file, &disk, &length, NULL)) {
        g_checksum_update(hash, (guchar *)"\0file\0", 6); g_checksum_update(hash, (guchar *)disk, length);
    } else g_checksum_update(hash, (guchar *)"\0missing", 8);
    result = g_strdup(g_checksum_get_string(hash));
    g_checksum_free(hash); g_string_free(text, TRUE); g_free(disk); g_free(file); return result;
}
int qt_core_config_read(int kind, QtConfigVisitor visitor, void *data, char *revision, int capacity)
{
    GSList **list = config_list(kind), *link;
    char *value;
    if (!list || !revision || capacity < 65) return FALSE;
    value = revision_for(kind); g_strlcpy(revision, value, capacity); g_free(value);
    if (visitor) for (link = *list; link; link = link->next) {
        struct popup *row = link->data; visitor(row->name, row->cmd, data);
    }
    return TRUE;
}
static int fail(char *error, int capacity, const char *message)
{
    if (error && capacity > 0) g_strlcpy(error, message, capacity);
    return FALSE;
}
int qt_core_config_save(int kind, const QtConfigRow *rows, int count, const char *revision, char *error, int capacity)
{
    GSList **target = config_list(kind), *replacement = NULL;
    char *current, *path;
    GString *text;
    GError *failure = NULL;
    GFile *file;
    int i, depth = 0, ok;
    if (!target || count < 0 || count > 4096 || (count && !rows)) return fail(error, capacity, "Invalid list.");
    current = revision_for(kind); ok = revision && !strcmp(current, revision); g_free(current);
    if (!ok) return fail(error, capacity, "This list changed while the editor was open. Reopen it before saving.");
    for (i = 0; i < count; ++i) {
        const char *name = rows[i].name, *cmd = rows[i].command;
        if (!name || !*name || !cmd || strlen(name) > 127 || strlen(cmd) > 383 || strpbrk(name, "\r\n") || strpbrk(cmd, "\r\n"))
            return fail(error, capacity, "Names and commands must be single-line, at most 127 and 383 UTF-8 bytes respectively.");
        if ((kind == QT_CONFIG_REPLACE || kind == QT_CONFIG_COMMANDS || kind == QT_CONFIG_CTCP) && strpbrk(name, " \t"))
            return fail(error, capacity, "Names for replacements, commands and CTCP replies must be single words.");
        if (kind == QT_CONFIG_USERMENU || kind == QT_CONFIG_POPUP || kind == QT_CONFIG_TAB_POPUP || kind == QT_CONFIG_URL_HANDLERS) {
            if (!g_ascii_strcasecmp(name, "SUB")) { if (!*cmd || ++depth > 16) return fail(error, capacity, "Submenus need labels and may nest at most 16 levels."); }
            else if (!g_ascii_strcasecmp(name, "ENDSUB") && --depth < 0) return fail(error, capacity, "ENDSUB has no matching SUB.");
        }
    }
    if (depth) return fail(error, capacity, "Each SUB must have a matching ENDSUB.");
    for (i = 0; i < count; ++i) list_addentry(&replacement, (char *)rows[i].command, (char *)rows[i].name);
    text = serialize(replacement); path = config_path(kind); file = g_file_new_for_path(path);
    ok = g_file_replace_contents(file, text->str, text->len, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL, &failure);
    g_object_unref(file); g_string_free(text, TRUE); g_free(path);
    if (!ok) { list_free(&replacement); fail(error, capacity, failure ? failure->message : "Unable to save configuration."); g_clear_error(&failure); return FALSE; }
    list_free(target); *target = replacement;
    qt_fe_customization_changed(kind); return TRUE;
}
const char *qt_core_replace_word(const char *word)
{
    GSList *link;
    if (!word) return NULL;
    for (link = replace_list; link; link = link->next) { struct popup *row = link->data; if (!strcmp(row->name, word)) return row->cmd; }
    return NULL;
}
int qt_core_custom_pref(const char *name)
{
    return name && *name && !strpbrk(name, " \t\r\n") ? cfg_get_bool((char *)name) : -1;
}
int qt_core_custom_run_target(void *ptr, const char *command, const char *target, const char *all_targets)
{
    session *sess = ptr;
    char expanded[8192];
    char *copy, *start;
    struct User *user;
    char *host = "", *account = "";
    if (!sess || !is_session(sess) || !command || strlen(command) > 4096 || strpbrk(command, "\r\n")) return FALSE;
    if (!target || !all_targets || strlen(target) > 4096 || strlen(all_targets) > 8192 ||
        strpbrk(target, "\r\n") || strpbrk(all_targets, "\r\n")) return FALSE;
    user = *target && sess->type == SESS_CHANNEL ? userlist_find(sess, target) : NULL;
    if (user) {
        if (user->hostname) { char *at = strchr(user->hostname, '@'); host = at ? at + 1 : user->hostname; }
        if (user->account) account = user->account;
    }
    copy = g_strdup(command);
    if (!auto_insert(expanded, sizeof(expanded), (unsigned char *)copy, NULL, NULL, (char *)all_targets, sess->channel, "",
                     server_get_network(sess->server, TRUE), host, sess->server->nick, (char *)target, account)) { g_free(copy); return FALSE; }
    g_free(copy); qt_core_activate(sess); start = expanded;
    if (*start == '/') ++start;
    if (*start == '!') zoitechat_exec(start + 1); else handle_command(sess, start, TRUE);
    return TRUE;
}
int qt_core_custom_run(void *ptr, const char *command) { return qt_core_custom_run_target(ptr, command, "", ""); }
static GHashTable *menu_markup;
void qt_core_menu_removed(void *identity)
{
    if (menu_markup) {
        g_hash_table_remove(menu_markup, identity);
        if (!g_hash_table_size(menu_markup)) { g_hash_table_destroy(menu_markup); menu_markup = NULL; }
    }
}
char *qt_core_menu_added(void *identity)
{
    menu_entry *entry = identity;
    qt_core_menu_removed(identity);
    char *plain = NULL;
    if (!entry || !entry->markup || !entry->label || !pango_parse_markup(entry->label, -1, 0, NULL, &plain, NULL, NULL)) return NULL;
    if (!menu_markup) menu_markup = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, g_free);
    g_hash_table_insert(menu_markup, entry, g_strdup(entry->label));
    return plain;
}
void qt_core_menus_foreach(void (*visitor)(const QtDynamicMenu *, void *), void *data)
{
    GSList *link;
    if (!visitor) return;
    for (link = menu_list; link; link = link->next) {
        menu_entry *entry = link->data;
        QtDynamicMenu row = {entry, entry->path, entry->label, entry->cmd, entry->ucmd, entry->group, entry->is_main,
            entry->pos, entry->state, entry->enable, entry->key, entry->modifier, entry->icon, menu_markup ? g_hash_table_lookup(menu_markup, entry) : NULL};
        char *candidate = NULL;
        if (entry->icon && *entry->icon && !g_file_test(entry->icon, G_FILE_TEST_IS_REGULAR)) {
            candidate = g_build_filename(get_xdir(), entry->icon, NULL);
            if (g_file_test(candidate, G_FILE_TEST_IS_REGULAR)) row.icon = candidate;
        }
        visitor(&row, data);
        g_free(candidate);
    }
}
int qt_core_menu_activate_target(void *ptr, void *identity, const char *expected_command, int checked, const char *target, const char *all_targets)
{
    menu_entry *entry = identity;
    char *command;
    if (!ptr || !is_session(ptr) || !identity || !g_slist_find(menu_list, identity) || !entry->enable || g_strcmp0(entry->cmd ? entry->cmd : "", expected_command ? expected_command : "")) return FALSE;
    if (entry->group || entry->ucmd) {
        if (entry->group && checked) {
            GSList *link;
            for (link = menu_list; link; link = link->next) {
                menu_entry *other = link->data;
                if (other != entry && other->group && !g_strcmp0(other->path, entry->path) &&
                    (!g_strcmp0(other->group, entry->group) || !g_strcmp0(other->label, entry->group) || !g_strcmp0(other->group, entry->label))) other->state = FALSE;
            }
        }
        entry->state = !!checked;
    }
    command = g_strdup(entry->ucmd && !checked ? entry->ucmd : entry->group && !checked ? "" : entry->cmd);
    qt_fe_menus_changed();
    if (!command || !*command) { g_free(command); return TRUE; }
    int ok = qt_core_custom_run_target(ptr, command, target, all_targets); g_free(command); return ok;
}
int qt_core_menu_activate(void *ptr, void *identity, const char *expected_command, int checked)
{ return qt_core_menu_activate_target(ptr, identity, expected_command, checked, "", ""); }
