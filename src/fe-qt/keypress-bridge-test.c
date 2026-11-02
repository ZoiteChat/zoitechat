/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "qt-bridge.h"
#include <stdio.h>
static session context;
static int calls, consumed;
int is_session(session *sess) { return sess == &context; }
int plugin_emit_keypress(session *sess, unsigned state, unsigned keyval, gunichar unicode)
{
    if (sess != &context || state != 5 || keyval != 'A' || unicode != 'A') return -1;
    ++calls; return consumed;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    CHECK(!qt_core_keypress(NULL, 5, 'A', 'A') && !calls);
    CHECK(!qt_core_keypress(&context, 5, 'A', 'A') && calls == 1);
    consumed = 1; CHECK(qt_core_keypress(&context, 5, 'A', 'A') == 1 && calls == 2);
    return 0;
}
