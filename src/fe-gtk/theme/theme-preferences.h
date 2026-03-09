#ifndef ZOITECHAT_THEME_PREFERENCES_H
#define ZOITECHAT_THEME_PREFERENCES_H

#include "../fe-gtk.h"
#include "../../common/zoitechat.h"

#include "theme-access.h"

GtkWidget *theme_preferences_create_page (GtkWindow *parent,
                                                struct zoitechatprefs *setup_prefs,
                                                gboolean *color_change_flag);
GtkWidget *theme_preferences_create_color_page (GtkWindow *parent,
                                                struct zoitechatprefs *setup_prefs,
                                                gboolean *color_change_flag);
void theme_preferences_apply_to_session (session_gui *gui, InputStyle *input_style);

#endif
