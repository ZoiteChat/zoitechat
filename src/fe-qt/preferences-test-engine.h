/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "preferences-bridge.h"
namespace PreferencesTestEngine {
#define S(g,f,l,m,c) {#f + 4,g,l,c,0,m,""},
#define I(g,f,l,m,c) {#f + 4,g,l,c,1,m,"0"},
#define B(g,f,l,m,c) {#f + 4,g,l,c,2,1,"0"},
QtPreference rows[] = {
#include "preferences.def"
};
#undef S
#undef I
#undef B
int saves = 0;
bool saveOk = true;
}
extern "C" {
int qt_core_preference_count(void) { return sizeof(PreferencesTestEngine::rows) / sizeof(QtPreference); }
int qt_core_preference_read(int index, QtPreference *result) {
    if (index < 0 || index >= qt_core_preference_count()) return 0;
    *result = PreferencesTestEngine::rows[index]; return 1;
}
int qt_core_preferences_apply(const QtPreferenceChange *changes, int count, char *error, int capacity) {
    if (!PreferencesTestEngine::saveOk) { qstrncpy(error, "Simulated save failure", capacity); return 0; }
    for (int i = 0; i < count; ++i) if (QByteArray(PreferencesTestEngine::rows[changes[i].index].value) != changes[i].expected) {
        qstrncpy(error, "Settings changed; reopen Preferences.", capacity); return 0;
    }
    for (int i = 0; i < count; ++i) qstrncpy(PreferencesTestEngine::rows[changes[i].index].value, changes[i].value, QT_PREFERENCE_CAPACITY);
    if (count) ++PreferencesTestEngine::saves;
    return 1;
}
}
