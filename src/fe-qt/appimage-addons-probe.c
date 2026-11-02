/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "zoitechat-plugin.h"
#include "addons-bridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static zoitechat_plugin *plugin;
static int checksum, fishlim, outside;
static void visit(const struct QtAddon *row, void *unused)
{
    const char *root = getenv("APPDIR");
    (void)unused;
    if (!row->filename || row->script || row->builtin) return;
    if (strstr(row->filename, "/checksum.so")) ++checksum;
    else if (strstr(row->filename, "/fishlim.so")) ++fishlim;
    else return;
    if (strncmp(row->filename, root, strlen(root))) ++outside;
}
static int tick(void *unused)
{
    const char *root = getenv("APPDIR");
    const char *libdir = plugin->zoitechat_get_info(plugin, "libdirfs");
    FILE *out;
    (void)unused;
    if (!root || !*root || !libdir) return 0;
    qt_core_addons_read(visit, NULL);
    out = fopen(getenv("QT_APPIMAGE_ADDONS_REPORT"), "w");
    if (out) {
        fprintf(out, "%s checksum=%d fishlim=%d outside=%d libdir=%s\n",
                checksum == 1 && fishlim == 1 && !outside && !strncmp(libdir, root, strlen(root)) ? "PASS" : "FAIL",
                checksum, fishlim, outside, libdir);
        fclose(out);
    }
    return 0;
}
int zoitechat_plugin_init(zoitechat_plugin *handle, char **name, char **description, char **version, char *argument)
{
    (void)argument;
    plugin = handle; *name = "AppImage addon audit"; *description = "Bundled plugin discovery"; *version = "1";
    handle->zoitechat_hook_timer(handle, 100, tick, NULL);
    return 1;
}
