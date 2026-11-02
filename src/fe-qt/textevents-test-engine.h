/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TEXTEVENTS_TEST_ENGINE_H
#define ZOITECHAT_QT_TEXTEVENTS_TEST_ENGINE_H
#include "textevents-bridge.h"
#include <QStringList>
#include <cstring>
namespace TextEventsTestEngine { QStringList texts{QStringLiteral("<$1> $2"), QStringLiteral("joined $1")}; int revision = 1; QStringList sounds{QStringLiteral("one.wav"), QString()}; int soundRevision = 1; QString played; }
extern "C" {
int qt_core_sounds_read(void (*visitor)(int, const char *, const char *, void *), void *data, char *revision, int capacity) {
    if (capacity < 65) return 0;
    std::strcpy(revision, QByteArray::number(TextEventsTestEngine::soundRevision).constData());
    for (int i = 0; visitor && i < 2; ++i) visitor(i, i ? "Join" : "Channel Message", TextEventsTestEngine::sounds[i].toUtf8().constData(), data);
    return 2;
}
int qt_core_sounds_save(const char *const *files, int count, const char *revision, char *error, int capacity) {
    if (count != 2 || QByteArray(revision) != QByteArray::number(TextEventsTestEngine::soundRevision)) { std::strncpy(error, "Sounds changed. Reload.", capacity); return 0; }
    for (int i = 0; i < count; ++i) TextEventsTestEngine::sounds[i] = QString::fromUtf8(files[i]);
    ++TextEventsTestEngine::soundRevision; return 1;
}
int qt_core_sound_preview(const char *file, char *, int) { TextEventsTestEngine::played = QString::fromUtf8(file); return 1; }

int qt_core_text_events(void (*visitor)(const QtTextEvent *, void *), void *data, char *revision, int capacity)
{
    if (capacity < 65) return 0;
    std::strcpy(revision, QByteArray::number(TextEventsTestEngine::revision).constData());
    for (int i = 0; visitor && i < 2; ++i) {
        const QByteArray text = TextEventsTestEngine::texts[i].toUtf8();
        QtTextEvent row{i, i ? "Join" : "Channel Message", text.constData(), i ? "joined $1" : "<$1> $2", i ? 1 : 2, {"Nickname", "Message"}};
        visitor(&row, data);
    }
    return 2;
}
int qt_core_text_events_save(const char *const *texts, int count, const char *revision, char *error, int capacity)
{
    if (count != 2 || QByteArray(revision) != QByteArray::number(TextEventsTestEngine::revision)) { std::strncpy(error, "Text events changed. Reload before saving.", capacity); return 0; }
    for (int i = 0; i < count; ++i) if (QByteArray(texts[i]).contains("$9")) { std::strncpy(error, "Invalid argument.", capacity); return 0; }
    for (int i = 0; i < count; ++i) TextEventsTestEngine::texts[i] = QString::fromUtf8(texts[i]);
    ++TextEventsTestEngine::revision; return 1;
}
int qt_core_text_event_preview(int, const char *text, char *output, int capacity, char *, int)
{ const QByteArray result = QString::fromUtf8(text).replace(QStringLiteral("$1"), QStringLiteral("example1")).replace(QStringLiteral("$2"), QStringLiteral("example2")).toUtf8(); std::strncpy(output, result.constData(), capacity); return 1; }
}
#endif
