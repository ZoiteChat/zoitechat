#ifndef ZOITECHAT_THEME_PREFERENCES_H
#define ZOITECHAT_THEME_PREFERENCES_H

#include <gtk/gtk.h>

#include "theme-access.h"
#include "../fe-gtk.h"
#include "../../common/zoitechat.h"

GtkWidget *theme_preferences_create_page (GtkWindow *parent, gboolean *color_change_flag);
GtkWidget *theme_preferences_create_color_page (GtkWindow *parent,
                                                struct zoitechatprefs *setup_prefs,
                                                gboolean *color_change_flag);
void theme_preferences_apply_to_session (session_gui *gui, InputStyle *input_style);

#endif
