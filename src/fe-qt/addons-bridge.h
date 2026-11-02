/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef ZOITECHAT_QT_ADDONS_BRIDGE_H
#define ZOITECHAT_QT_ADDONS_BRIDGE_H
#ifdef __cplusplus
extern "C" {
#endif
struct QtAddon { void *identity; const char *name, *version, *description, *filename; int script, builtin; };
int qt_core_addons_enabled(void);
void qt_core_addons_read(void (*visitor)(const struct QtAddon *, void *), void *data);
// action: 0 load, 1 unload, 2 reload. Identity/name/file are snapshots for existing rows.
int qt_core_addon_action(void *session, int action, void *identity, const char *name, const char *file, char *error, int capacity);
void qt_fe_addons_changed(void);
#ifdef __cplusplus
}
#endif
#endif
