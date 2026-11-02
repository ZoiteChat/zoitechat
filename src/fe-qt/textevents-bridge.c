/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/cfgfiles.h"
#include "../common/text.h"
#include "textevents-bridge.h"
#include <gio/gio.h>
#include <string.h>
extern struct text_event te[];
extern char *pntevts_text[], *pntevts[], *sound_files[];
static char *compiler_message;
static int compiling;
int qt_core_text_event_capture_message(const char *message)
{
    if (!compiling) return 0;
    g_free(compiler_message); compiler_message = g_strdup(message); return 1;
}
static char *revision_for(int sounds)
{
    GChecksum *sum = g_checksum_new(G_CHECKSUM_SHA256);
    for (int i = 0; i < NUM_XP; ++i) {
        const char *text = sounds ? sound_files[i] : pntevts_text[i];
        if (!text) text = "";
        g_checksum_update(sum, (const guchar *)text, strlen(text) + 1);
    }
    char *path = g_build_filename(get_xdir(), sounds ? "sound.conf" : "pevents.conf", NULL), *disk = NULL;
    gsize size;
    if (g_file_get_contents(path, &disk, &size, NULL)) g_checksum_update(sum, (guchar *)disk, size);
    char *result = g_strdup(g_checksum_get_string(sum));
    g_free(path); g_free(disk); g_checksum_free(sum); return result;
}
static int fail(char *error, int capacity, const char *message)
{ if (error && capacity > 0) g_strlcpy(error, message, capacity); return 0; }
static char *compile(int id, const char *text, char *error, int capacity)
{
    char *compiled = NULL; int maximum;
    if (id < 0 || id >= NUM_XP || !text || strlen(text) > 1000 || !g_utf8_validate(text, -1, NULL) || strpbrk(text, "\r\n")) {
        fail(error, capacity, "Use a single UTF-8 line of at most 1,000 bytes."); return NULL;
    }
    compiling = 1; g_clear_pointer(&compiler_message, g_free);
    int invalid = pevt_build_string(text, &compiled, &maximum);
    compiling = 0;
    if (invalid) {
        fail(error, capacity, compiler_message ? compiler_message : "Invalid text-event format. Check the $ substitutions.");
        g_clear_pointer(&compiler_message, g_free); return NULL;
    }
    g_clear_pointer(&compiler_message, g_free);
    if (maximum > (te[id].num_args & 0x7f)) {
        g_free(compiled); fail(error, capacity, "This event does not provide that argument number."); return NULL;
    }
    return compiled;
}
int qt_core_text_events(void (*visitor)(const struct QtTextEvent *, void *), void *data, char *revision, int capacity)
{
    if (!revision || capacity < 65) return 0;
    char *value = revision_for(0); g_strlcpy(revision, value, capacity); g_free(value);
    if (visitor) for (int i = 0; i < NUM_XP; ++i) {
        struct QtTextEvent row = {i, te[i].name, pntevts_text[i],
            te[i].num_args & 128 ? te[i].def : g_dgettext(NULL, te[i].def), te[i].num_args & 0x7f, {0}};
        for (int j = 0; j < row.arguments && j < 9; ++j) {
            row.help[j] = g_dgettext(NULL, te[i].help[j]);
            if (row.help[j] && row.help[j][0] == '\001') ++row.help[j];
        }
        visitor(&row, data);
    }
    return NUM_XP;
}
int qt_core_text_events_save(const char *const *texts, int count, const char *revision, char *error, int capacity)
{
    if (!texts || count != NUM_XP || !revision) return fail(error, capacity, "Invalid event list.");
    char *current = revision_for(0); int same = !strcmp(current, revision); g_free(current);
    if (!same) return fail(error, capacity, "Text events changed. Reload before saving.");
    char **compiled = g_new0(char *, NUM_XP);
    for (int i = 0; i < NUM_XP; ++i) if (!(compiled[i] = compile(i, texts[i], error, capacity))) {
        for (int j = 0; j < i; ++j) g_free(compiled[j]);
        g_free(compiled); return 0;
    }
    GString *contents = g_string_new("");
    for (int i = 0; i < NUM_XP; ++i) g_string_append_printf(contents, "event_name=%s\nevent_text=%s\n\n", te[i].name, texts[i]);
    char *path = g_build_filename(get_xdir(), "pevents.conf", NULL);
    GFile *file = g_file_new_for_path(path); GError *failure = NULL;
    int saved = g_file_replace_contents(file, contents->str, contents->len, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL, &failure);
    g_object_unref(file); g_free(path); g_string_free(contents, TRUE);
    if (!saved) {
        fail(error, capacity, failure ? failure->message : "Could not save text events."); g_clear_error(&failure);
        for (int i = 0; i < NUM_XP; ++i) g_free(compiled[i]);
        g_free(compiled); return 0;
    }
    for (int i = 0; i < NUM_XP; ++i) {
        g_free(pntevts[i]); g_free(pntevts_text[i]); pntevts[i] = compiled[i]; pntevts_text[i] = g_strdup(texts[i]);
    }
    g_free(compiled); return 1;
}
int qt_core_text_event_preview(int id, const char *text, char *output, int capacity, char *error, int error_capacity)
{
    if (!output || capacity < 8192) return fail(error, error_capacity, "Preview buffer is too small.");
    char *compiled = compile(id, text, error, error_capacity); if (!compiled) return 0;
    char *args[11] = {0};
    for (int i = 0; i < 11; ++i) args[i] = g_strdup_printf("example%d", i - 1);
    /* format_event is synchronous and emits no callbacks or IRC traffic. */
    char *previous = pntevts[id]; pntevts[id] = compiled;
    format_event(NULL, id, args, output, capacity, 0);
    pntevts[id] = previous; g_free(compiled);
    for (int i = 0; i < 11; ++i) g_free(args[i]);
    return 1;
}

