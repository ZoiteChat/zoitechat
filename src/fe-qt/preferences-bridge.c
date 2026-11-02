/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/zoitechatc.h"
#include "../common/cfgfiles.h"
#include "../common/text.h"
#include "preferences-bridge.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
struct Setting { const char *key, *group, *label, *choices; size_t offset, size; int type, maximum; };
#define S(g,f,l,m,c) {#f + 4,g,l,c,offsetof(struct zoitechatprefs,f),sizeof(prefs.f),0,m},
#define I(g,f,l,m,c) {#f + 4,g,l,c,offsetof(struct zoitechatprefs,f),sizeof(prefs.f),1,m},
#define B(g,f,l,m,c) {#f + 4,g,l,c,offsetof(struct zoitechatprefs,f),sizeof(prefs.f),2,1},
static const struct Setting settings[] = {
#include "preferences.def"
};
#undef S
#undef I
#undef B
int qt_core_preference_count(void) { return G_N_ELEMENTS(settings); }
int qt_core_preference_read(int index, struct QtPreference *result)
{
    if (!result || index < 0 || index >= qt_core_preference_count()) return 0;
    const struct Setting *s = &settings[index];
    result->key = s->key; result->group = s->group; result->label = s->label; result->choices = s->choices;
    result->type = s->type; result->maximum = s->maximum;
    if (!s->type) g_strlcpy(result->value, (char *)&prefs + s->offset, sizeof(result->value));
    else g_snprintf(result->value, sizeof(result->value), "%d", *(int *)((char *)&prefs + s->offset));
    return 1;
}
static int fail(char *error, int capacity, const char *message)
{ if (error && capacity > 0) g_strlcpy(error, message, capacity); return 0; }
int qt_core_preferences_apply(const struct QtPreferenceChange *changes, int count, char *error, int capacity)
{
    if (count < 0 || count > qt_core_preference_count() || (count && !changes)) return fail(error, capacity, "Invalid settings request.");
    if (!count) return 1;
    for (int i = 0; i < count; ++i) {
        struct QtPreference current;
        if (!qt_core_preference_read(changes[i].index, &current) ||
            !memchr(changes[i].expected, 0, sizeof(changes[i].expected)) ||
            !memchr(changes[i].value, 0, sizeof(changes[i].value))) return fail(error, capacity, "Invalid setting.");
        for (int j = 0; j < i; ++j) if (changes[j].index == changes[i].index) return fail(error, capacity, "Duplicate setting.");
        if (strcmp(current.value, changes[i].expected)) return fail(error, capacity, "Settings changed while this dialog was open. Cancel and reopen Preferences.");
        const struct Setting *s = &settings[changes[i].index]; const char *value = changes[i].value;
        if (!s->type) {
            if (!strcmp(s->key, "input_command_char") && (strlen(value) != 1 || (unsigned char)value[0] < 33 || (unsigned char)value[0] > 126)) return fail(error, capacity, "The command prefix must be one printable ASCII character.");
            if (!strcmp(s->key, "completion_suffix") && g_utf8_validate(value, -1, NULL) && g_utf8_strlen(value, -1) > 1) return fail(error, capacity, "The completion suffix must be one character or empty.");
            if (strlen(value) >= s->size || !g_utf8_validate(value, -1, NULL) || strpbrk(value, "\r\n")) return fail(error, capacity, "A text setting is too long or contains invalid characters.");
        } else {
            char *end; errno = 0; long number = strtol(value, &end, 10);
            if (!*value || *end || errno || number < 0 || number > s->maximum) return fail(error, capacity, "A numeric setting is outside its allowed range.");
        }
    }
    /* Validate the resulting listen range even when only one endpoint changed. */
    int first = prefs.hex_dcc_port_first, last = prefs.hex_dcc_port_last, range_changed = 0;
    for (int i = 0; i < count; ++i) {
        const char *key = settings[changes[i].index].key;
        if (!strcmp(key, "dcc_port_first")) { first = atoi(changes[i].value); range_changed = 1; }
        if (!strcmp(key, "dcc_port_last")) { last = atoi(changes[i].value); range_changed = 1; }
    }
    if (range_changed && (first || last) && (!first || last < first)) return fail(error, capacity, "DCC listen ports must both be zero, or form an ascending nonzero range.");
    struct zoitechatprefs previous = prefs;
    for (int i = 0; i < count; ++i) {
        const struct Setting *s = &settings[changes[i].index];
        if (!s->type) g_strlcpy((char *)&prefs + s->offset, changes[i].value, s->size);
        else *(int *)((char *)&prefs + s->offset) = atoi(changes[i].value);
    }
    gboolean reposition = FALSE;
    for (int i = 0; i < count; ++i) {
        const char *key = settings[changes[i].index].key;
        if (!strcmp(key, "gui_tab_layout") || !strcmp(key, "gui_tab_pos") || !strcmp(key, "gui_ulist_pos")) reposition = TRUE;
    }
    if (reposition) {
        prefs.hex_gui_tab_pos = CLAMP(prefs.hex_gui_tab_pos, 1, 7);
        prefs.hex_gui_ulist_pos = CLAMP(prefs.hex_gui_ulist_pos, 1, 4);
        if (prefs.hex_gui_tab_layout == 2 && (prefs.hex_gui_tab_pos == 5 || prefs.hex_gui_tab_pos == 6)) prefs.hex_gui_tab_pos = 1;
        if (prefs.hex_gui_tab_pos == prefs.hex_gui_ulist_pos) prefs.hex_gui_tab_pos = prefs.hex_gui_ulist_pos == 3 ? 4 : 3;
    }
    if (!save_config()) { prefs = previous; return fail(error, capacity, "Could not save zoitechat.conf. Changes were not applied."); }
    if (previous.hex_gui_lagometer != prefs.hex_gui_lagometer || previous.hex_gui_throttlemeter != prefs.hex_gui_throttlemeter ||
        previous.hex_away_track != prefs.hex_away_track || previous.hex_notify_timeout != prefs.hex_notify_timeout)
        zoitechat_reinit_timers();
    if (previous.hex_irc_logging != prefs.hex_irc_logging || strcmp(previous.hex_irc_logmask, prefs.hex_irc_logmask))
        for (GSList *link = sess_list; link; link = link->next) log_open_or_close(link->data);
    return 1;
}
