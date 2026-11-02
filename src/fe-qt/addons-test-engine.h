/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_ADDONS_TEST_ENGINE_H
#define ZOITECHAT_QT_ADDONS_TEST_ENGINE_H
#include "addons-bridge.h"
#include <QList>
#include <QString>
#include <cstring>
namespace AddonsTestEngine {
struct Row { void *id; QString name, file; bool script = false, builtin = false; };
QList<Row> rows;
bool enabled = true;
int actions = 0;
QString lastFile;
}
extern "C" {
int qt_core_addons_enabled(void) { return AddonsTestEngine::enabled; }
void qt_core_addons_read(void (*visitor)(const QtAddon *, void *), void *data) {
    for (const auto &row : AddonsTestEngine::rows) {
        const auto name = row.name.toUtf8(), file = row.file.toUtf8();
        QtAddon addon{row.id, name.constData(), "1.0", "Description", file.constData(), row.script, row.builtin}; visitor(&addon, data);
    }
}
int qt_core_addon_action(void *, int action, void *identity, const char *, const char *file, char *error, int capacity) {
    if (!AddonsTestEngine::enabled) return 0;
    if (action) {
        bool found = false;
        for (const auto &row : AddonsTestEngine::rows) if (row.id == identity) found = true;
        if (!found) { std::strncpy(error, "Addon no longer loaded.", capacity); return 0; }
    }
    ++AddonsTestEngine::actions; AddonsTestEngine::lastFile = QString::fromUtf8(file); return 1;
}
}
#endif
