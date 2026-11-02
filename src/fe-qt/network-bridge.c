/*
 * Qt network manager adapter to ZoiteChat's existing servlist API.
 * Never save a parallel config format. C owns all live ircnet pointers.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../common/zoitechat.h"
#include <glib.h>
#include "../common/servlist.h"
#include "qt-network.h"

static ircnet *find_live(void *identity)
{
    return identity && g_slist_find(network_list, identity) ? identity : NULL;
}
static char *copy(const char *value)
{
    return value ? g_strdup(value) : NULL;
}
static char *copy_nonempty(const char *value)
{
    return value && *value ? g_strdup(value) : NULL;
}
static void wipe_free(char *value)
{
    if (value) {
        volatile char *ptr = (volatile char *)value;
        size_t n = strlen(value);
        while (n--) *ptr++ = 0;
        g_free(value);
    }
}
static void set_error(char *output, size_t size, const char *message)
{
    if (output && size) g_strlcpy(output, message, size);
}
static int single_line(const char *text)
{
    return !text || (!strchr(text, '\n') && !strchr(text, '\r'));
}
static GPtrArray *lines(const char *text)
{
    GPtrArray *array = g_ptr_array_new_with_free_func(g_free);
    gchar **parts = g_strsplit(text ? text : "", "\n", -1);
    for (size_t i = 0; parts[i]; ++i) {
        char *line = g_strstrip(g_strdup(parts[i]));
        if (*line) g_ptr_array_add(array, line);
        else g_free(line);
    }
    g_strfreev(parts);
    return array;
}
static char *collect_servers(ircnet *net)
{
    GString *s = g_string_new(NULL);
    for (GSList *item = net->servlist; item; item = item->next) {
        ircserver *entry = item->data;
        if (s->len) g_string_append_c(s, '\n');
        g_string_append(s, entry->hostname);
    }
    return g_string_free(s, FALSE);
}
static char *collect_channels(ircnet *net)
{
    GString *s = g_string_new(NULL);
    for (GSList *item = net->favchanlist; item; item = item->next) {
        favchannel *entry = item->data;
        if (s->len) g_string_append_c(s, '\n');
        g_string_append(s, entry->name);
        if (entry->key && *entry->key) {
            g_string_append_c(s, ',');
            g_string_append(s, entry->key);
        }
    }
    return g_string_free(s, FALSE);
}
static char *collect_commands(ircnet *net)
{
    GString *s = g_string_new(NULL);
    for (GSList *item = net->commandlist; item; item = item->next) {
        commandentry *entry = item->data;
        if (s->len) g_string_append_c(s, '\n');
        g_string_append(s, entry->command);
    }
    return g_string_free(s, FALSE);
}

int qt_net_count(void)
{
    return (int)g_slist_length(network_list);
}
int qt_net_read(int index, QtNetwork *out)
{
    ircnet *net = g_slist_nth_data(network_list, index);
    if (!net || !out) return 0;
    memset(out, 0, sizeof(*out));
    out->identity = net;
    out->name = copy(net->name);
    out->nick = copy(net->nick);
    out->nick2 = copy(net->nick2);
    out->user = copy(net->user);
    out->real = copy(net->real);
    out->password = copy(net->pass);
    out->encoding = copy(net->encoding);
    out->servers = collect_servers(net);
    out->channels = collect_channels(net);
    out->commands = collect_commands(net);
    out->login_type = net->logintype;
    out->flags = net->flags;
    out->selected = net->selected;
    return 1;
}
void qt_net_release(QtNetwork *out)
{
    if (!out) return;
    g_free(out->name);
    g_free(out->nick);
    g_free(out->nick2);
    g_free(out->user);
    g_free(out->real);
    wipe_free(out->password);
    g_free(out->encoding);
    g_free(out->servers);
    g_free(out->channels);
    g_free(out->commands);
    memset(out, 0, sizeof(*out));
}

static int valid_server(const char *entry)
{
    const char *separator = strchr(entry, '/');
    if (!*entry || strchr(entry, ' ') || strchr(entry, '\t') ||
        strchr(entry, '=') || strchr(entry, '\r'))
        return 0;
    if (!separator) return 1;
    if (separator == entry || strchr(separator + 1, '/')) return 0;
    const char *port = separator + 1;
    if (*port == '+') ++port;
    if (!*port) return 0;
    unsigned long number = 0;
    for (; *port; ++port) {
        if (!g_ascii_isdigit(*port)) return 0;
        number = number * 10 + (*port - '0');
        if (number > 65535) return 0;
    }
    return number > 0;
}

int qt_net_store(const QtNetwork *input, void **saved, char *error, size_t error_size)
{
    if (saved) *saved = NULL;
    set_error(error, error_size, "");
    if (!input || !input->name || !single_line(input->name) ||
        !single_line(input->nick) || !single_line(input->nick2) ||
        !single_line(input->user) || !single_line(input->real) ||
        !single_line(input->password) || !single_line(input->encoding)) {
        set_error(error, error_size, "Fields may not contain line breaks.");
        return 0;
    }

    char *name = g_strdup(input->name);
    g_strstrip(name);
    if (!*name || strlen(name) > 120 || strchr(name, '=')) {
        set_error(error, error_size, "Enter a network name (at most 120 characters, no '=').");
        g_free(name);
        return 0;
    }

    ircnet *old = find_live(input->identity);
    if (input->identity && !old) {
        set_error(error, error_size, "This network no longer exists. Reopen the editor.");
        g_free(name);
        return 0;
    }
    ircnet *duplicate = servlist_net_find(name, NULL, g_ascii_strcasecmp);
    if (duplicate && duplicate != old) {
        set_error(error, error_size, "A network with that name already exists.");
        g_free(name);
        return 0;
    }

    GPtrArray *servers = lines(input->servers);
    GPtrArray *channels = lines(input->channels);
    GPtrArray *commands = lines(input->commands);
    const char *failure = NULL;
    if (servers->len == 0) failure = "Add at least one IRC server.";
    for (guint i = 0; !failure && i < servers->len; i++) {
        if (!valid_server(g_ptr_array_index(servers, i)))
            failure = "Servers must be hostnames with an optional /port or /+TLS-port.";
    }
    for (guint i = 0; !failure && i < channels->len; i++) {
        const char *entry = g_ptr_array_index(channels, i);
        if (strlen(entry) > 400 || strchr(entry, '=') ||
            strchr(entry, ' ') || strchr(entry, '\t') ||
            strchr(entry, '\r') || entry[0] == ',')
            failure = "Auto-join entries must be channel[,key], one per line.";
    }
    for (guint i = 0; !failure && i < commands->len; i++) {
        const char *entry = g_ptr_array_index(commands, i);
        if (strlen(entry) > 1000 || strchr(entry, '\r'))
            failure = "Invalid connect command.";
    }
    if (!failure && input->encoding && *input->encoding) {
        char *charset = g_strdup(input->encoding);
        if (!servlist_check_encoding(charset))
            failure = "Unknown character encoding.";
        g_free(charset);
    }
    if (!failure && !(input->flags & QT_NET_GLOBAL_IDENTITY) &&
        (!input->nick || !*input->nick))
        failure = "Set a nickname or enable global identity.";

    if (failure) {
        set_error(error, error_size, failure);
        g_ptr_array_free(servers, TRUE);
        g_ptr_array_free(channels, TRUE);
        g_ptr_array_free(commands, TRUE);
        g_free(name);
        return 0;
    }

    ircnet *net = old ? old : servlist_net_add(name, NULL, FALSE);
    g_free(net->name);
    net->name = name;
#define REPLACE(field, value) do { g_free(net->field); net->field = copy_nonempty(value); } while (0)
    REPLACE(nick, input->nick);
    REPLACE(nick2, input->nick2);
    REPLACE(user, input->user);
    REPLACE(real, input->real);
    REPLACE(encoding, input->encoding);
#undef REPLACE
    wipe_free(net->pass);
    net->pass = copy_nonempty(input->password);

    while (net->servlist) servlist_server_remove(net, net->servlist->data);
    while (net->favchanlist) servlist_favchan_remove(net, net->favchanlist->data);
    while (net->commandlist) servlist_command_remove(net, net->commandlist->data);

    for (guint i = 0; i < servers->len; i++)
        servlist_server_add(net, g_ptr_array_index(servers, i));
    for (guint i = 0; i < channels->len; i++)
        servlist_favchan_add(net, g_ptr_array_index(channels, i));
    for (guint i = 0; i < commands->len; i++)
        servlist_command_add(net, g_ptr_array_index(commands, i));

    net->flags = input->flags & 127;
    net->logintype = input->login_type;
    net->selected = input->selected >= 0 && input->selected < (int)servers->len
                    ? input->selected : 0;
    g_ptr_array_free(servers, TRUE);
    g_ptr_array_free(channels, TRUE);
    g_ptr_array_free(commands, TRUE);
    if (saved) *saved = net;
    if (!servlist_save()) {
        set_error(error, error_size,
                  "The changes are in memory, but writing servlist.conf failed.");
        return 0;
    }
    return 1;
}

int qt_net_delete(void *identity)
{
    ircnet *net = find_live(identity);
    if (!net) return 0;
    servlist_net_remove(net);
    return servlist_save();
}
int qt_net_connect(void *identity)
{
    ircnet *net = find_live(identity);
    if (!net || !net->servlist) return 0;
    servlist_connect(NULL, net, TRUE);
    return 1;
}