static int valid_sound(const char *file)
{
    return file && strlen(file) <= 480 && g_utf8_validate(file, -1, NULL) && !strpbrk(file, "\r\n");
}
int qt_core_sounds_read(void (*visitor)(int, const char *, const char *, void *), void *data, char *revision, int capacity)
{
    if (!revision || capacity < 65) return 0;
    char *value = revision_for(1); g_strlcpy(revision, value, capacity); g_free(value);
    if (visitor) for (int i = 0; i < NUM_XP; ++i) visitor(i, te[i].name, sound_files[i] ? sound_files[i] : "", data);
    return NUM_XP;
}
int qt_core_sounds_save(const char *const *files, int count, const char *revision, char *error, int capacity)
{
    if (!files || count != NUM_XP || !revision) return fail(error, capacity, "Invalid sound list.");
    char *current = revision_for(1); int same = !strcmp(current, revision); g_free(current);
    if (!same) return fail(error, capacity, "Sound assignments changed. Reload before saving.");
    for (int i = 0; i < NUM_XP; ++i) if (!valid_sound(files[i])) return fail(error, capacity, "Sound paths must be single UTF-8 lines of at most 480 bytes.");
    GString *contents = g_string_new("");
    for (int i = 0; i < NUM_XP; ++i) if (*files[i]) g_string_append_printf(contents, "event=%s\nsound=%s\n\n", te[i].name, files[i]);
    char *path = g_build_filename(get_xdir(), "sound.conf", NULL); GFile *file = g_file_new_for_path(path); GError *failure = NULL;
    int saved = g_file_replace_contents(file, contents->str, contents->len, NULL, FALSE, G_FILE_CREATE_PRIVATE, NULL, NULL, &failure);
    g_object_unref(file); g_free(path); g_string_free(contents, TRUE);
    if (!saved) { fail(error, capacity, failure ? failure->message : "Could not save sounds."); g_clear_error(&failure); return 0; }
    for (int i = 0; i < NUM_XP; ++i) { char *copy = g_strdup(files[i]); g_free(sound_files[i]); sound_files[i] = copy; }
    return 1;
}
int qt_core_sound_preview(const char *file, char *error, int capacity)
{
    if (!valid_sound(file) || !*file) return fail(error, capacity, "Choose a sound file first.");
    char *path = g_path_is_absolute(file) ? g_strdup(file) : g_build_filename(get_xdir(), ZOITECHAT_SOUND_DIR, file, NULL);
    GFile *input = g_file_new_for_path(path); GError *failure = NULL;
    GFileInputStream *stream = g_file_read(input, NULL, &failure);
    g_object_unref(input); g_free(path);
    if (!stream) { fail(error, capacity, failure ? failure->message : "Could not read sound file."); g_clear_error(&failure); return 0; }
    g_object_unref(stream); sound_play(file, TRUE); return 1;
}
