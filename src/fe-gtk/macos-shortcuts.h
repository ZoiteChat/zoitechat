/* macOS text editing shortcuts. GPL-2.0-or-later; see COPYING. */
#ifndef ZOITECHAT_MACOS_SHORTCUTS_H
#define ZOITECHAT_MACOS_SHORTCUTS_H

#include <gtk/gtk.h>

typedef struct _GtkXText GtkXText;

void macos_shortcuts_init (void (*open_preferences) (void));
void macos_shortcuts_minimize_active (GtkWindow *fallback);
gboolean macos_shortcuts_remap_default (guint *keyval, GdkModifierType *mod,
                                      const char *action, const char *data1,
                                      const char *data2);
gboolean macos_shortcuts_copy_chat (GtkWidget *entry, GdkEventKey *event,
                                  GtkXText *xtext);

#endif
