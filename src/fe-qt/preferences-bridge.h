/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_PREFERENCES_BRIDGE_H
#define ZOITECHAT_QT_PREFERENCES_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
#define QT_PREFERENCE_CAPACITY 4096
struct QtPreference {
    const char *key, *group, *label, *choices;
    int type, maximum; /* 0 string, 1 integer, 2 boolean */
    char value[QT_PREFERENCE_CAPACITY];
};
struct QtPreferenceChange { int index; char expected[QT_PREFERENCE_CAPACITY], value[QT_PREFERENCE_CAPACITY]; };
int qt_core_preference_count(void);
int qt_core_preference_read(int index, struct QtPreference *result);
int qt_core_preferences_apply(const struct QtPreferenceChange *changes, int count, char *error, int capacity);
#ifdef __cplusplus
}
#endif
#endif
