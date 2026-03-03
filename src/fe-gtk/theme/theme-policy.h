#ifndef ZOITECHAT_THEME_POLICY_H
#define ZOITECHAT_THEME_POLICY_H

#include <glib.h>

gboolean theme_policy_system_prefers_dark (void);
gboolean theme_policy_is_dark_mode_active (unsigned int mode);
gboolean theme_policy_is_app_dark_mode_active (void);

#endif
