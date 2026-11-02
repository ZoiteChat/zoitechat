/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "qt-bridge.h"
#include <stdio.h>
static session detached, missing, recent, older;
static int calls, available = 1;
int qt_fe_session_attached(void *ptr) { return ptr == &recent || ptr == &older; }
session *lastact_getfirst(int (*filter)(session *))
{
    ++calls;
    if (!filter || filter(&detached) || filter(&missing) || !filter(&recent) || !filter(&older)) return &missing;
    return available ? &recent : NULL;
}
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    CHECK(qt_core_next_activity() == &recent && calls == 1);
    available = 0;
    CHECK(qt_core_next_activity() == NULL && calls == 2);
    return 0;
}
