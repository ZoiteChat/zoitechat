/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "../common/zoitechat.h"
#include "../common/text.h"
#include "../common/cfgfiles.h"
#include "textevents-bridge.h"
#include <glib/gstdio.h>
#include <stdio.h>
#include <string.h>
struct text_event te[NUM_XP];
char *pntevts_text[NUM_XP], *pntevts[NUM_XP], *sound_files[NUM_XP];
static int played;
void sound_play(const char *file, gboolean quiet) { (void)file; (void)quiet; ++played; }
static char *directory;
char *get_xdir(void) { return directory; }
int pevt_build_string(const char *input, char **output, int *maximum)
{
    if (strstr(input, "bad")) { qt_core_text_event_capture_message("Invalid substitution."); return 1; }
    *maximum = strstr(input, "$9") ? 9 : 1; *output = g_strdup(input); return 0;
}
void format_event(session *sess, int index, char **args, char *output, gsize capacity, unsigned int strip)
{ (void)sess; (void)args; (void)strip; g_strlcpy(output, pntevts[index], capacity); }
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void)
{
    directory = g_dir_make_tmp("zoitechat-qt-events-XXXXXX", NULL); CHECK(directory);
    static char *help[] = {"Nickname", "Message", NULL};
    const char *texts[NUM_XP];
    for (int i = 0; i < NUM_XP; ++i) {
        te[i].name = "Event"; te[i].def = "default $1"; te[i].num_args = 2; te[i].help = help;
        pntevts_text[i] = g_strdup("old $1"); pntevts[i] = g_strdup("compiled old"); texts[i] = "new $1";
    }
    char revision[65], previous[65], error[1024], preview[8192];
    CHECK(qt_core_text_events(NULL, NULL, revision, sizeof(revision)) == NUM_XP); strcpy(previous, revision);
    CHECK(qt_core_text_event_preview(0, "draft $1", preview, sizeof(preview), error, sizeof(error)));
    CHECK(!strcmp(preview, "draft $1") && !strcmp(pntevts[0], "compiled old"));
    CHECK(!qt_core_text_event_preview(-1, "draft", preview, sizeof(preview), error, sizeof(error)));
    texts[NUM_XP - 1] = "bad";
    CHECK(!qt_core_text_events_save(texts, NUM_XP, revision, error, sizeof(error)) && !strcmp(error, "Invalid substitution."));
    CHECK(!strcmp(pntevts_text[0], "old $1"));
    texts[NUM_XP - 1] = "$9";
    CHECK(!qt_core_text_events_save(texts, NUM_XP, revision, error, sizeof(error)));
    texts[NUM_XP - 1] = "new $1";
    CHECK(qt_core_text_events_save(texts, NUM_XP, revision, error, sizeof(error)));
    CHECK(!strcmp(pntevts_text[0], "new $1") && !strcmp(pntevts[0], "new $1"));
    CHECK(!qt_core_text_events_save(texts, NUM_XP, previous, error, sizeof(error)));
    char *path = g_build_filename(directory, "pevents.conf", NULL), *contents;
    CHECK(g_file_get_contents(path, &contents, NULL, NULL) && strstr(contents, "event_text=new $1")); g_free(contents);
    CHECK(qt_core_text_events(NULL, NULL, revision, sizeof(revision)));
    CHECK(g_file_set_contents(path, "external change", -1, NULL));
    CHECK(!qt_core_text_events_save(texts, NUM_XP, revision, error, sizeof(error)));
    char *saved = directory; directory = g_build_filename(saved, "missing", "child", NULL);
    CHECK(qt_core_text_events(NULL, NULL, revision, sizeof(revision)));
    texts[0] = "not saved";
    CHECK(!qt_core_text_events_save(texts, NUM_XP, revision, error, sizeof(error)) && !strcmp(pntevts_text[0], "new $1"));
    g_free(directory); directory = saved;
    CHECK(!qt_core_text_event_capture_message("unrelated"));
    for (int i = 0; i < NUM_XP; ++i) { g_free(pntevts_text[i]); g_free(pntevts[i]); }
    for (int i = 0; i < NUM_XP; ++i) texts[i] = "";
    texts[0] = "one.wav";
    CHECK(qt_core_sounds_read(NULL, NULL, revision, sizeof(revision)) == NUM_XP); strcpy(previous, revision);
    CHECK(qt_core_sounds_save(texts, NUM_XP, revision, error, sizeof(error)) && !strcmp(sound_files[0], "one.wav"));
    CHECK(!qt_core_sounds_save(texts, NUM_XP, previous, error, sizeof(error)));
    CHECK(qt_core_sounds_read(NULL, NULL, revision, sizeof(revision)));
    texts[0] = "invalid\nsound";
    CHECK(!qt_core_sounds_save(texts, NUM_XP, revision, error, sizeof(error)) && !strcmp(sound_files[0], "one.wav"));
    CHECK(!qt_core_sound_preview("missing.wav", error, sizeof(error)) && played == 0);
    char *sound_dir = g_build_filename(directory, ZOITECHAT_SOUND_DIR, NULL);
    CHECK(!g_mkdir_with_parents(sound_dir, 0700));
    char *sound_path = g_build_filename(sound_dir, "one.wav", NULL);
    CHECK(g_file_set_contents(sound_path, "test", -1, NULL));
    CHECK(qt_core_sound_preview("one.wav", error, sizeof(error)) && played == 1);
    CHECK(qt_core_sound_preview(sound_path, error, sizeof(error)) && played == 2);
    char *sound_config = g_build_filename(directory, "sound.conf", NULL);
    CHECK(g_file_get_contents(sound_config, &contents, NULL, NULL) && strstr(contents, "sound=one.wav")); g_free(contents);
    for (int i = 0; i < NUM_XP; ++i) g_free(sound_files[i]);
    g_remove(sound_path); g_rmdir(sound_dir); g_remove(sound_config);
    g_free(sound_path); g_free(sound_dir); g_free(sound_config);
    g_remove(path); g_rmdir(directory); g_free(directory); g_free(path); return 0;
}
