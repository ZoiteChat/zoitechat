/*
 * Plain C data-transfer interface for ZoiteChat's saved IRC networks.
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * qt_net_read() allocates the string fields; call qt_net_release().
 * For qt_net_store(), the caller owns the strings and they are copied.
 */
#ifndef ZOITECHAT_QT_NETWORK_H
#define ZOITECHAT_QT_NETWORK_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct QtNetwork {
    void *identity; /* opaque ircnet *; only valid while present in network_list */
    char *name;
    char *servers;      /* newline-separated hostname[/+port] entries */
    char *channels;     /* newline-separated channel[,key] entries */
    char *commands;     /* newline-separated connect commands */
    char *nick;
    char *nick2;
    char *user;
    char *real;
    char *password;
    char *encoding;
    int login_type;
    unsigned flags;
    int selected;       /* zero-based preferred server index */
} QtNetwork;

enum {
    QT_NET_CYCLE = 1,
    QT_NET_GLOBAL_IDENTITY = 2,
    QT_NET_TLS = 4,
    QT_NET_AUTOCONNECT = 8,
    QT_NET_PROXY = 16,
    QT_NET_ALLOW_INVALID_CERT = 32,
    QT_NET_FAVORITE = 64
};
enum {
    QT_NET_LOGIN_DEFAULT = 0,
    QT_NET_LOGIN_MSG_NICKSERV = 1,
    QT_NET_LOGIN_NICKSERV = 2,
    QT_NET_LOGIN_SASL = 6,
    QT_NET_LOGIN_PASS = 7,
    QT_NET_LOGIN_CHALLENGE = 8,
    QT_NET_LOGIN_CUSTOM = 9,
    QT_NET_LOGIN_SASL_EXTERNAL = 10,
    QT_NET_LOGIN_SCRAM_SHA1 = 11,
    QT_NET_LOGIN_SCRAM_SHA256 = 12,
    QT_NET_LOGIN_SCRAM_SHA512 = 13
};
int qt_net_count(void);
int qt_net_read(int index, QtNetwork *result);
void qt_net_release(QtNetwork *record);
int qt_net_store(const QtNetwork *record, void **saved,
                 char *error, size_t error_size);
int qt_net_delete(void *identity);
int qt_net_connect(void *identity);
#ifdef __cplusplus
}
#endif
#endif
