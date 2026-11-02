/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "qt-bridge.h"
#include <stdio.h>
#include <string.h>
static session context;
static int activated, called;
static char received[256];
int is_session(session *sess) { return sess == &context; }
void qt_core_activate(void *sess) { if (sess == &context) ++activated; }
int handle_command(session *sess, char *text, int check) { (void)sess; (void)check; ++called; g_strlcpy(received, text, sizeof(received)); return 1; }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    qt_core_submit_command(&context, "/close"); CHECK(called == 1 && activated == 1 && !strcmp(received, "close"));
    qt_core_submit_command(&context, "join #test"); CHECK(called == 2 && !strcmp(received, "join #test"));
    qt_core_submit_command(NULL, "/quit"); qt_core_submit_command(&context, "/"); qt_core_submit_command(&context, "/echo a\nquit");
    CHECK(called == 2); return 0;
}
