/* ZoiteChat
 * Copyright (C) 2026 ZoiteChat contributors.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef ZOITECHAT_INLINE_IMAGE_H
#define ZOITECHAT_INLINE_IMAGE_H

#include "xtext.h"

/* does this look like a direct http(s) link to an image file? */
gboolean inline_image_is_image_url (const char *url);

/* show the image behind 'url' inline below 'ent', or hide it again if it is
 * already shown.  Errors are reported to 'sess'. */
void inline_image_toggle (session *sess, GtkXText *xtext, textentry *ent,
								  const char *url);

/* replace image links in a message with a clickable placeholder label that
 * hides the URL.  Returns a newly allocated string, or NULL if the text has
 * no image links (or the feature is off). */
char *inline_image_filter_text (const char *text);

/* pop up a window with a larger version of the entry's inline image */
void inline_image_show_viewer (GtkXText *xtext, textentry *ent);

/* can downloads go through the proxy configured for ZoiteChat (or is no
 * proxy configured)?  FALSE means loading media would have to bypass the
 * user's proxy, which is never done. */
gboolean inline_image_proxy_usable (void);

#endif
