/* ZoiteChat
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef ZOITECHAT_AUTOAWAY_H
#define ZOITECHAT_AUTOAWAY_H

#include <glib.h>

struct server;

void autoaway_activity (void);
void autoaway_manual (struct server *serv);
void autoaway_reset (struct server *serv);
void autoaway_reinit (void);
/* Both samples are in microseconds; -1 means desktop idle is unavailable. */
void autoaway_check (gint64 now, gint64 desktop_idle);

#endif
