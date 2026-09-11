#ifndef ZOITECHAT_EMOJI_FONT_H
#define ZOITECHAT_EMOJI_FONT_H

#include <gtk/gtk.h>

/* Application-private family, distinct from any system Noto version. */
#define ZOITECHAT_EMOJI_FAMILY "ZoiteChat Emoji"

/* Use the shared FreeType font map without changing the widget's font/style.
 * Children inherit it. Call before creating layouts for the widget. */
void emoji_font_apply (GtkWidget *widget);

/* Borrowed reference, or NULL if registration failed. Also usable headlessly. */
PangoFontMap *emoji_font_get_map (void);

#endif
