/* ZoiteChat
 * Copyright (C) 1998-2010 Peter Zelezny.
 * Copyright (C) 2009-2013 Berke Viktor.
 * Copyright (C) 2026 deepend-tildeclub.
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

#include "theme-policy.h"

#include "../fe-gtk.h"
#include "../../common/zoitechat.h"
#include "../../common/zoitechatc.h"

gboolean
theme_policy_system_prefers_dark (void)
{
	return FALSE;
}

gboolean
theme_policy_is_dark_mode_active (unsigned int mode)
{
	if (mode == ZOITECHAT_DARK_MODE_AUTO)
		return theme_policy_system_prefers_dark ();

	return fe_dark_mode_is_enabled_for (mode);
}

gboolean
theme_policy_is_app_dark_mode_active (void)
{
	return theme_policy_is_dark_mode_active (prefs.hex_gui_dark_mode);
}
