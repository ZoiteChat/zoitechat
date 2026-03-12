#ifndef ZOITECHAT_THEME_APPLICATION_H
#define ZOITECHAT_THEME_APPLICATION_H

#include "../fe-gtk.h"

gboolean theme_application_apply_mode (unsigned int mode, gboolean *palette_changed);
void theme_application_reload_input_style (void);
InputStyle *theme_application_update_input_style (InputStyle *style);

#endif
