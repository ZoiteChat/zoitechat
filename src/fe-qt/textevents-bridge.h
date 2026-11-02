/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_TEXTEVENTS_BRIDGE_H
#define ZOITECHAT_QT_TEXTEVENTS_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
int qt_core_sounds_read(void (*visitor)(int, const char *, const char *, void *), void *data, char *revision, int capacity);
int qt_core_sounds_save(const char *const *files, int count, const char *revision, char *error, int capacity);
int qt_core_sound_preview(const char *file, char *error, int capacity);
int qt_core_text_event_capture_message(const char *message);
struct QtTextEvent { int id; const char *name, *text, *defaults; int arguments; const char *help[9]; };
int qt_core_text_events(void (*visitor)(const struct QtTextEvent *, void *), void *data, char *revision, int capacity);
int qt_core_text_events_save(const char *const *texts, int count, const char *revision, char *error, int capacity);
int qt_core_text_event_preview(int id, const char *text, char *output, int capacity, char *error, int error_capacity);
#ifdef __cplusplus
}
#endif
#endif
