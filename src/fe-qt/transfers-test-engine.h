/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TRANSFERS_TEST_ENGINE_H
#define ZOITECHAT_QT_TRANSFERS_TEST_ENGINE_H
#include "transfers-bridge.h"
#include <QList>
#include <QString>
#include <cstring>
namespace TransfersTestEngine {
struct Row { uint64_t id; int type, state; QString nick, file, destination; uint64_t size = 100, position = 50, resumable = 0; bool pending = false; };
QList<Row> rows;
uint64_t lastId = 0;
int lastAction = -1;
int sendRequests = 0;
int chatRequests = 0;
}
extern "C" {
void qt_core_transfers_read(void (*visitor)(const QtTransfer *, void *), void *data) {
    for (const auto &row : TransfersTestEngine::rows) {
        const auto nick = row.nick.toUtf8(), file = row.file.toUtf8(), destination = row.destination.toUtf8();
        const QtTransfer entry{row.id, row.type, row.state, nick.constData(), file.constData(), destination.constData(), row.size, row.position, row.resumable, 32, row.pending}; visitor(&entry, data);
    }
}
int qt_core_transfer_action(uint64_t id, int action, const char *, char *error, int capacity) {
    for (const auto &row : TransfersTestEngine::rows) if (row.id == id) { TransfersTestEngine::lastId = id; TransfersTestEngine::lastAction = action; return 1; }
    std::strncpy(error, "Transfer no longer exists.", capacity); return 0;
}
int qt_core_direct_chat_start(void *, const char *, int, char *, int) { ++TransfersTestEngine::chatRequests; return 1; }
int qt_core_transfer_send(void *, const char *, const char *, int, int, char *, int) { ++TransfersTestEngine::sendRequests; return 1; }
}
#endif
