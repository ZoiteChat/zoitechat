#ifndef ZOITECHAT_HCT_H
#define ZOITECHAT_HCT_H

#include <glib.h>

int zoitechat_import_hct (const char *path, char **theme_name, GError **error);

gboolean zoitechat_hct_is_file (const char *path);

#endif
