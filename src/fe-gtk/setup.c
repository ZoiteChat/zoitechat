/* X-Chat
 * Copyright (C) 2004-2007 Peter Zelezny.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../common/zoitechat.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include "../common/cfgfiles.h"
#include "../common/fe.h"
#include "../common/text.h"
#include "../common/userlist.h"
#include "../common/util.h"
#include "../common/zoitechatc.h"
#include "../common/outbound.h"
#include "../common/servlist.h"
#include "fe-gtk.h"
#include "theme/theme-manager.h"
#include "theme/theme-preferences.h"
#include "gtkutil.h"
#include "maingui.h"
#include "pixmaps.h"
#include "menu.h"
#include "preferences-persistence.h"
#include "preferences-pages.h"
#include "setup.h"
#include "editlist.h"
#include "fkeys.h"
#include "textgui.h"
#ifdef USE_PLUGIN
#include "plugingui.h"
#endif

GtkWidget *ignore_preferences_create (void);
#include "plugin-tray.h"
#include "notifications/notification-backend.h"

#ifdef WIN32
#include "../common/fe.h"
#endif
#include "sexy-spell-entry.h"

#define LABEL_INDENT 0

static GtkWidget *setup_window = NULL;
static int last_selected_row = 0; /* sound row */
static gboolean color_change;
static struct zoitechatprefs setup_prefs;
static GtkWidget *cancel_button;
static GtkWidget *font_dialog = NULL;
static GtkWidget *setup_topicbar_multiline_toggle = NULL;
static GtkWidget *setup_wrap_align_label = NULL;
static GtkWidget *setup_wrap_align_combo = NULL;

enum
{
        ST_END,
        ST_TOGGLE,
        ST_TOGGLR,
        ST_3OGGLE,
        ST_ENTRY,
        ST_EFONT,
        ST_EFILE,
        ST_EFOLDER,
        ST_MENU,
        ST_RADIO,
        ST_NUMBER,
        ST_HSCALE,
        ST_HEADER,
        ST_LABEL,
        ST_ALERTHEAD
};

typedef struct
{
        int type;
        char *label;
        int offset;
        char *tooltip;
        char const *const *list;
        int extra;
} setting;

typedef GtkWidget *(*setup_page_factory) (const setting *settings);

/* A page owns its navigation and search metadata. Adding a settings panel does
 * not require another parallel category array or a fixed page limit. */
typedef struct
{
        const char *id;
        const char *group;
        const char *title;
        const char *description;
        setup_page_factory factory;
        const setting *settings;
        GtkWidget *container;
        GtkWidget *body;
        GtkWidget *row;
        char *search_text;
} setup_page;

static GArray *setup_pages;
static GtkWidget *setup_book;
static GtkWidget *setup_sidebar;
static GtkWidget *setup_search;
static GtkWidget *setup_empty;
static GtkWidget *setup_error_bar;
static GtkWidget *setup_error_label;
static char *setup_query;

#ifdef WIN32
static const char *const langsmenu[] =
{
        N_("Afrikaans"),
        N_("Albanian"),
        N_("Amharic"),
        N_("Asturian"),
        N_("Azerbaijani"),
        N_("Basque"),
        N_("Belarusian"),
        N_("Bulgarian"),
        N_("Catalan"),
        N_("Chinese (Simplified)"),
        N_("Chinese (Traditional)"),
        N_("Czech"),
        N_("Danish"),
        N_("Dutch"),
        N_("English (British)"),
        N_("English"),
        N_("Estonian"),
        N_("Finnish"),
        N_("French"),
        N_("Galician"),
        N_("German"),
        N_("Greek"),
        N_("Gujarati"),
        N_("Hindi"),
        N_("Hungarian"),
        N_("Indonesian"),
        N_("Italian"),
        N_("Japanese"),
        N_("Kannada"),
        N_("Kinyarwanda"),
        N_("Korean"),
        N_("Latvian"),
        N_("Lithuanian"),
        N_("Macedonian"),
        N_("Malay"),
        N_("Malayalam"),
        N_("Norwegian (Bokmal)"),
        N_("Norwegian (Nynorsk)"),
        N_("Polish"),
        N_("Portuguese"),
        N_("Portuguese (Brazilian)"),
        N_("Punjabi"),
        N_("Russian"),
        N_("Serbian"),
        N_("Slovak"),
        N_("Slovenian"),
        N_("Spanish"),
        N_("Swedish"),
        N_("Thai"),
        N_("Turkish"),
        N_("Ukrainian"),
        N_("Vietnamese"),
        N_("Walloon"),
        NULL
};
#endif

static const char *const wrapalignmenu[] =
{
        N_("Start of line (default)"),
        N_("Prefix (nick name)"),
        N_("Message"),
        NULL
};

static const setting appearance_settings[] =
{
        {ST_HEADER,     N_("General"),0,0,0},
#ifdef WIN32
        {ST_MENU,   N_("Language:"), P_OFFINTNL(hex_gui_lang), 0, langsmenu, 0},
        {ST_EFONT,  N_("Main font:"), P_OFFSETNL(hex_text_font_main), 0, 0, sizeof prefs.hex_text_font_main},
#else
        {ST_EFONT,  N_("Font:"), P_OFFSETNL(hex_text_font), 0, 0, sizeof prefs.hex_text_font},
#endif

        {ST_HEADER,     N_("Text Box"),0,0,0},
        {ST_TOGGLE, N_("Colored nick names"), P_OFFINTNL(hex_text_color_nicks), N_("Give each person on IRC a different color"),0,0},
        {ST_TOGGLR, N_("Indent nick names"), P_OFFINTNL(hex_text_indent), N_("Make nick names right-justified"),0,0},
        {ST_TOGGLE, N_ ("Show marker line"), P_OFFINTNL (hex_text_show_marker), N_ ("Insert a red line after the last read text."), 0, 0},
        {ST_TOGGLE, N_("Keep search position when closing"), P_OFFINTNL(hex_text_search_keep_position), 0, 0, 0},
        {ST_MENU,   N_("Align wrapped lines under:"), P_OFFINTNL(hex_text_wrap_align), N_("Where wrapped lines start when nick names are not indented"), wrapalignmenu, 0},

        {ST_HEADER,     N_("Timestamps"),0,0,0},
        {ST_TOGGLE, N_("Enable timestamps"), P_OFFINTNL(hex_stamp_text),0,0,1},
        {ST_ENTRY,  N_("Timestamp format:"), P_OFFSETNL(hex_stamp_text_format),
#ifdef WIN32
                                        N_("See the strftime MSDN article for details."),0,sizeof prefs.hex_stamp_text_format},
#else
                                        N_("See the strftime manpage for details."),0,sizeof prefs.hex_stamp_text_format},
#endif

        {ST_HEADER,     N_("Title Bar"),0,0,0},
        {ST_TOGGLE, N_("Show channel modes"), P_OFFINTNL(hex_gui_win_modes),0,0,0},
        {ST_TOGGLR, N_("Show number of users"), P_OFFINTNL(hex_gui_win_ucount),0,0,0},
        {ST_TOGGLE, N_("Show nickname"), P_OFFINTNL(hex_gui_win_nick),0,0,0},

        {ST_HEADER,     N_("Topic Bar"),0,0,0},
        {ST_TOGGLE, N_("Place mode buttons beside the topic"), P_OFFINTNL(hex_gui_mode_buttons_inline), 0, 0, 0},
        {ST_TOGGLE, N_("Allow multi-line topics"), P_OFFINTNL(hex_gui_topicbar_multiline), 0, 0, 0},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting appearance_advanced_settings[] =
{
        {ST_HEADER,     N_("Advanced"),0,0,0},
        {ST_EFILE, N_ ("Background image:"), P_OFFSETNL (hex_text_background), 0, 0, sizeof prefs.hex_text_background},
        {ST_HSCALE, N_("Window opacity:"), P_OFFINTNL(hex_gui_transparency),0,0,0},
        {ST_HSCALE, N_("Mouse wheel scroll speed (Slower ← → Faster):"), P_OFFINTNL(hex_gui_mouse_scroll_speed), 0, 0, 100},

        {ST_END, 0, 0, 0, 0, 0}
};

static const char *const tabcompmenu[] = 
{
        N_("A-Z"),
        N_("Last-spoke order"),
        NULL
};

static const setting inputbox_settings[] =
{
        {ST_HEADER, N_("Input Box"),0,0,0},
        {ST_TOGGLE, N_("Render colors and attributes"), P_OFFINTNL (hex_gui_input_attr),0,0,0},
        {ST_TOGGLE, N_("Show nick box"), P_OFFINTNL(hex_gui_input_nick),0,0,1},
        {ST_TOGGLE, N_("Show user mode icon in nick box"), P_OFFINTNL(hex_gui_input_icon),0,0,0},
        {ST_TOGGLE, N_("Spell checking"), P_OFFINTNL(hex_gui_input_spell),0,0,1},
        {ST_ENTRY,      N_("Dictionaries to use:"), P_OFFSETNL(hex_text_spell_langs),0,0,sizeof prefs.hex_text_spell_langs},
#ifdef WIN32
        {ST_LABEL,      N_("Use language codes (for example, \"en_US\").\nOn Windows 8 and newer, dictionaries are provided by Windows. Separate multiple entries with commas.")},
#else
        {ST_LABEL,      N_("Use language codes. Separate multiple entries with commas.")},
#endif

        {ST_HEADER, N_("Nick Completion"),0,0,0},
        {ST_ENTRY,      N_("Nick completion suffix:"), P_OFFSETNL(hex_completion_suffix),0,0,sizeof prefs.hex_completion_suffix},
        {ST_MENU,       N_("Nick completion sorted:"), P_OFFINTNL(hex_completion_sort), 0, tabcompmenu, 0},
        {ST_NUMBER,     N_("Nick completion amount:"), P_OFFINTNL(hex_completion_amount), N_("Threshold of nicks to start listing instead of completing"), (const char **)N_("nicks."), 1000},

        {ST_END, 0, 0, 0, 0, 0}
};

static const char *const lagmenutext[] = 
{
        N_("Off"),
        N_("Graphical"),
        N_("Text"),
        N_("Both"),
        NULL
};

static const char *const ulmenutext[] = 
{
        N_("A-Z, ops first"),
        N_("A-Z"),
        N_("Z-A, ops last"),
        N_("Z-A"),
        N_("Unsorted"),
        NULL
};

static const char *const cspos[] =
{
        N_("Left (upper)"),
        N_("Left (lower)"),
        N_("Right (upper)"),
        N_("Right (lower)"),
        N_("Top"),
        N_("Bottom"),
        N_("Hidden"),
        NULL
};

static const char *const ulpos[] =
{
        N_("Left (upper)"),
        N_("Left (lower)"),
        N_("Right (upper)"),
        N_("Right (lower)"),
        NULL
};

static const setting userlist_settings[] =
{
        {ST_HEADER,     N_("User List"),0,0,0},
        {ST_TOGGLE, N_("Show hostnames in user list"), P_OFFINTNL(hex_gui_ulist_show_hosts), 0, 0, 0},
        {ST_TOGGLE, N_("Show icons for user modes"), P_OFFINTNL(hex_gui_ulist_icons), N_("Use graphical icons instead of text symbols in the user list."), 0, 0},
        {ST_TOGGLE, N_("Color nicknames in userlist"), P_OFFINTNL(hex_gui_ulist_color), N_("Will color nicknames the same as in chat."), 0, 0},
        {ST_TOGGLE, N_("Show user count in channels"), P_OFFINTNL(hex_gui_ulist_count), 0, 0, 0},
        {ST_MENU,       N_("User list sorted by:"), P_OFFINTNL(hex_gui_ulist_sort), 0, ulmenutext, 0},
        {ST_MENU,       N_("Show user list at:"), P_OFFINTNL(hex_gui_ulist_pos), 0, ulpos, 1},

        {ST_HEADER,     N_("Away Tracking"),0,0,0},
        {ST_TOGGLE,     N_("Track the away status of users and mark them in a different color"), P_OFFINTNL(hex_away_track),0,0,1},
        {ST_NUMBER, N_("On channels smaller than:"), P_OFFINTNL(hex_away_size_max),0,0,10000},

        {ST_HEADER,     N_("Action Upon Double Click"),0,0,0},
        {ST_ENTRY,      N_("Execute command:"), P_OFFSETNL(hex_gui_ulist_doubleclick), 0, 0, sizeof prefs.hex_gui_ulist_doubleclick},

        {ST_HEADER,     N_("Extra Gadgets"),0,0,0},
        {ST_MENU,       N_("Lag meter:"), P_OFFINTNL(hex_gui_lagometer), 0, lagmenutext, 0},
        {ST_MENU,       N_("Throttle meter:"), P_OFFINTNL(hex_gui_throttlemeter), 0, lagmenutext, 0},

        {ST_END, 0, 0, 0, 0, 0}
};

static const char *const tabwin[] =
{
        N_("Windows"),
        N_("Tabs"),
        NULL
};

static const char *const focusnewtabsmenu[] =
{
        N_("Never"),
        N_("Always"),
        N_("Only requested tabs"),
        NULL
};

static const char *const noticeposmenu[] =
{
        N_("Automatic"),
        N_("In an extra tab"),
        N_("In the front tab"),
        NULL
};

static const char *const swtype[] =
{
        N_("Tabs"),
        "",
        N_("Tree"),
        NULL
};

static const setting tabs_settings[] =
{
        {ST_HEADER, N_("Channel Switcher"), 0, 0, 0},
        {ST_RADIO,  N_("Switcher type:"),P_OFFINTNL(hex_gui_tab_layout), 0, swtype, 0},
        {ST_HEADER, N_("Tab Behavior"), 0, 0, 0},
        {ST_TOGGLE, N_("Open an extra tab for server messages"), P_OFFINTNL(hex_gui_tab_server), 0, 0, 0},
        {ST_TOGGLE, N_("Open a new tab when you receive a private message"), P_OFFINTNL(hex_gui_autoopen_dialog), 0, 0, 0},
        {ST_TOGGLE, N_("Sort tabs in alphabetical order"), P_OFFINTNL(hex_gui_tab_sort), 0, 0, 0},
        {ST_TOGGLE, N_("Show icons in the channel tree"), P_OFFINTNL(hex_gui_tab_icons), 0, 0, 0},
        {ST_TOGGLE, N_("Show dotted lines in the channel tree"), P_OFFINTNL(hex_gui_tab_dots), 0, 0, 0},
        {ST_TOGGLE, N_("Scroll mouse-wheel to change tabs"), P_OFFINTNL (hex_gui_tab_scrollchans), 0, 0, 0},
        {ST_TOGGLE, N_("Show close button on tabs"), P_OFFINTNL(hex_gui_tab_closebuttons), 0, 0, 0},
        {ST_TOGGLE, N_("Middle click to close tab"), P_OFFINTNL(hex_gui_tab_middleclose), 0, 0, 0},
        {ST_HEADER, N_("Fonts & Placement"), 0, 0, 0},
        {ST_TOGGLE, N_("Smaller text"), P_OFFINTNL(hex_gui_tab_small), 0, 0, 0},
        {ST_EFONT,  N_("Tab font:"), P_OFFSETNL(hex_gui_tab_font),
                N_("Font for server and channel tabs in the Tabs layout. Leave blank to use the theme font. Smaller text still applies."),
                0, sizeof prefs.hex_gui_tab_font},
        {ST_MENU,       N_("Focus new tabs:"), P_OFFINTNL(hex_gui_tab_newtofront), 0, focusnewtabsmenu, 0},
        {ST_MENU,       N_("Placement of notices:"), P_OFFINTNL(hex_irc_notice_pos), 0, noticeposmenu, 0},
        {ST_MENU,       N_("Show channel switcher at:"), P_OFFINTNL(hex_gui_tab_pos), 0, cspos, 1},
        {ST_NUMBER,     N_("Shorten tab labels to:"), P_OFFINTNL(hex_gui_tab_trunc), 0, (const char **)N_("letters."), 99},

        {ST_HEADER,     N_("Tabs or Windows"),0,0,0},
        {ST_MENU,       N_("Open channels in:"), P_OFFINTNL(hex_gui_tab_chans), 0, tabwin, 0},
        {ST_MENU,       N_("Open dialogs in:"), P_OFFINTNL(hex_gui_tab_dialogs), 0, tabwin, 0},
        {ST_MENU,       N_("Open utilities in:"), P_OFFINTNL(hex_gui_tab_utils), N_("Open DCC, Ignore, Notify etc, in tabs or windows?"), tabwin, 0},

        {ST_END, 0, 0, 0, 0, 0}
};

static const char *const dccaccept[] =
{
        N_("Ask for confirmation"),
        N_("Ask for download folder"),
        N_("Save without interaction"),
        NULL
};

static const setting filexfer_settings[] =
{
        {ST_HEADER, N_("Files and Directories"), 0, 0, 0},
        {ST_MENU,       N_("Auto accept file offers:"), P_OFFINTNL(hex_dcc_auto_recv), 0, dccaccept, 0},
        {ST_EFOLDER,N_("Download files to:"), P_OFFSETNL(hex_dcc_dir), 0, 0, sizeof prefs.hex_dcc_dir},
        {ST_EFOLDER,N_("Move completed files to:"), P_OFFSETNL(hex_dcc_completed_dir), 0, 0, sizeof prefs.hex_dcc_completed_dir},
        {ST_TOGGLE, N_("Save nick name in filenames"), P_OFFINTNL(hex_dcc_save_nick), 0, 0, 0},

        {ST_HEADER,     N_("Auto Open DCC Windows"),0,0,0},
        {ST_TOGGLE, N_("Send window"), P_OFFINTNL(hex_gui_autoopen_send), 0, 0, 0},
        {ST_TOGGLE, N_("Receive window"), P_OFFINTNL(hex_gui_autoopen_recv), 0, 0, 0},
        {ST_TOGGLE, N_("Chat window"), P_OFFINTNL(hex_gui_autoopen_chat), 0, 0, 0},

        {ST_HEADER, N_("Maximum File Transfer Speeds (KiB/s or MiB/s)"), 0, 0, 0},
        {ST_NUMBER,     N_("One upload:"), P_OFFINTNL(hex_dcc_max_send_cps), 
                                        N_("Maximum speed for one transfer"), 0, 10000000},
        {ST_NUMBER,     N_("One download:"), P_OFFINTNL(hex_dcc_max_get_cps),
                                        N_("Maximum speed for one transfer"), 0, 10000000},
        {ST_NUMBER,     N_("All uploads combined:"), P_OFFINTNL(hex_dcc_global_max_send_cps),
                                        N_("Maximum speed for all files"), 0, 10000000},
        {ST_NUMBER,     N_("All downloads combined:"), P_OFFINTNL(hex_dcc_global_max_get_cps),
                                        N_("Maximum speed for all files"), 0, 10000000},

        {ST_END, 0, 0, 0, 0, 0}
};

static const int balloonlist[3] =
{
        P_OFFINTNL(hex_input_balloon_chans), P_OFFINTNL(hex_input_balloon_priv), P_OFFINTNL(hex_input_balloon_hilight)
};

static const int trayblinklist[3] =
{
        P_OFFINTNL(hex_input_tray_chans), P_OFFINTNL(hex_input_tray_priv), P_OFFINTNL(hex_input_tray_hilight)
};

static const int taskbarlist[3] =
{
        P_OFFINTNL(hex_input_flash_chans), P_OFFINTNL(hex_input_flash_priv), P_OFFINTNL(hex_input_flash_hilight)
};

static const int beeplist[3] =
{
        P_OFFINTNL(hex_input_beep_chans), P_OFFINTNL(hex_input_beep_priv), P_OFFINTNL(hex_input_beep_hilight)
};

static const setting alert_settings[] =
{
        {ST_HEADER,     N_("Alerts"),0,0,0},

        {ST_ALERTHEAD},


        {ST_3OGGLE, N_("Show notifications on:"), 0, 0, (void *)balloonlist, 0},
        {ST_3OGGLE, N_("Blink tray icon on:"), 0, 0, (void *)trayblinklist, 0},
        {ST_3OGGLE, N_("Blink task bar on:"), 0, 0, (void *)taskbarlist, 0},
#ifdef WIN32
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, N_("Play the \"Instant Message Notification\" system sound upon the selected events"), (void *)beeplist, 0},
#else
#ifdef USE_LIBCANBERRA
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, N_("Play \"message-new-instant\" from the freedesktop.org sound theme upon the selected events"), (void *)beeplist, 0},
#else
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, N_("Play a GTK beep upon the selected events"), (void *)beeplist, 0},
#endif
#endif

        {ST_TOGGLE,     N_("Omit alerts when marked as being away"), P_OFFINTNL(hex_away_omit_alerts), 0, 0, 0},
        {ST_TOGGLE,     N_("Omit alerts while the window is focused"), P_OFFINTNL(hex_gui_focus_omitalerts), 0, 0, 0},

        {ST_HEADER,     N_("Tray Behavior"), 0, 0, 0},
        {ST_TOGGLE,     N_("Enable system tray icon"), P_OFFINTNL(hex_gui_tray), 0, 0, 4},
        {ST_TOGGLE,     N_("Minimize to tray"), P_OFFINTNL(hex_gui_tray_minimize), 0, 0, 0},
        {ST_TOGGLE,     N_("Close to tray"), P_OFFINTNL(hex_gui_tray_close), 0, 0, 0},
        {ST_TOGGLE,     N_("Automatically mark away/back"), P_OFFINTNL(hex_gui_tray_away), N_("Automatically change status when hiding to tray."), 0, 0},
        {ST_TOGGLE,     N_("Only show notifications when hidden or iconified"), P_OFFINTNL(hex_gui_tray_quiet), 0, 0, 0},

        {ST_HEADER,     N_("Highlighted Messages"),0,0,0},
        {ST_LABEL,      N_("Highlighted messages are ones where your nickname is mentioned, but also:"), 0, 0, 0, 1},

        {ST_ENTRY,      N_("Extra words to highlight:"), P_OFFSETNL(hex_irc_extra_hilight), 0, 0, sizeof prefs.hex_irc_extra_hilight},
        {ST_ENTRY,      N_("Nick names not to highlight:"), P_OFFSETNL(hex_irc_no_hilight), 0, 0, sizeof prefs.hex_irc_no_hilight},
        {ST_ENTRY,      N_("Nick names to always highlight:"), P_OFFSETNL(hex_irc_nick_hilight), 0, 0, sizeof prefs.hex_irc_nick_hilight},
        {ST_LABEL,      N_("Separate multiple words with commas.\nWildcards are accepted.")},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting alert_settings_nonotifications[] =
{
        {ST_HEADER,     N_("Alerts"),0,0,0},

        {ST_ALERTHEAD},
        {ST_3OGGLE, N_("Blink tray icon on:"), 0, 0, (void *)trayblinklist, 0},
#ifdef HAVE_GTK_MAC
        {ST_3OGGLE, N_("Bounce dock icon on:"), 0, 0, (void *)taskbarlist, 0},
#else
#ifndef __APPLE__
        {ST_3OGGLE, N_("Blink task bar on:"), 0, 0, (void *)taskbarlist, 0},
#endif
#endif
#ifdef WIN32
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, N_("Play the \"Instant Message Notification\" system sound upon the selected events"), (void *)beeplist, 0},
#else
#ifdef USE_LIBCANBERRA
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, N_("Play \"message-new-instant\" from the freedesktop.org sound theme upon the selected events"), (void *)beeplist, 0},
#else
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, N_("Play a GTK beep upon the selected events"), (void *)beeplist, 0},
#endif
#endif

        {ST_TOGGLE,     N_("Omit alerts when marked as being away"), P_OFFINTNL(hex_away_omit_alerts), 0, 0, 0},
        {ST_TOGGLE,     N_("Omit alerts while the window is focused"), P_OFFINTNL(hex_gui_focus_omitalerts), 0, 0, 0},

        {ST_HEADER,     N_("Tray Behavior"), 0, 0, 0},
        {ST_TOGGLE,     N_("Enable system tray icon"), P_OFFINTNL(hex_gui_tray), 0, 0, 4},
        {ST_TOGGLE,     N_("Minimize to tray"), P_OFFINTNL(hex_gui_tray_minimize), 0, 0, 0},
        {ST_TOGGLE,     N_("Close to tray"), P_OFFINTNL(hex_gui_tray_close), 0, 0, 0},
        {ST_TOGGLE,     N_("Automatically mark away/back"), P_OFFINTNL(hex_gui_tray_away), N_("Automatically change status when hiding to tray."), 0, 0},

        {ST_HEADER,     N_("Highlighted Messages"),0,0,0},
        {ST_LABEL,      N_("Highlighted messages are ones where your nickname is mentioned, but also:"), 0, 0, 0, 1},

        {ST_ENTRY,      N_("Extra words to highlight:"), P_OFFSETNL(hex_irc_extra_hilight), 0, 0, sizeof prefs.hex_irc_extra_hilight},
        {ST_ENTRY,      N_("Nick names not to highlight:"), P_OFFSETNL(hex_irc_no_hilight), 0, 0, sizeof prefs.hex_irc_no_hilight},
        {ST_ENTRY,      N_("Nick names to always highlight:"), P_OFFSETNL(hex_irc_nick_hilight), 0, 0, sizeof prefs.hex_irc_nick_hilight},
        {ST_LABEL,      N_("Separate multiple words with commas.\nWildcards are accepted.")},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting alert_settings_unity[] =
{
        {ST_HEADER,     N_("Alerts"),0,0,0},

        {ST_ALERTHEAD},
        {ST_3OGGLE, N_("Show notifications on:"), 0, 0, (void *)balloonlist, 0},
        {ST_3OGGLE, N_("Blink task bar on:"), 0, 0, (void *)taskbarlist, 0},
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, 0, (void *)beeplist, 0},

        {ST_TOGGLE,     N_("Omit alerts when marked as being away"), P_OFFINTNL(hex_away_omit_alerts), 0, 0, 0},
        {ST_TOGGLE,     N_("Omit alerts while the window is focused"), P_OFFINTNL(hex_gui_focus_omitalerts), 0, 0, 0},

        {ST_HEADER,     N_("Highlighted Messages"),0,0,0},
        {ST_LABEL,      N_("Highlighted messages are ones where your nickname is mentioned, but also:"), 0, 0, 0, 1},

        {ST_ENTRY,      N_("Extra words to highlight:"), P_OFFSETNL(hex_irc_extra_hilight), 0, 0, sizeof prefs.hex_irc_extra_hilight},
        {ST_ENTRY,      N_("Nick names not to highlight:"), P_OFFSETNL(hex_irc_no_hilight), 0, 0, sizeof prefs.hex_irc_no_hilight},
        {ST_ENTRY,      N_("Nick names to always highlight:"), P_OFFSETNL(hex_irc_nick_hilight), 0, 0, sizeof prefs.hex_irc_nick_hilight},
        {ST_LABEL,      N_("Separate multiple words with commas.\nWildcards are accepted.")},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting alert_settings_unityandnonotifications[] =
{
        {ST_HEADER, N_("Alerts"), 0, 0, 0},

        {ST_ALERTHEAD},
        {ST_3OGGLE, N_("Blink task bar on:"), 0, 0, (void *)taskbarlist, 0},
        {ST_3OGGLE, N_("Make a beep sound on:"), 0, 0, (void *)beeplist, 0},

        {ST_TOGGLE, N_("Omit alerts when marked as being away"), P_OFFINTNL (hex_away_omit_alerts), 0, 0, 0},
        {ST_TOGGLE, N_("Omit alerts while the window is focused"), P_OFFINTNL (hex_gui_focus_omitalerts), 0, 0, 0},

        {ST_HEADER, N_("Highlighted Messages"), 0, 0, 0},
        {ST_LABEL, N_("Highlighted messages are ones where your nickname is mentioned, but also:"), 0, 0, 0, 1},

        {ST_ENTRY, N_("Extra words to highlight:"), P_OFFSETNL (hex_irc_extra_hilight), 0, 0, sizeof prefs.hex_irc_extra_hilight},
        {ST_ENTRY, N_("Nick names not to highlight:"), P_OFFSETNL (hex_irc_no_hilight), 0, 0, sizeof prefs.hex_irc_no_hilight},
        {ST_ENTRY, N_("Nick names to always highlight:"), P_OFFSETNL (hex_irc_nick_hilight), 0, 0, sizeof prefs.hex_irc_nick_hilight},
        {ST_LABEL, N_("Separate multiple words with commas.\nWildcards are accepted.")},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting general_settings[] =
{
        {ST_HEADER,     N_("Default Messages"),0,0,0},
        {ST_ENTRY,      N_("Quit:"), P_OFFSETNL(hex_irc_quit_reason), 0, 0, sizeof prefs.hex_irc_quit_reason},
        {ST_ENTRY,      N_("Leave channel:"), P_OFFSETNL(hex_irc_part_reason), 0, 0, sizeof prefs.hex_irc_part_reason},
        {ST_ENTRY,      N_("Away:"), P_OFFSETNL(hex_away_reason), 0, 0, sizeof prefs.hex_away_reason},

        {ST_HEADER,     N_("Away"),0,0,0},
        {ST_TOGGLE,     N_("Show away once"), P_OFFINTNL(hex_away_show_once), N_("Show identical away messages only once."), 0, 0},
        {ST_TOGGLE,     N_("Automatically unmark away"), P_OFFINTNL(hex_away_auto_unmark), N_("Unmark yourself as away before sending messages."), 0, 0},

        {ST_HEADER,     N_("Miscellaneous"),0,0,0},
        {ST_TOGGLE,     N_("Display MODEs in raw form"), P_OFFINTNL(hex_irc_raw_modes), 0, 0, 0},
        {ST_TOGGLE,     N_("WHOIS on notify"), P_OFFINTNL(hex_notify_whois_online), N_("Sends a /WHOIS when a user comes online in your notify list."), 0, 0},
        {ST_TOGGLE,     N_("Hide join and part messages"), P_OFFINTNL(hex_irc_conf_mode), N_("Hide channel join/part messages by default."), 0, 0},
        {ST_TOGGLE,     N_("Hide nick change messages"), P_OFFINTNL(hex_irc_hide_nickchange), 0, 0, 0},
        {ST_TOGGLE,     N_("Hide hostmasks in join and part messages"), P_OFFINTNL(hex_irc_hide_join_part_hostmask), 0, 0, 0},
#ifdef __APPLE__
        {ST_TOGGLE,     N_("Enable Command+Q to quit"), P_OFFINTNL(hex_gui_ctrlq_quit), 0, 0, 0},
#else
        {ST_TOGGLE,     N_("Enable Ctrl+Q to quit"), P_OFFINTNL(hex_gui_ctrlq_quit), 0, 0, 0},
#endif

        {ST_END, 0, 0, 0, 0, 0}
};

static const char *const bantypemenu[] = 
{
        N_("*!*@*.host"),
        N_("*!*@domain"),
        N_("*!*user@*.host"),
        N_("*!*user@domain"),
        NULL
};

static const setting advanced_settings[] =
{
        {ST_HEADER,     N_("Auto Copy Behavior"),0,0,0},
        {ST_TOGGLE, N_("Automatically copy selected text"), P_OFFINTNL(hex_text_autocopy_text),
                                        N_("Copy selected text to clipboard when left mouse button is released. "
                                                "Otherwise, Ctrl+Shift+C will copy the "
                                                "selected text to the clipboard."), 0, 0},
        {ST_TOGGLE, N_("Automatically include timestamps"), P_OFFINTNL(hex_text_autocopy_stamp),
                                        N_("Automatically include timestamps in copied lines of text. Otherwise, "
                                                "include timestamps if the Shift key is held down while selecting."), 0, 0},
        {ST_TOGGLE, N_("Automatically include color information"), P_OFFINTNL(hex_text_autocopy_color),
                                        N_("Automatically include color information in copied lines of text.  "
                                                "Otherwise, include color information if the Ctrl key is held down "
                                                "while selecting."), 0, 0},

        {ST_HEADER,     N_("Miscellaneous"), 0, 0, 0},
#ifdef WIN32
        {ST_ENTRY,  N_("Alternative fonts:"), P_OFFSETNL(hex_text_font_alternative), N_("Separate multiple entries with commas without spaces before or after."), 0, sizeof prefs.hex_text_font_alternative},
#endif
        {ST_TOGGLE,     N_("Display lists in compact mode"), P_OFFINTNL(hex_gui_compact), N_("Use less spacing between user list/channel tree rows."), 0, 0},
        {ST_TOGGLE,     N_("Use server time if supported"), P_OFFINTNL(hex_irc_cap_server_time), N_("Display timestamps obtained from server if it supports the time-server extension."), 0, 0},
        {ST_TOGGLE,     N_("Automatically reconnect to servers on disconnect"), P_OFFINTNL(hex_net_auto_reconnect), 0, 0, 1},
        {ST_NUMBER,     N_("Lag check interval:"), P_OFFINTNL(hex_net_lag_check), 0, (const char **)N_("seconds."), 9999},
        {ST_NUMBER,     N_("Auto reconnect delay:"), P_OFFINTNL(hex_net_reconnect_delay), 0, 0, 9999},
        {ST_NUMBER,     N_("Auto join delay:"), P_OFFINTNL(hex_irc_join_delay), 0, 0, 9999},
        {ST_MENU,       N_("Ban Type:"), P_OFFINTNL(hex_irc_ban_type), N_("Attempt to use this banmask when banning or quieting. (requires irc_who_join)"), bantypemenu, 0},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting logging_settings[] =
{
        {ST_HEADER,     N_("Logging"),0,0,0},
        {ST_TOGGLE,     N_("Display scrollback from previous session"), P_OFFINTNL(hex_text_replay), 0, 0, 0},
        {ST_NUMBER,     N_("Scrollback lines:"), P_OFFINTNL(hex_text_max_lines),0,0,100000},
        {ST_TOGGLE,     N_("Enable logging of conversations to disk"), P_OFFINTNL(hex_irc_logging), 0, 0, 0},
        {ST_ENTRY,      N_("Log filename:"), P_OFFSETNL(hex_irc_logmask), 0, 0, sizeof prefs.hex_irc_logmask},
        {ST_LABEL,      N_("%s=Server %c=Channel %n=Network.")},

        {ST_HEADER,     N_("Timestamps"),0,0,0},
        {ST_TOGGLE,     N_("Insert timestamps in logs"), P_OFFINTNL(hex_stamp_log), 0, 0, 1},
        {ST_ENTRY,      N_("Log timestamp format:"), P_OFFSETNL(hex_stamp_log_format), 0, 0, sizeof prefs.hex_stamp_log_format},
#ifdef WIN32
        {ST_LABEL,      N_("See the strftime MSDN article for details.")},
#else
        {ST_LABEL,      N_("See the strftime manpage for details.")},
#endif

        {ST_HEADER,     N_("URLs"),0,0,0},
        {ST_TOGGLE,     N_("Enable logging of URLs to disk"), P_OFFINTNL(hex_url_logging), 0, 0, 0},
        {ST_TOGGLE,     N_("Enable URL grabber"), P_OFFINTNL(hex_url_grabber), 0, 0, 1},
        {ST_NUMBER,     N_("Maximum number of URLs to grab:"), P_OFFINTNL(hex_url_grabber_limit), 0, 0, 9999},

        {ST_END, 0, 0, 0, 0, 0}
};

static const char *const proxytypes[] =
{
        N_("(Disabled)"),
        N_("Wingate"),
        N_("SOCKS4"),
        N_("SOCKS5"),
        N_("HTTP"),
        N_("Auto"),
        NULL
};

static const char *const proxyuse[] =
{
        N_("All connections"),
        N_("IRC server only"),
        N_("DCC only"),
        NULL
};

static const setting network_settings[] =
{
        {ST_HEADER,     N_("Your Address"), 0, 0, 0, 0},
        {ST_ENTRY,      N_("Bind to:"), P_OFFSETNL(hex_net_bind_host), 0, 0, sizeof prefs.hex_net_bind_host},
        {ST_LABEL,      N_("Only useful for computers with multiple addresses.")},

        {ST_HEADER, N_("File Transfers"), 0, 0, 0},
        {ST_TOGGLE, N_("Get my address from the IRC server"), P_OFFINTNL(hex_dcc_ip_from_server),
                                        N_("Asks the IRC server for your real address. Use this if you have a 192.168.*.* address!"), 0, 0},
        {ST_ENTRY,      N_("DCC IP address:"), P_OFFSETNL(hex_dcc_ip),
                                        N_("Claim you are at this address when offering files."), 0, sizeof prefs.hex_dcc_ip},
        {ST_NUMBER,     N_("First DCC listen port:"), P_OFFINTNL(hex_dcc_port_first), 0, 0, 65535},
        {ST_NUMBER,     N_("Last DCC listen port:"), P_OFFINTNL(hex_dcc_port_last), 0, 
                (const char **)N_("!Leave ports at zero for full range."), 65535},

        {ST_HEADER,     N_("Proxy Server"), 0, 0, 0, 0},
        {ST_ENTRY,      N_("Hostname:"), P_OFFSETNL(hex_net_proxy_host), 0, 0, sizeof prefs.hex_net_proxy_host},
        {ST_NUMBER,     N_("Port:"), P_OFFINTNL(hex_net_proxy_port), 0, 0, 65535},
        {ST_MENU,       N_("Type:"), P_OFFINTNL(hex_net_proxy_type), 0, proxytypes, 0},
        {ST_MENU,       N_("Use proxy for:"), P_OFFINTNL(hex_net_proxy_use), 0, proxyuse, 0},

        {ST_HEADER,     N_("Connection Health"), 0, 0, 0, 0},
        {ST_NUMBER,     N_("TCP keepalive idle:"), P_OFFINTNL(hex_net_keepalive_idle), 0, (const char **)N_("seconds."), 7200},
        {ST_NUMBER,     N_("TCP keepalive interval:"), P_OFFINTNL(hex_net_keepalive_interval), 0, (const char **)N_("seconds."), 600},
        {ST_NUMBER,     N_("TCP keepalive probes:"), P_OFFINTNL(hex_net_keepalive_count), 0, 0, 20},

        {ST_HEADER,     N_("Proxy Authentication"), 0, 0, 0, 0},
        {ST_TOGGLE,     N_("Use authentication (HTTP or SOCKS5 only)"), P_OFFINTNL(hex_net_proxy_auth), 0, 0, 0},
        {ST_ENTRY,      N_("Username:"), P_OFFSETNL(hex_net_proxy_user), 0, 0, sizeof prefs.hex_net_proxy_user},
        {ST_ENTRY,      N_("Password:"), P_OFFSETNL(hex_net_proxy_pass), 0, GINT_TO_POINTER(1), sizeof prefs.hex_net_proxy_pass},

        {ST_HEADER,     N_("Remote Media"), 0, 0, 0, 0},
        {ST_TOGGLE,     N_("Allow loading remote media (inline images) when clicked"),
                                        P_OFFINTNL(hex_net_remote_media),
                                        N_("Loading remote media contacts the site hosting it, which can reveal your IP address and that you viewed the message. You are asked for confirmation before each new domain is contacted."), 0, 0},

        {ST_END, 0, 0, 0, 0, 0}
};

static const setting identd_settings[] =
{
        {ST_HEADER, N_("Identd Server"), 0, 0, 0, 0},
        {ST_TOGGLE, N_("Enabled"), P_OFFINTNL(hex_identd_server), N_("Server will respond with the networks username"), 0, 1},
        {ST_NUMBER,     N_("Port:"), P_OFFINTNL(hex_identd_port), N_("You must have permissions to listen on this port. "
                                                                                                   "If not 113 (0 defaults to this) then you must configure port-forwarding."), 0, 65535},

        {ST_END, 0, 0, 0, 0, 0}
};

#define setup_get_str(pr,set) (((char *)pr)+set->offset)
#define setup_get_int(pr,set) *(((int *)pr)+set->offset)
#define setup_get_int3(pr,off) *(((int *)pr)+off) 

#define setup_set_int(pr,set,num) *((int *)pr+set->offset)=num
#define setup_set_str(pr,set,str) strcpy(((char *)pr)+set->offset,str)

typedef enum
{
	SETUP_ALIGN_START,
	SETUP_ALIGN_CENTER,
	SETUP_ALIGN_FILL
} setup_align;

static GtkAlign
setup_align_to_gtk (setup_align align)
{
	switch (align)
	{
	case SETUP_ALIGN_FILL:
		return GTK_ALIGN_FILL;
	case SETUP_ALIGN_CENTER:
		return GTK_ALIGN_CENTER;
	case SETUP_ALIGN_START:
	default:
		return GTK_ALIGN_START;
	}
}

static void
setup_table_attach (GtkWidget *table, GtkWidget *child,
		    guint left_attach, guint right_attach,
		    guint top_attach, guint bottom_attach,
		    gboolean hexpand, gboolean vexpand,
		    setup_align halign, setup_align valign,
		    guint xpad, guint ypad)
{
	gtk_widget_set_hexpand (child, hexpand);
	gtk_widget_set_vexpand (child, vexpand);
	gtk_widget_set_halign (child, setup_align_to_gtk (halign));
	gtk_widget_set_valign (child, setup_align_to_gtk (valign));
	gtk_widget_set_margin_start (child, xpad);
	gtk_widget_set_margin_end (child, xpad);
	gtk_widget_set_margin_top (child, ypad);
	gtk_widget_set_margin_bottom (child, ypad);

	gtk_grid_attach (GTK_GRID (table), child, left_attach, top_attach,
			 right_attach - left_attach, bottom_attach - top_attach);
}


static void
setup_3oggle_cb (GtkToggleButton *but, unsigned int *setting)
{
        *setting = gtk_toggle_button_get_active (but);
}

static void
setup_headlabel (GtkWidget *tab, int row, int col, char *text)
{
        GtkWidget *label;
        char buf[128];
        char *sp;

        g_snprintf (buf, sizeof (buf), "<b><span size=\"smaller\">%s</span></b>", text);
        sp = strchr (buf + 17, ' ');
        if (sp)
                *sp = '\n';

        label = gtk_label_new (NULL);
        gtk_label_set_markup (GTK_LABEL (label), buf);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        setup_table_attach (tab, label, col, col + 1, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER, 4, 0);
}

static void
setup_create_alert_header (GtkWidget *tab, int row, const setting *set)
{
        setup_headlabel (tab, row, 3, _("Channel Message"));
        setup_headlabel (tab, row, 4, _("Private Message"));
        setup_headlabel (tab, row, 5, _("Highlighted Message"));
}

/* makes 3 toggles side-by-side */

static void
setup_create_3oggle (GtkWidget *tab, int row, const setting *set)
{
        GtkWidget *label, *wid;
        int *offsets = (int *)set->list;

        label = gtk_label_new (_(set->label));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        if (set->tooltip)
        {
                gtk_widget_set_tooltip_text (label, _(set->tooltip));
        }
        setup_table_attach (tab, label, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        wid = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid),
                                                                                        setup_get_int3 (&setup_prefs, offsets[0]));
        {
                char *name = g_strdup_printf ("%s %s", _(set->label), _("Channel Message"));
                atk_object_set_name (gtk_widget_get_accessible (wid), name);
                g_free (name);
        }
        g_signal_connect (G_OBJECT (wid), "toggled",
                                                        G_CALLBACK (setup_3oggle_cb), ((int *)&setup_prefs) + offsets[0]);
        setup_table_attach (tab, wid, 3, 4, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_CENTER, SETUP_ALIGN_CENTER, 0, 0);

        wid = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid),
                                                                                        setup_get_int3 (&setup_prefs, offsets[1]));
        {
                char *name = g_strdup_printf ("%s %s", _(set->label), _("Private Message"));
                atk_object_set_name (gtk_widget_get_accessible (wid), name);
                g_free (name);
        }
        g_signal_connect (G_OBJECT (wid), "toggled",
                                                        G_CALLBACK (setup_3oggle_cb), ((int *)&setup_prefs) + offsets[1]);
        setup_table_attach (tab, wid, 4, 5, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_CENTER, SETUP_ALIGN_CENTER, 0, 0);

        wid = gtk_check_button_new ();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid),
                                                                                        setup_get_int3 (&setup_prefs, offsets[2]));
        {
                char *name = g_strdup_printf ("%s %s", _(set->label), _("Highlighted Message"));
                atk_object_set_name (gtk_widget_get_accessible (wid), name);
                g_free (name);
        }
        g_signal_connect (G_OBJECT (wid), "toggled",
                                                        G_CALLBACK (setup_3oggle_cb), ((int *)&setup_prefs) + offsets[2]);
        setup_table_attach (tab, wid, 5, 6, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_CENTER, SETUP_ALIGN_CENTER, 0, 0);
}

static void
setup_toggle_cb (GtkToggleButton *but, const setting *set)
{
        GtkWidget *label, *disable_wid;

        setup_set_int (&setup_prefs, set, gtk_toggle_button_get_active (but));

        /* does this toggle also enable/disable another widget? */
        disable_wid = g_object_get_data (G_OBJECT (but), "nxt");
        if (disable_wid)
        {
                gtk_widget_set_sensitive (disable_wid, gtk_toggle_button_get_active (but));
                label = g_object_get_data (G_OBJECT (disable_wid), "lbl");
                gtk_widget_set_sensitive (label, gtk_toggle_button_get_active (but));
        }
}

static void
setup_toggle_sensitive_cb (GtkToggleButton *but, GtkWidget *wid)
{
        gtk_widget_set_sensitive (wid, gtk_toggle_button_get_active (but));
}

static void
setup_topicbar_inline_toggled_cb (GtkToggleButton *but, gpointer userdata)
{
        (void) userdata;

        if (setup_topicbar_multiline_toggle)
                gtk_widget_set_sensitive (setup_topicbar_multiline_toggle,
                        !gtk_toggle_button_get_active (but));
}

static void
setup_text_indent_toggled_cb (GtkToggleButton *but, gpointer userdata)
{
        gboolean sens = !gtk_toggle_button_get_active (but);

        (void) userdata;

        /* wrapped line alignment only applies without nick indentation */
        if (setup_wrap_align_label)
                gtk_widget_set_sensitive (setup_wrap_align_label, sens);
        if (setup_wrap_align_combo)
                gtk_widget_set_sensitive (setup_wrap_align_combo, sens);
}

static GtkWidget *
setup_create_toggleL (GtkWidget *tab, int row, const setting *set)
{
        GtkWidget *wid, *label;

        wid = gtk_check_button_new_with_label (_(set->label));
        label = gtk_bin_get_child (GTK_BIN (wid));
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 60);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid),
                                                                                        setup_get_int (&setup_prefs, set));
        g_signal_connect (G_OBJECT (wid), "toggled",
                                                        G_CALLBACK (setup_toggle_cb), (gpointer)set);
        if (set->offset == STRUCT_OFFSET_INT (struct zoitechatprefs, hex_text_indent))
                g_signal_connect (G_OBJECT (wid), "toggled",
                                                        G_CALLBACK (setup_text_indent_toggled_cb), NULL);
        if (set->offset == STRUCT_OFFSET_INT (struct zoitechatprefs, hex_gui_mode_buttons_inline))
                g_signal_connect (G_OBJECT (wid), "toggled",
                                                        G_CALLBACK (setup_topicbar_inline_toggled_cb), NULL);
        if (set->offset == STRUCT_OFFSET_INT (struct zoitechatprefs, hex_gui_topicbar_multiline))
        {
                setup_topicbar_multiline_toggle = wid;
                gtk_widget_set_sensitive (wid, !setup_prefs.hex_gui_mode_buttons_inline);
        }
        if (set->tooltip)
                gtk_widget_set_tooltip_text (wid, _(set->tooltip));
        setup_table_attach (tab, wid, 2, 6, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, LABEL_INDENT, 0);

        return wid;
}

static GtkWidget *
setup_create_italic_label (char *text)
{
        GtkWidget *label;
        char *markup;

        label = gtk_label_new (NULL);
        markup = g_markup_printf_escaped ("<span size=\"smaller\">%s</span>", text);
        gtk_label_set_markup (GTK_LABEL (label), markup);
        g_free (markup);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 52);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");

        return label;
}

typedef struct
{
        const setting *set;
        GtkSpinButton *spin;
        GtkComboBox *unit;
} setup_dcc_speed_data;

static gboolean
setup_is_dcc_speed_setting (const setting *set)
{
        return set->offset == P_OFFINTNL (hex_dcc_max_send_cps)
                || set->offset == P_OFFINTNL (hex_dcc_max_get_cps)
                || set->offset == P_OFFINTNL (hex_dcc_global_max_send_cps)
                || set->offset == P_OFFINTNL (hex_dcc_global_max_get_cps);
}

static int
setup_dcc_speed_multiplier (GtkComboBox *combo)
{
        return gtk_combo_box_get_active (combo) == 1 ? 1024 * 1024 : 1024;
}

static void
setup_dcc_speed_spin_range (setup_dcc_speed_data *data)
{
        int max = data->set->extra / setup_dcc_speed_multiplier (data->unit);
        gtk_spin_button_set_range (data->spin, 0, max > 0 ? max : 0);
}

static void
setup_dcc_speed_store (setup_dcc_speed_data *data)
{
        int value = gtk_spin_button_get_value_as_int (data->spin);
        int speed = value * setup_dcc_speed_multiplier (data->unit);
        if (speed > data->set->extra)
                speed = data->set->extra;
        setup_set_int (&setup_prefs, data->set, speed);
}

static void
setup_dcc_speed_spin_cb (GtkSpinButton *spin, setup_dcc_speed_data *data)
{
        (void)spin;
        setup_dcc_speed_store (data);
}

static void
setup_dcc_speed_unit_cb (GtkComboBox *combo, setup_dcc_speed_data *data)
{
        int speed = setup_get_int (&setup_prefs, data->set);
        int multiplier;
        int value;

        (void)combo;
        setup_dcc_speed_spin_range (data);
        multiplier = setup_dcc_speed_multiplier (data->unit);
        value = (speed + (multiplier / 2)) / multiplier;
        gtk_spin_button_set_value (data->spin, value);
        setup_dcc_speed_store (data);
}

static void
setup_spin_cb (GtkSpinButton *spin, const setting *set)
{
        setup_set_int (&setup_prefs, set, gtk_spin_button_get_value_as_int (spin));
}

static GtkWidget *
setup_create_spin (GtkWidget *table, int row, const setting *set)
{
        GtkWidget *label, *wid, *rbox;
        char *text;

        label = gtk_label_new (_(set->label));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        setup_table_attach (table, label, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        rbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
        gtk_widget_set_halign (rbox, GTK_ALIGN_START);
        gtk_widget_set_valign (rbox, GTK_ALIGN_CENTER);
        setup_table_attach (table, rbox, 3, 4, row, row + 1, TRUE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

        wid = gtk_spin_button_new_with_range (0, set->extra, 1);
        g_object_set_data (G_OBJECT (wid), "lbl", label);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), wid);
        if (set->tooltip)
                gtk_widget_set_tooltip_text (wid, _(set->tooltip));
        if (setup_is_dcc_speed_setting (set))
        {
                GtkWidget *unit;
                setup_dcc_speed_data *data = g_new0 (setup_dcc_speed_data, 1);
                int speed = setup_get_int (&setup_prefs, set);
                int use_mib = speed >= (1024 * 1024) && speed % (1024 * 1024) == 0;
                int multiplier;

                data->set = set;
                data->spin = GTK_SPIN_BUTTON (wid);

                unit = gtk_combo_box_text_new ();
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (unit), _("KiB/s"));
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (unit), _("MiB/s"));
                gtk_combo_box_set_wrap_width (GTK_COMBO_BOX (unit), 1);
                gtk_combo_box_set_active (GTK_COMBO_BOX (unit), use_mib ? 1 : 0);
                data->unit = GTK_COMBO_BOX (unit);

                setup_dcc_speed_spin_range (data);
                multiplier = setup_dcc_speed_multiplier (data->unit);
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (wid), (speed + (multiplier / 2)) / multiplier);

                g_signal_connect (G_OBJECT (wid), "value-changed",
                                                        G_CALLBACK (setup_dcc_speed_spin_cb), data);
                g_signal_connect (G_OBJECT (unit), "changed",
                                                        G_CALLBACK (setup_dcc_speed_unit_cb), data);

                gtk_box_pack_start (GTK_BOX (rbox), wid, 0, 0, 0);
                gtk_box_pack_start (GTK_BOX (rbox), unit, 0, 0, 6);
        } else
        {
                gtk_spin_button_set_value (GTK_SPIN_BUTTON (wid),
                                                                                setup_get_int (&setup_prefs, set));
                g_signal_connect (G_OBJECT (wid), "value-changed",
                                                        G_CALLBACK (setup_spin_cb), (gpointer)set);
                gtk_box_pack_start (GTK_BOX (rbox), wid, 0, 0, 0);
        }

        if (set->list)
        {
                text = _((char *)set->list);
                if (text[0] == '!')
                        label = setup_create_italic_label (text + 1);
                else
                        label = gtk_label_new (text);
                gtk_box_pack_start (GTK_BOX (rbox), label, 0, 0, 6);
        }

        return wid;
}

static guint setup_trans_apply_tag = 0;
/* the window opacity the dialog opened with, so cancelling can undo the
 * live preview done by the slider */
static int setup_transparency_snapshot;

static gint
setup_apply_trans (gpointer userdata)
{
        prefs.hex_gui_transparency = setup_prefs.hex_gui_transparency;
        if (current_sess)
                gtk_widget_set_opacity (current_sess->gui->window,
                                                                (prefs.hex_gui_transparency / 255.));

        setup_trans_apply_tag = 0;
        return 0;
}

static void
setup_hscale_cb (GtkRange *wid, const setting *set)
{
        setup_set_int (&setup_prefs, set, (int) gtk_range_get_value (wid));

        if (setup_trans_apply_tag == 0)
                setup_trans_apply_tag = g_idle_add ((GSourceFunc) setup_apply_trans, NULL);
}

static void
setup_create_hscale (GtkWidget *table, int row, const setting *set)
{
        GtkWidget *wid;

        wid = gtk_label_new (_(set->label));
        gtk_widget_set_halign (wid, GTK_ALIGN_START);
        gtk_widget_set_valign (wid, GTK_ALIGN_CENTER);
        setup_table_attach (table, wid, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        wid = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0., 255., 1.);
        gtk_scale_set_value_pos (GTK_SCALE (wid), GTK_POS_RIGHT);
        gtk_range_set_value (GTK_RANGE (wid), setup_get_int (&setup_prefs, set));
        g_signal_connect (G_OBJECT(wid), "value-changed",
                                                        G_CALLBACK (setup_hscale_cb), (gpointer)set);
        setup_table_attach (table, wid, 3, 6, row, row + 1, TRUE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

#ifndef WIN32 /* Windows always supports this */
        /* Only used for transparency currently */
        if (current_sess &&
                 !gdk_screen_is_composited (gtk_widget_get_screen (current_sess->gui->window)))
                gtk_widget_set_sensitive (wid, FALSE);
#endif
}


static GtkWidget *proxy_user;
static GtkWidget *proxy_pass;

static void
setup_menu_cb (GtkWidget *cbox, const setting *set)
{
        int n = gtk_combo_box_get_active (GTK_COMBO_BOX (cbox));

        /* set the prefs.<field> */
        setup_set_int (&setup_prefs, set, n + set->extra);

        if (set->list == proxytypes)
        {
                /* only HTTP and SOCKS5 can use a username/pass */
                gtk_widget_set_sensitive (proxy_user, (n == 3 || n == 4 || n == 5));
                gtk_widget_set_sensitive (proxy_pass, (n == 3 || n == 4 || n == 5));
        }
}

static void
setup_radio_cb (GtkWidget *item, const setting *set)
{
        if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (item)))
        {
                int n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "n"));
                /* set the prefs.<field> */
                setup_set_int (&setup_prefs, set, n);
        }
}

static int
setup_create_radio (GtkWidget *table, int row, const setting *set)
{
        GtkWidget *wid, *hbox;
        int i;
        const char **text = (const char **)set->list;
        GSList *group;

        wid = gtk_label_new (_(set->label));
        gtk_widget_set_halign (wid, GTK_ALIGN_START);
        gtk_widget_set_valign (wid, GTK_ALIGN_CENTER);
        setup_table_attach (table, wid, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
        setup_table_attach (table, hbox, 3, 4, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

        i = 0;
        group = NULL;
        while (text[i])
        {
                if (text[i][0] != 0)
                {
                        wid = gtk_radio_button_new_with_mnemonic (group, _(text[i]));
                        /*if (set->tooltip)
                                gtk_widget_set_tooltip_text (wid, _(set->tooltip));*/
                        group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (wid));
                        gtk_container_add (GTK_CONTAINER (hbox), wid);
                        if (i == setup_get_int (&setup_prefs, set))
                                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (wid), TRUE);
                        g_object_set_data (G_OBJECT (wid), "n", GINT_TO_POINTER (i));
                        g_signal_connect (G_OBJECT (wid), "toggled",
                                                                        G_CALLBACK (setup_radio_cb), (gpointer)set);
                }
                i++;
                row++;
        }

        return i;
}

/*
static const char *id_strings[] =
{
        "",
        "*",
        "%C4*%C18%B%B",
        "%U"
};

static void
setup_id_menu_cb (GtkWidget *item, char *dest)
{
        int n = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "n"));

        strcpy (dest, id_strings[n]);
}

static void
setup_create_id_menu (GtkWidget *table, char *label, int row, char *dest)
{
        GtkWidget *wid, *menu, *item;
        int i, def = 0;
        static const char *text[] =
        {
                ("(disabled)"),
                ("A star (*)"),
                ("A red star (*)"),
                ("Underlined")
        };

        wid = gtk_label_new (label);
        gtk_widget_set_halign (wid, GTK_ALIGN_START);
        gtk_widget_set_valign (wid, GTK_ALIGN_CENTER);
        setup_table_attach (table, wid, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        wid = gtk_option_menu_new ();
        menu = gtk_menu_new ();

        for (i = 0; i < 4; i++)
        {
                if (strcmp (id_strings[i], dest) == 0)
                {
                        def = i;
                        break;
                }
        }

        i = 0;
        while (text[i])
        {
                item = gtk_menu_item_new_with_label (_(text[i]));
                g_object_set_data (G_OBJECT (item), "n", GINT_TO_POINTER (i));

                gtk_widget_show (item);
                gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
                g_signal_connect (G_OBJECT (item), "activate",
                                                                G_CALLBACK (setup_id_menu_cb), dest);
                i++;
        }

        gtk_option_menu_set_menu (GTK_OPTION_MENU (wid), menu);
        gtk_option_menu_set_history (GTK_OPTION_MENU (wid), def);

        setup_table_attach (table, wid, 3, 4, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);
}

*/

static void
setup_create_menu (GtkWidget *table, int row, const setting *set)
{
        GtkWidget *wid, *cbox;
        const char **text = (const char **)set->list;
        int i;

        wid = gtk_label_new (_(set->label));
        gtk_widget_set_halign (wid, GTK_ALIGN_START);
        gtk_widget_set_valign (wid, GTK_ALIGN_CENTER);
        setup_table_attach (table, wid, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        cbox = gtk_combo_box_text_new ();
        gtk_label_set_mnemonic_widget (GTK_LABEL (wid), cbox);

        for (i = 0; text[i]; i++)
                gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (cbox), _(text[i]));

        gtk_combo_box_set_active (GTK_COMBO_BOX (cbox),
                                                                          setup_get_int (&setup_prefs, set) - set->extra);
        g_signal_connect (G_OBJECT (cbox), "changed",
                                                        G_CALLBACK (setup_menu_cb), (gpointer)set);

        setup_table_attach (table, cbox, 3, 6, row, row + 1, TRUE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

        if (set->offset == STRUCT_OFFSET_INT (struct zoitechatprefs, hex_text_wrap_align))
        {
                setup_wrap_align_label = wid;
                setup_wrap_align_combo = cbox;
                g_object_add_weak_pointer (G_OBJECT (wid), (gpointer *)&setup_wrap_align_label);
                g_object_add_weak_pointer (G_OBJECT (cbox), (gpointer *)&setup_wrap_align_combo);
                gtk_widget_set_sensitive (wid, !setup_prefs.hex_text_indent);
                gtk_widget_set_sensitive (cbox, !setup_prefs.hex_text_indent);
        }
}

static void
setup_filereq_cb (GtkWidget *entry, char *file)
{
        if (file)
        {
                if (file[0])
                        gtk_entry_set_text (GTK_ENTRY (entry), file);
        }
}

static void
setup_browsefile_response_cb (GtkNativeDialog *dialog, gint response, gpointer user_data)
{
        GtkWidget *entry = user_data;

        if (response == GTK_RESPONSE_ACCEPT)
        {
                char *file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
                setup_filereq_cb (entry, file);
                g_free (file);
        }

        g_object_unref (dialog);
}

static void
setup_browsefile_cb (GtkWidget *button, GtkWidget *entry)
{
        GtkFileChooserNative *dialog;
        GtkFileFilter *filefilter;
        const char *current;
        char *dirname;

        (void)button;
        dialog = gtk_file_chooser_native_new (_("Select an Image File"),
                                              GTK_WINDOW (setup_window),
                                              GTK_FILE_CHOOSER_ACTION_OPEN,
                                              _("_Open"),
                                              _("_Cancel"));
        gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (dialog), TRUE);

        current = gtk_entry_get_text (GTK_ENTRY (entry));
        if (current && current[0])
        {
                dirname = g_path_get_dirname (current);
                if (dirname && dirname[0] && g_file_test (dirname, G_FILE_TEST_IS_DIR))
                        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), dirname);
                else if (g_file_test (current, G_FILE_TEST_IS_DIR))
                        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog), current);
                g_free (dirname);
        }

        filefilter = gtk_file_filter_new ();
#ifdef WIN32
        gtk_file_filter_add_pattern (filefilter, "*.png");
        gtk_file_filter_add_pattern (filefilter, "*.tiff");
        gtk_file_filter_add_pattern (filefilter, "*.gif");
        gtk_file_filter_add_pattern (filefilter, "*.jpeg");
        gtk_file_filter_add_pattern (filefilter, "*.jpg");
#else
        gtk_file_filter_add_mime_type (filefilter, "image/*");
#endif
        gtk_file_filter_set_name (filefilter, _("Images"));
        gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (dialog), filefilter);

        g_signal_connect (G_OBJECT (dialog), "response",
                          G_CALLBACK (setup_browsefile_response_cb), entry);
        gtk_native_dialog_show (GTK_NATIVE_DIALOG (dialog));
}


static void
setup_fontchooser_response (GtkDialog *dialog, gint response, GtkWidget *entry)
{
        if (response == GTK_RESPONSE_OK)
        {
                char *font_name;

                font_name = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (dialog));
                if (font_name)
                {
                        gtk_entry_set_text (GTK_ENTRY (entry), font_name);
                        g_free (font_name);
                }
        }

        gtk_widget_destroy (GTK_WIDGET (dialog));
        font_dialog = NULL;
}

static void
setup_browsefolder_cb (GtkWidget *button, GtkEntry *entry)
{
        gtkutil_file_req (GTK_WINDOW (setup_window), _("Select Download Folder"), setup_filereq_cb, entry, (char*)gtk_entry_get_text (entry), NULL, FRF_CHOOSEFOLDER|FRF_MODAL);
}

static void
setup_browsefont_cb (GtkWidget *button, GtkWidget *entry)
{
        GtkWidget *dialog;
        const char *font_name;

        dialog = gtk_font_chooser_dialog_new (_("Select font"), GTK_WINDOW (setup_window));
	theme_manager_attach_window (dialog);
        font_dialog = dialog;      /* global var */

        gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

        font_name = gtk_entry_get_text (GTK_ENTRY (entry));
        if (font_name[0])
                gtk_font_chooser_set_font (GTK_FONT_CHOOSER (dialog), font_name);

        g_signal_connect (G_OBJECT (dialog), "response",
                                                        G_CALLBACK (setup_fontchooser_response), entry);

        gtk_widget_show (dialog);
}

static void
setup_entry_cb (GtkEntry *entry, setting *set)
{
        int size;
        int pos;
        unsigned char *p = (unsigned char*)gtk_entry_get_text (entry);
        size_t len = strlen ((const char *) p);

        /* need to truncate? */
        if (len >= set->extra)
        {
                len = pos = 0;
                while (1)
                {
                        size = g_utf8_skip [*p];
                        len += size;
                        p += size;
                        /* truncate to "set->extra" BYTES */
                        if (len >= set->extra)
                        {
                                gtk_editable_delete_text (GTK_EDITABLE (entry), pos, -1);
                                break;
                        }
                        pos++;
                }
        }
        else
        {
                setup_set_str (&setup_prefs, set, gtk_entry_get_text (entry));
        }
}

static void
setup_create_label (GtkWidget *table, int row, const setting *set)
{
        setup_table_attach (table, setup_create_italic_label (_(set->label)),
                            2, 6, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);
}

static GtkWidget *
setup_create_entry (GtkWidget *table, int row, const setting *set)
{
        GtkWidget *label;
        GtkWidget *wid, *bwid;

        label = gtk_label_new (_(set->label));
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
        setup_table_attach (table, label, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER,
                            LABEL_INDENT, 0);

        wid = gtk_entry_new ();
        g_object_set_data (G_OBJECT (wid), "lbl", label);
        gtk_label_set_mnemonic_widget (GTK_LABEL (label), wid);
        if (set->list)
                gtk_entry_set_visibility (GTK_ENTRY (wid), FALSE);
        if (set->tooltip)
                gtk_widget_set_tooltip_text (wid, _(set->tooltip));
        gtk_entry_set_max_length (GTK_ENTRY (wid), set->extra - 1);
        gtk_entry_set_text (GTK_ENTRY (wid), setup_get_str (&setup_prefs, set));
        g_signal_connect (G_OBJECT (wid), "changed",
                                                        G_CALLBACK (setup_entry_cb), (gpointer)set);

        if (set->offset == P_OFFSETNL(hex_net_proxy_user))
                proxy_user = wid;
        if (set->offset == P_OFFSETNL(hex_net_proxy_pass))
                proxy_pass = wid; 

        /* only http and Socks5 can auth */
        if ( (set->offset == P_OFFSETNL(hex_net_proxy_pass) ||
                        set->offset == P_OFFSETNL(hex_net_proxy_user)) &&
             (setup_prefs.hex_net_proxy_type != 4 && setup_prefs.hex_net_proxy_type != 3 && setup_prefs.hex_net_proxy_type != 5) )
                gtk_widget_set_sensitive (wid, FALSE);

        if (set->type == ST_ENTRY)
                setup_table_attach (table, wid, 3, 6, row, row + 1, TRUE, FALSE,
                                    SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);
        else
        {
                setup_table_attach (table, wid, 3, 5, row, row + 1, TRUE, FALSE,
                                    SETUP_ALIGN_FILL, SETUP_ALIGN_CENTER, 0, 0);
                bwid = gtk_button_new_with_label (_("Browse..."));
                setup_table_attach (table, bwid, 5, 6, row, row + 1, FALSE, FALSE,
                                    SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);
                if (set->type == ST_EFILE)
                        g_signal_connect (G_OBJECT (bwid), "clicked",
                                                                        G_CALLBACK (setup_browsefile_cb), wid);
                if (set->type == ST_EFONT)
                        g_signal_connect (G_OBJECT (bwid), "clicked",
                                                                        G_CALLBACK (setup_browsefont_cb), wid);
                if (set->type == ST_EFOLDER)
                        g_signal_connect (G_OBJECT (bwid), "clicked",
                                                                        G_CALLBACK (setup_browsefolder_cb), wid);
        }

        return wid;
}

static void
setup_create_button (GtkWidget *table, int row, char *label, GCallback callback)
{
        GtkWidget *but = gtk_button_new_with_label (label);
        setup_table_attach (table, but, 2, 3, row, row + 1, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 5);
        g_signal_connect (G_OBJECT (but), "clicked", callback, NULL);
}

static GtkWidget *
setup_create_frame (void)
{
        GtkWidget *tab;

        tab = gtkutil_grid_new (3, 2, FALSE);
        gtk_widget_set_margin_start (tab, 12);
        gtk_grid_set_row_spacing (GTK_GRID (tab), 8);
        gtk_grid_set_column_spacing (GTK_GRID (tab), 12);

        return tab;
}

static void
open_data_cb (GtkWidget *button, gpointer data)
{
        fe_open_url (get_xdir ());
}

static GtkWidget *
setup_create_page (const setting *set)
{
        int i, row, do_disable;
        GtkWidget *tab = NULL, *box, *frame, *label;
        GtkWidget *wid = NULL, *parentwid = NULL;

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 20);
        gtk_widget_set_valign (box, GTK_ALIGN_START);
        i = row = do_disable = 0;
        while (set[i].type != ST_END)
        {
                switch (set[i].type)
                {
                case ST_HEADER:
                {
                        GtkWidget *heading = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
                        GtkWidget *line = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
                        char *markup = g_markup_printf_escaped ("<b>%s</b>", _(set[i].label));
                        label = gtk_label_new (NULL);
                        gtk_label_set_markup (GTK_LABEL (label), markup);
                        g_free (markup);
                        gtk_widget_set_halign (label, GTK_ALIGN_START);
                        frame = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
                        gtk_box_pack_start (GTK_BOX (heading), label, FALSE, FALSE, 0);
                        gtk_widget_set_valign (line, GTK_ALIGN_CENTER);
                        gtk_box_pack_start (GTK_BOX (heading), line, TRUE, TRUE, 0);
                        gtk_box_pack_start (GTK_BOX (frame), heading, FALSE, FALSE, 0);
                        tab = setup_create_frame ();
                        gtk_box_pack_start (GTK_BOX (frame), tab, FALSE, FALSE, 0);
                        gtk_box_pack_start (GTK_BOX (box), frame, FALSE, FALSE, 0);
                        row = -1;
                        break;
                }
                case ST_EFONT:
                case ST_ENTRY:
                case ST_EFILE:
                case ST_EFOLDER:
                        wid = setup_create_entry (tab, row, &set[i]);
                        break;
                case ST_TOGGLR:
                case ST_TOGGLE:
                        wid = setup_create_toggleL (tab, row, &set[i]);
                        if (set[i].extra)
                                do_disable = set[i].extra;
                        break;
                case ST_3OGGLE:
                        setup_create_3oggle (tab, row, &set[i]);
                        break;
                case ST_MENU:
                        setup_create_menu (tab, row, &set[i]);
                        break;
                case ST_RADIO:
                        setup_create_radio (tab, row, &set[i]);
                        break;
                case ST_NUMBER:
                        wid = setup_create_spin (tab, row, &set[i]);
                        break;
                case ST_HSCALE:
                        setup_create_hscale (tab, row, &set[i]);
                        break;
                case ST_LABEL:
                        setup_create_label (tab, row, &set[i]);
                        break;
                case ST_ALERTHEAD:
                        setup_create_alert_header (tab, row, &set[i]);
                }

                if (do_disable)
                {
                        if (GTK_IS_WIDGET (parentwid))
                        {
                                g_signal_connect (G_OBJECT (parentwid), "toggled", G_CALLBACK(setup_toggle_sensitive_cb), wid);
                                gtk_widget_set_sensitive (wid, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (parentwid)));
                                do_disable--;
                                if (!do_disable)
                                        parentwid = NULL;
                        }
                        else
                                parentwid = wid;
                }

                i++;
                row++;
        }

        if (set == logging_settings)
        {
                setup_create_button (tab, row, _("Open Data Folder"), G_CALLBACK(open_data_cb));
        }

        return box;
}

static GtkWidget *
setup_create_theme_page (void)
{
        return theme_preferences_create_page (GTK_WINDOW (setup_window),
                                                    &setup_prefs,
                                                    &color_change);
}

/* === GLOBALS for sound GUI === */

static GtkWidget *sndfile_entry;
static GtkWidget *setup_sound_tree;
static int ignore_changed = FALSE;

extern struct text_event te[]; /* text.c */
extern char *sound_files[];
static char *setup_sound_files[NUM_XP];
static char *setup_sound_original[NUM_XP];
static gboolean setup_sound_created;
static gboolean setup_sound_pending;

static void
setup_sounds_ensure_created (void)
{
        if (setup_sound_created) return;
        for (int i = 0; i < NUM_XP; i++)
        {
                setup_sound_files[i] = g_strdup (sound_files[i]);
                setup_sound_original[i] = g_strdup (sound_files[i]);
        }
        setup_sound_created = TRUE;
}

static void
setup_alert_sound_refresh (void)
{
        for (guint i = 0; setup_pages && i < setup_pages->len; i++)
        {
                setup_page *page = &g_array_index (setup_pages, setup_page, i);
                if (!strcmp (page->id, "notifications") && page->body)
                        preferences_notifications_refresh_sound (page->body);
        }
}

/* Notifications and Sounds edit the same staged Beep event, even when both
 * pages have already been created. Neither page may reinitialize the snapshot. */
static void
setup_alert_sound_changed (gpointer unused)
{
        GtkTreeModel *model;
        GtkTreeIter iter;
        GtkTreeSelection *selection;
        int event;
        if (!setup_sound_tree) return;
        model = gtk_tree_view_get_model (GTK_TREE_VIEW (setup_sound_tree));
        if (!gtk_tree_model_get_iter_first (model, &iter)) return;
        do
        {
                gtk_tree_model_get (model, &iter, 2, &event, -1);
                if (event == XP_TE_BEEP)
                {
                        gtk_list_store_set (GTK_LIST_STORE (model), &iter, 1,
                                setup_sound_files[XP_TE_BEEP] ? setup_sound_files[XP_TE_BEEP] : "", -1);
                        break;
                }
        } while (gtk_tree_model_iter_next (model, &iter));
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (setup_sound_tree));
        if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        {
                gtk_tree_model_get (model, &iter, 2, &event, -1);
                if (event == XP_TE_BEEP)
                {
                        ignore_changed = TRUE;
                        gtk_entry_set_text (GTK_ENTRY (sndfile_entry),
                                setup_sound_files[XP_TE_BEEP] ? setup_sound_files[XP_TE_BEEP] : "");
                        ignore_changed = FALSE;
                }
        }
}

static void
setup_snd_populate (GtkTreeView * treeview)
{
        GtkListStore *store;
        GtkTreeIter iter;
        GtkTreeSelection *sel;
        GtkTreePath *path;
        int i;

        sel = gtk_tree_view_get_selection (treeview);
        store = (GtkListStore *)gtk_tree_view_get_model (treeview);

        for (i = NUM_XP-1; i >= 0; i--)
        {
                gtk_list_store_prepend (store, &iter);
                if (setup_sound_files[i])
                        gtk_list_store_set (store, &iter, 0, te[i].name, 1, setup_sound_files[i], 2, i, -1);
                else
                        gtk_list_store_set (store, &iter, 0, te[i].name, 1, "", 2, i, -1);
                if (i == last_selected_row)
                {
                        gtk_tree_selection_select_iter (sel, &iter);
                        path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
                        if (path)
                        {
                                gtk_tree_view_scroll_to_cell (treeview, path, NULL, TRUE, 0.5, 0.5);
                                gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
                                gtk_tree_path_free (path);
                        }
                }
        }
}

static int
setup_snd_get_selected (GtkTreeSelection *sel, GtkTreeIter *iter)
{
        int n;
        GtkTreeModel *model;

        if (!gtk_tree_selection_get_selected (sel, &model, iter))
                return -1;

        gtk_tree_model_get (model, iter, 2, &n, -1);
        return n;
}

static void
setup_snd_row_cb (GtkTreeSelection *sel, gpointer user_data)
{
        int n;
        GtkTreeIter iter;

        n = setup_snd_get_selected (sel, &iter);
        if (n == -1)
                return;
        last_selected_row = n;

        ignore_changed = TRUE;
        if (setup_sound_files[n])
                gtk_entry_set_text (GTK_ENTRY (sndfile_entry), setup_sound_files[n]);
        else
                gtk_entry_set_text (GTK_ENTRY (sndfile_entry), "");
        ignore_changed = FALSE;
}

static void
setup_snd_add_columns (GtkTreeView * treeview)
{
        GtkCellRenderer *renderer;
        GtkTreeModel *model;

        /* event column */
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                                                                                                                -1, _("Event"), renderer,
                                                                                                                                "text", 0, NULL);

        /* file column */
        renderer = gtk_cell_renderer_text_new ();
        gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                                                                                                                -1, _("Sound file"), renderer,
                                                                                                                                "text", 1, NULL);

        model = GTK_TREE_MODEL (gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT));
        gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), model);
        g_object_unref (model);
}

static void
setup_snd_filereq_cb (GWeakRef *reference, char *file)
{
        GtkWidget *entry;
        if (!file)
        {
                g_weak_ref_clear (reference);
                g_free (reference);
                return;
        }
        entry = g_weak_ref_get (reference);
        if (entry && entry == sndfile_entry && setup_sound_created && file[0])
        {
                char *directory = g_path_get_dirname (file);
                char *sounds = g_build_filename (get_xdir (), ZOITECHAT_SOUND_DIR, NULL);
                char *basename = g_path_get_basename (file);
                gtk_entry_set_text (GTK_ENTRY (entry), !strcmp (directory, sounds) ? basename : file);
                g_free (directory);
                g_free (sounds);
                g_free (basename);
        }
        if (entry) g_object_unref (entry);
}

static void
setup_snd_browse_cb (GtkWidget *button, GtkEntry *entry)
{
        GWeakRef *reference = g_new0 (GWeakRef, 1);
        char *sounds_dir = g_build_filename (get_xdir (), ZOITECHAT_SOUND_DIR, NULL);
        char *filter = NULL;
        int filter_type;
#ifdef WIN32 /* win32 only supports wav, others could support anything */
        filter = "*.wav";
        filter_type = FRF_EXTENSIONS;
#else
        filter = "audio/*";
        filter_type = FRF_MIMETYPES;
#endif

        g_weak_ref_init (reference, entry);
        gtkutil_file_req (GTK_WINDOW (setup_window), _("Select a sound file"), setup_snd_filereq_cb, reference,
                                                sounds_dir, filter, FRF_MODAL|FRF_FILTERISINITIAL|filter_type);
        g_free (sounds_dir);
}

static void
setup_snd_play_cb (GtkWidget *button, GtkEntry *entry)
{
        sound_play (gtk_entry_get_text (entry), FALSE);
}

static void
setup_snd_changed_cb (GtkEntry *ent, GtkTreeView *tree)
{
        int n;
        GtkTreeIter iter;
        GtkListStore *store;
        GtkTreeSelection *sel;

        if (ignore_changed)
                return;

        sel = gtk_tree_view_get_selection (tree);
        n = setup_snd_get_selected (sel, &iter);
        if (n == -1)
                return;

        /* get the new sound file */
        g_free (setup_sound_files[n]);
        setup_sound_files[n] = g_strdup (gtk_entry_get_text (GTK_ENTRY (ent)));

        /* update the TreeView list */
        store = (GtkListStore *)gtk_tree_view_get_model (tree);
        gtk_list_store_set (store, &iter, 1, setup_sound_files[n], -1);
        if (n == XP_TE_BEEP) setup_alert_sound_refresh ();
}

static GtkWidget *
setup_create_sound_page (void)
{
        GtkWidget *vbox1;
        GtkWidget *vbox2;
        GtkWidget *scrolledwindow1;
        GtkWidget *sound_tree;
        GtkWidget *table1;
        GtkWidget *sound_label;
        GtkWidget *sound_browse;
        GtkWidget *sound_play;
        GtkTreeSelection *sel;

        setup_sounds_ensure_created ();
        vbox1 = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
        gtk_container_set_border_width (GTK_CONTAINER (vbox1), 6);
        gtk_widget_show (vbox1);

        vbox2 = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 0);
        gtk_widget_show (vbox2);
        gtk_box_pack_start (GTK_BOX (vbox1), vbox2, TRUE, TRUE, 0);

        scrolledwindow1 = gtk_scrolled_window_new (NULL, NULL);
        gtk_widget_show (scrolledwindow1);
        gtk_box_pack_start (GTK_BOX (vbox2), scrolledwindow1, TRUE, TRUE, 0);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow1),
                                                                                          GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
        gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolledwindow1),
                                                                                                         GTK_SHADOW_IN);

        sound_tree = gtk_tree_view_new ();
        setup_sound_tree = sound_tree;
        sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (sound_tree));
        gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
        setup_snd_add_columns (GTK_TREE_VIEW (sound_tree));
        setup_snd_populate (GTK_TREE_VIEW (sound_tree));
        g_signal_connect (G_OBJECT (sel), "changed",
                                                        G_CALLBACK (setup_snd_row_cb), NULL);
        gtk_widget_show (sound_tree);
        gtk_container_add (GTK_CONTAINER (scrolledwindow1), sound_tree);
        gtk_tree_view_set_grid_lines (GTK_TREE_VIEW (sound_tree), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);

        table1 = gtkutil_grid_new (2, 3, FALSE);
        gtk_widget_show (table1);
        gtk_box_pack_start (GTK_BOX (vbox2), table1, FALSE, TRUE, 8);
        gtk_grid_set_row_spacing (GTK_GRID (table1), 2);
        gtk_grid_set_column_spacing (GTK_GRID (table1), 4);

        sound_label = gtk_label_new_with_mnemonic (_("Sound file:"));
        gtk_widget_show (sound_label);
        setup_table_attach (table1, sound_label, 0, 1, 0, 1, FALSE, FALSE,
                            SETUP_ALIGN_START, SETUP_ALIGN_CENTER, 0, 0);

        sndfile_entry = gtk_entry_new ();
        g_signal_connect (G_OBJECT (sndfile_entry), "changed",
                                                        G_CALLBACK (setup_snd_changed_cb), sound_tree);
        gtk_widget_show (sndfile_entry);
        setup_table_attach (table1, sndfile_entry, 0, 1, 1, 2, TRUE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

        sound_browse = gtk_button_new_with_mnemonic (_("_Browse..."));
        g_signal_connect (G_OBJECT (sound_browse), "clicked",
                                                        G_CALLBACK (setup_snd_browse_cb), sndfile_entry);
        gtk_widget_show (sound_browse);
        setup_table_attach (table1, sound_browse, 1, 2, 1, 2, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

        sound_play = gtkutil_button_new_from_stock ("gtk-media-play", _("_Play"));
        g_signal_connect (G_OBJECT (sound_play), "clicked",
                                                        G_CALLBACK (setup_snd_play_cb), sndfile_entry);
        gtk_widget_show (sound_play);
        setup_table_attach (table1, sound_play, 2, 3, 1, 2, FALSE, FALSE,
                            SETUP_ALIGN_FILL, SETUP_ALIGN_FILL, 0, 0);

        setup_snd_row_cb (sel, NULL);

        return vbox1;
}

static GtkWidget *
setup_page_from_settings (const setting *settings)
{
        return setup_create_page (settings);
}

static GtkWidget *
setup_page_from_text (const setting *settings)
{
        (void) settings;
        return preferences_text_page_new (&setup_prefs,
#ifdef WIN32
                langsmenu
#else
                NULL
#endif
        );
}

static GtkWidget *
setup_page_from_tabs (const setting *settings)
{
        (void) settings;
        return preferences_tabs_page_new (&setup_prefs);
}

static GtkWidget *
setup_page_from_history (const setting *settings)
{
        (void) settings;
        return preferences_history_page_new (&setup_prefs, get_xdir ());
}

static void
setup_typing_preview_update (GtkWidget *widget, const struct zoitechatprefs *staged)
{
        SexySpellEntry *entry = SEXY_SPELL_ENTRY (widget);
        const char *previous = g_object_get_data (G_OBJECT (widget), "preview-spelling-languages");
        if (g_strcmp0 (previous, staged->hex_text_spell_langs))
        {
                GSList *available = sexy_spell_entry_get_languages (entry);
                char **tokens = g_strsplit_set (staged->hex_text_spell_langs, ", \t", -1);
                gboolean loaded = FALSE;
                sexy_spell_entry_deactivate_language (entry, NULL);
                for (int i = 0; tokens[i]; i++)
                        for (GSList *item = available; item; item = item->next)
                                if (!strcmp (item->data, tokens[i]))
                                        loaded |= sexy_spell_entry_activate_language (entry, tokens[i], NULL);
                if (!loaded)
                        sexy_spell_entry_activate_language (entry, "en", NULL);
                g_slist_free_full (available, g_free);
                g_strfreev (tokens);
                g_object_set_data_full (G_OBJECT (widget), "preview-spelling-languages", g_strdup (staged->hex_text_spell_langs), g_free);
        }
        sexy_spell_entry_set_checked (entry, staged->hex_gui_input_spell);
        sexy_spell_entry_set_parse_attributes (entry, staged->hex_gui_input_attr);
}

static gint
setup_spell_language_compare (gconstpointer a, gconstpointer b)
{
        const PreferencesSpellLanguage *left = a, *right = b;
        return g_utf8_collate (left->name, right->name);
}

static GtkWidget *
setup_page_from_typing (const setting *settings)
{
        (void) settings;
        GtkWidget *preview = sexy_spell_entry_new ();
        GSList *available = sexy_spell_entry_get_languages (SEXY_SPELL_ENTRY (preview));
        GArray *languages = g_array_new (FALSE, FALSE, sizeof (PreferencesSpellLanguage));
        GHashTable *seen = g_hash_table_new (g_str_hash, g_str_equal);
        for (GSList *item = available; item; item = item->next)
        {
                if (g_hash_table_contains (seen, item->data)) continue;
                PreferencesSpellLanguage language = {g_strdup (item->data),
                        sexy_spell_entry_get_language_name (SEXY_SPELL_ENTRY (preview), item->data)};
                if (!language.name || !*language.name)
                {
                        g_free ((char *)language.name);
                        language.name = g_strdup (item->data);
                }
                g_hash_table_add (seen, (gpointer)language.code);
                g_array_append_val (languages, language);
        }
        g_array_sort (languages, setup_spell_language_compare);
        GtkWidget *page = preferences_typing_page_new (&setup_prefs, preview,
                (PreferencesSpellLanguage *)languages->data, languages->len, setup_typing_preview_update);
        g_hash_table_destroy (seen);
        for (guint i = 0; i < languages->len; i++)
        {
                PreferencesSpellLanguage *language = &g_array_index (languages, PreferencesSpellLanguage, i);
                g_free ((char *)language->code); g_free ((char *)language->name);
        }
        g_array_unref (languages);
        g_slist_free_full (available, g_free);
        return page;
}

static GtkWidget *
setup_page_from_notifications (const setting *settings)
{
        PreferencesAlertCapabilities capabilities = 0;
        /* Keep the established platform/backend capability decisions and the
         * legacy descriptors for search aliases, without duplicating policy. */
        for (int i = 0; settings[i].type != ST_END; i++)
        {
                if (settings[i].type == ST_3OGGLE)
                {
                        if (settings[i].list == (void *)balloonlist) capabilities |= PREFERENCES_ALERT_DESKTOP;
                        if (settings[i].list == (void *)trayblinklist) capabilities |= PREFERENCES_ALERT_TRAY;
                        if (settings[i].list == (void *)taskbarlist) capabilities |= PREFERENCES_ALERT_ATTENTION;
                }
                if (settings[i].type == ST_TOGGLE && settings[i].offset == P_OFFINTNL(hex_gui_tray))
                        capabilities |= PREFERENCES_ALERT_TRAY_BEHAVIOR;
                if (settings[i].type == ST_TOGGLE && settings[i].offset == P_OFFINTNL(hex_gui_tray_quiet))
                        capabilities |= PREFERENCES_ALERT_HIDDEN_ONLY;
        }
        setup_sounds_ensure_created ();
        return preferences_notifications_page_new (&setup_prefs, capabilities,
                &setup_sound_files[XP_TE_BEEP], setup_alert_sound_changed, NULL);
}

static GtkWidget *
setup_page_from_theme (const setting *settings)
{
        (void)settings;
        return setup_create_theme_page ();
}

static GtkWidget *
setup_page_from_sound (const setting *settings)
{
        (void)settings;
        return setup_create_sound_page ();
}

static void
setup_search_append (GString *text, const char *value)
{
        if (value)
        {
                g_string_append_c (text, ' ');
                g_string_append (text, _(value));
        }
}

static char *
setup_search_normalize (const char *text)
{
        char *folded = g_utf8_casefold (text, -1);
        char *normalized = g_utf8_normalize (folded, -1, G_NORMALIZE_ALL_COMPOSE);
        g_free (folded);
        return normalized;
}

static void
setup_register_page (const char *id, const char *group, const char *title, const char *description,
                     setup_page_factory factory, const setting *settings,
                     const char *keywords)
{
        setup_page page = {0};
        GtkWidget *box, *heading, *label, *sw;
        GString *text = g_string_new ("");
        char *markup;
        int i, j;

        page.id = id;
        page.group = group;
        page.title = title;
        page.description = description;
        page.factory = factory;
        page.settings = settings;
        setup_search_append (text, group);
        setup_search_append (text, title);
        setup_search_append (text, description);
        setup_search_append (text, keywords);
        for (i = 0; settings && settings[i].type != ST_END; i++)
        {
                setup_search_append (text, settings[i].label);
                setup_search_append (text, settings[i].tooltip);
                if (settings[i].type == ST_MENU || settings[i].type == ST_RADIO)
                        for (j = 0; settings[i].list && settings[i].list[j]; j++)
                                setup_search_append (text, settings[i].list[j]);
        }
        page.search_text = setup_search_normalize (text->str);
        g_string_free (text, TRUE);

        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_set_border_width (GTK_CONTAINER (box), 16);
        heading = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        gtk_box_pack_start (GTK_BOX (box), heading, FALSE, FALSE, 0);
        label = gtk_label_new (NULL);
        markup = g_markup_printf_escaped ("<b><big>%s</big></b>", _(title));
        gtk_label_set_markup (GTK_LABEL (label), markup);
        g_free (markup);
        gtk_widget_set_halign (label, GTK_ALIGN_START);
        gtk_widget_set_tooltip_text (label, _(description));
        gtk_box_pack_start (GTK_BOX (heading), label, FALSE, FALSE, 0);
        label = gtk_label_new (_(description));
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (label), 64);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        gtk_box_pack_start (GTK_BOX (heading), label, FALSE, FALSE, 0);
        page.container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        gtk_box_pack_start (GTK_BOX (box), page.container, TRUE, TRUE, 0);

        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (sw), box);
        gtk_notebook_append_page (GTK_NOTEBOOK (setup_book), sw, NULL);
        g_array_append_val (setup_pages, page);
}

static void
setup_ensure_page_created (int index)
{
        setup_page *page;
        GtkWidget *widget;
        if (index < 0 || index >= (int)setup_pages->len)
                return;
        page = &g_array_index (setup_pages, setup_page, index);
        if (g_object_get_data (G_OBJECT (page->container), "setup-created"))
                return;
        widget = page->factory (page->settings);
        page->body = widget;
        gtk_box_pack_start (GTK_BOX (page->container), widget, TRUE, TRUE, 0);
        g_object_set_data (G_OBJECT (page->container), "setup-created", GINT_TO_POINTER (1));
        gtk_widget_show_all (page->container);
}

static GtkWidget *
setup_page_from_keyboard (const setting *settings)
{
        (void)settings;
        return key_preferences_create ();
}

static GtkWidget *
setup_page_from_events (const setting *settings)
{
        (void)settings;
        return pevent_preferences_create ();
}

static GtkWidget *
setup_page_from_replace (const setting *settings)
{
        (void)settings;
        return menu_preferences_create ("replace");
}

static GtkWidget *
setup_page_from_menus (const setting *settings)
{
        (void)settings;
        return menu_preferences_create ("menus");
}

static GtkWidget *
setup_page_from_commands (const setting *settings)
{
        (void)settings;
        return menu_preferences_create ("commands");
}

static GtkWidget *
setup_page_from_urls (const setting *settings)
{
        (void)settings;
        return menu_preferences_create ("urls");
}

static GtkWidget *
setup_page_from_ctcp (const setting *settings)
{
        (void)settings;
        return menu_preferences_create ("ctcp");
}

static GtkWidget *
setup_page_from_ignore (const setting *settings)
{
        (void)settings;
        return ignore_preferences_create ();
}

#ifdef USE_PLUGIN
static GtkWidget *
setup_page_from_plugins (const setting *settings)
{
        (void)settings;
        return plugingui_preferences_create ();
}
#endif

static GtkWidget *
setup_create_pages (GtkWidget *box)
{
        const setting *alerts;
        gboolean tray = gtkutil_tray_icon_supported (GTK_WINDOW (setup_window));
        gboolean notifications = notification_backend_supported ();

        setup_pages = g_array_new (FALSE, TRUE, sizeof (setup_page));
        setup_book = gtk_notebook_new ();
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (setup_book), FALSE);
        gtk_notebook_set_show_border (GTK_NOTEBOOK (setup_book), FALSE);
        gtk_box_pack_start (GTK_BOX (box), setup_book, TRUE, TRUE, 0);

        setup_register_page ("text", N_("Interface"), N_("Text & layout"),
                N_("Fonts, conversation text, timestamps and window information."),
                setup_page_from_text, appearance_settings, N_("Conversation preview timestamp time style 24-hour 12-hour custom format unread nickname alignment"));
        setup_register_page ("appearance", N_("Interface"), N_("Appearance"),
                N_("Application theme, chat colours and message formatting."),
                setup_page_from_theme, NULL, N_("Themes appearance conversation colors colours palette GTK3 import remove colors.conf dark light contrast nickname text background foreground hex reset"));
        setup_register_page ("effects", N_("Interface"), N_("Effects & scrolling"),
                N_("Background images, window opacity and mouse wheel behavior."),
                setup_page_from_settings, appearance_advanced_settings, NULL);
        setup_register_page ("typing", N_("Interface"), N_("Typing & spelling"),
                N_("The message input, spell checking and nickname completion."),
                setup_page_from_typing, inputbox_settings, N_("Language dictionaries installed unavailable codes search typing preview completion example suffix threshold matching names"));
        setup_register_page ("user-list", N_("Interface"), N_("User list"),
                N_("How people are displayed and sorted in a channel."),
                setup_page_from_settings, userlist_settings, NULL);
        setup_register_page ("tabs", N_("Interface"), N_("Channels & tabs"),
                N_("Channel navigation, tab placement and activity indicators."),
                setup_page_from_tabs, tabs_settings, N_("Sidebar tree navigation placement preview opening closing conversations"));

        setup_register_page ("keyboard", N_("Interface"), N_("Keyboard shortcuts"),
                N_("Keyboard shortcuts and the actions they run."),
                setup_page_from_keyboard, NULL, N_("Key accelerator action data reset add delete binding shortcuts search category record duplicate shared chain order parameters"));
        setup_register_page ("menus", N_("Interface"), N_("Menus & buttons"),
                N_("Customize user list buttons, context menus, dialog buttons and the user menu."),
                setup_page_from_menus, NULL, N_("Popup command add delete reorder formatting"));

        setup_register_page ("messages", N_("Chatting"), N_("Messages & away"),
                N_("Default messages, away behavior and channel events."),
                setup_page_from_settings, general_settings, NULL);
        alerts = tray ? (notifications ? alert_settings : alert_settings_nonotifications)
                      : (notifications ? alert_settings_unity : alert_settings_unityandnonotifications);
        setup_register_page ("replace", N_("Chatting"), N_("Text replacements"),
                N_("Automatically replace words as you type. Add, edit or reorder your replacements."),
                setup_page_from_replace, NULL, N_("Auto replace text expansion"));
        setup_register_page ("events", N_("Chatting"), N_("Message formatting"),
                N_("Edit text event templates with argument help, import/export and a preview."),
                setup_page_from_events, NULL, N_("Text events format message join part quit highlight load save test"));
        setup_register_page ("notifications", N_("Chatting"), N_("Notifications"),
                N_("Highlights, desktop alerts and system tray behavior."),
                setup_page_from_notifications, alerts, N_("Mentions quiet rules test notification alert system sound beep file"));
        setup_register_page ("sounds", N_("Chatting"), N_("Sounds"),
                N_("Assign and preview a sound for each chat event."),
                setup_page_from_sound, NULL, N_("Sound file browse play"));
        setup_register_page ("logging", N_("Chatting"), N_("History & logging"),
                N_("Saved conversations, scrollback and collected links."),
                setup_page_from_history, logging_settings, N_("Conversation history buffer restore saved logs folder location filename format daily monthly date timestamp preview collected links URL grabber unlimited"));
        setup_register_page ("advanced", N_("Chatting"), N_("Behavior & timing"),
                N_("Clipboard behavior, display options and connection timing."),
                setup_page_from_settings, advanced_settings, NULL);

        setup_register_page ("connection", N_("Network"), N_("Connection"),
                N_("Your address, proxy, connection health and remote media."),
                setup_page_from_settings, network_settings, NULL);
        setup_register_page ("transfers", N_("Network"), N_("File transfers"),
                N_("Download folders, incoming files and transfer speed limits."),
                setup_page_from_settings, filexfer_settings, NULL);
        setup_register_page ("identd", N_("Network"), N_("Identd"),
                N_("The optional identification server and its listening port."),
                setup_page_from_settings, identd_settings, NULL);
        setup_register_page ("ignore", N_("Privacy"), N_("Ignore rules"),
                N_("Manage ignored masks and message types. Ignore changes apply immediately."),
                setup_page_from_ignore, NULL, N_("Ignore list mask private channel CTCP DCC notice invite stats"));
        setup_register_page ("commands", N_("Advanced"), N_("Commands & aliases"),
                N_("Define your own commands and expansions using the documented formatting codes."),
                setup_page_from_commands, NULL, N_("User commands name expansion special codes"));
        setup_register_page ("urls", N_("Advanced"), N_("Link handling"),
                N_("Choose commands and applications used to open links."),
                setup_page_from_urls, NULL, N_("URL handlers shell browser command"));
        setup_register_page ("ctcp", N_("Advanced"), N_("CTCP replies"),
                N_("Customize replies to client-to-client requests."),
                setup_page_from_ctcp, NULL, N_("CTCP VERSION PING TIME reply request"));
#ifdef USE_PLUGIN
        setup_register_page ("plugins", N_("Extensions"), N_("Plugins & scripts"),
                N_("Load, unload and reload extensions. These actions take effect immediately."),
                setup_page_from_plugins, NULL, N_("Plugins scripts addons Python Perl Lua load unload reload"));
#endif
        return setup_book;
}

static void
setup_scroll_selected_row (GtkWidget *widget, gpointer data)
{
        GtkListBoxRow *row;
        GtkWidget *scroll;
        GtkAdjustment *adjustment;
        int x, y;
        (void)widget;
        (void)data;
        if (!setup_sidebar || !gtk_widget_get_mapped (setup_sidebar)) return;
        row = gtk_list_box_get_selected_row (GTK_LIST_BOX (setup_sidebar));
        if (!row || !gtk_widget_get_child_visible (GTK_WIDGET (row))) return;
        scroll = gtk_widget_get_ancestor (setup_sidebar, GTK_TYPE_SCROLLED_WINDOW);
        if (!scroll || !gtk_widget_translate_coordinates (GTK_WIDGET (row), setup_sidebar, 0, 0, &x, &y)) return;
        adjustment = gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (scroll));
        gtk_adjustment_clamp_page (adjustment, y, y + gtk_widget_get_allocated_height (GTK_WIDGET (row)));
}

static void
setup_sidebar_size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
        setup_scroll_selected_row (widget, data);
}

static void
setup_sidebar_selected (GtkListBox *list, GtkListBoxRow *row, gpointer data)
{
        int page;
        (void)list;
        (void)data;
        if (!row)
                return;
        page = gtk_list_box_row_get_index (row);
        setup_ensure_page_created (page);
        gtk_notebook_set_current_page (GTK_NOTEBOOK (setup_book), page);
        setup_scroll_selected_row (GTK_WIDGET (list), NULL);
}

void
setup_reveal_widget (GtkWidget *widget)
{
        guint i;
        GtkWidget *child, *parent;
        if (!setup_pages || !widget) return;
        for (i = 0; i < setup_pages->len; i++)
        {
                setup_page *page = &g_array_index (setup_pages, setup_page, i);
                if (widget == page->container || gtk_widget_is_ancestor (widget, page->container))
                {
                        gtk_entry_set_text (GTK_ENTRY (setup_search), "");
                        gtk_list_box_select_row (GTK_LIST_BOX (setup_sidebar), GTK_LIST_BOX_ROW (page->row));
                        for (child = widget; (parent = gtk_widget_get_parent (child)); child = parent)
                        {
                                if (GTK_IS_EXPANDER (parent))
                                        gtk_expander_set_expanded (GTK_EXPANDER (parent), TRUE);
                                if (GTK_IS_STACK (parent))
                                {
                                        GtkWidget *selector = g_object_get_data (G_OBJECT (parent), "preferences-selector");
                                        if (selector)
                                        {
                                                char *name;
                                                gtk_container_child_get (GTK_CONTAINER (parent), child, "name", &name, NULL);
                                                gtk_combo_box_set_active_id (GTK_COMBO_BOX (selector), name);
                                                g_free (name);
                                        }
                                        else gtk_stack_set_visible_child (GTK_STACK (parent), child);
                                }
                        }
                        setup_scroll_selected_row (setup_sidebar, NULL);
                        return;
                }
        }
}

static gboolean
setup_sidebar_filter (GtkListBoxRow *row, gpointer data)
{
        setup_page *page = &g_array_index (setup_pages, setup_page, gtk_list_box_row_get_index (row));
        char **words;
        gboolean matches = TRUE;
        int i;
        (void)data;
        if (!setup_query || !*setup_query)
                return TRUE;
        words = g_strsplit_set (setup_query, " \t\r\n", -1);
        for (i = 0; words[i]; i++)
                if (*words[i] && !strstr (page->search_text, words[i]))
                        matches = FALSE;
        g_strfreev (words);
        return matches;
}

static void
setup_sidebar_header (GtkListBoxRow *row, GtkListBoxRow *before, gpointer data)
{
        setup_page *page = &g_array_index (setup_pages, setup_page, gtk_list_box_row_get_index (row));
        setup_page *previous = before ? &g_array_index (setup_pages, setup_page, gtk_list_box_row_get_index (before)) : NULL;
        GtkWidget *label;
        char *markup;
        (void)data;
        gtk_list_box_row_set_header (row, NULL);
        if (previous && !strcmp (page->group, previous->group))
                return;
        label = gtk_label_new (NULL);
        markup = g_markup_printf_escaped ("<b>%s</b>", _(page->group));
        gtk_label_set_markup (GTK_LABEL (label), markup);
        g_free (markup);
        gtk_label_set_xalign (GTK_LABEL (label), 0);
        gtk_widget_set_margin_start (label, 12);
        gtk_widget_set_margin_top (label, before ? 18 : 10);
        gtk_widget_set_margin_bottom (label, 6);
        gtk_style_context_add_class (gtk_widget_get_style_context (label), "dim-label");
        gtk_widget_show (label);
        gtk_list_box_row_set_header (row, label);
}

static void
setup_search_changed (GtkEditable *entry, gpointer data)
{
        GtkListBoxRow *selected;
        GtkListBoxRow *first = NULL;
        guint i;
        (void)data;
        g_free (setup_query);
        setup_query = setup_search_normalize (gtk_entry_get_text (GTK_ENTRY (entry)));
        gtk_list_box_invalidate_filter (GTK_LIST_BOX (setup_sidebar));
        selected = gtk_list_box_get_selected_row (GTK_LIST_BOX (setup_sidebar));
        for (i = 0; i < setup_pages->len; i++)
        {
                GtkListBoxRow *row = GTK_LIST_BOX_ROW (g_array_index (setup_pages, setup_page, i).row);
                if (setup_sidebar_filter (row, NULL) && !first)
                        first = row;
        }
        gtk_widget_set_visible (setup_book, first != NULL);
        gtk_widget_set_visible (setup_empty, first == NULL);
        if (first && (!selected || !setup_sidebar_filter (selected, NULL)))
                gtk_list_box_select_row (GTK_LIST_BOX (setup_sidebar), first);
}

static void
setup_search_activate (GtkEntry *entry, gpointer data)
{
        GtkListBoxRow *row = gtk_list_box_get_selected_row (GTK_LIST_BOX (setup_sidebar));
        (void)entry;
        (void)data;
        if (row && setup_sidebar_filter (row, NULL))
                gtk_widget_child_focus (setup_book, GTK_DIR_TAB_FORWARD);
}

static void
setup_search_clear (GtkButton *button, gpointer data)
{
        (void)button;
        (void)data;
        gtk_entry_set_text (GTK_ENTRY (setup_search), "");
        gtk_widget_grab_focus (setup_search);
}

static gboolean
setup_key_press (GtkWidget *win, GdkEventKey *event, gpointer data)
{
        (void)data;
        if ((event->state & GDK_CONTROL_MASK) &&
            (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F))
        {
                gtk_widget_grab_focus (setup_search);
                gtk_editable_select_region (GTK_EDITABLE (setup_search), 0, -1);
                return TRUE;
        }
        if (event->keyval == GDK_KEY_Escape)
        {
                if (*gtk_entry_get_text (GTK_ENTRY (setup_search)))
                        setup_search_clear (NULL, NULL);
                else if (gtk_widget_get_sensitive (cancel_button))
                        gtk_widget_destroy (win);
                return TRUE;
        }
        return FALSE;
}

static void
setup_create_sidebar (GtkWidget *box)
{
        GtkWidget *sidebar, *sw, *label, *separator;
        guint i;
        sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width (GTK_CONTAINER (sidebar), 10);
        gtk_widget_set_size_request (sidebar, 220, -1);
        setup_search = gtk_search_entry_new ();
        gtk_entry_set_placeholder_text (GTK_ENTRY (setup_search), _("Search settings"));
        gtk_widget_set_tooltip_text (setup_search, _("Search page names, settings and help text. Ctrl+F to search."));
        atk_object_set_name (gtk_widget_get_accessible (setup_search), _("Search settings"));
        gtk_box_pack_start (GTK_BOX (sidebar), setup_search, FALSE, FALSE, 0);
        setup_sidebar = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (setup_sidebar), GTK_SELECTION_BROWSE);
        gtk_list_box_set_header_func (GTK_LIST_BOX (setup_sidebar), setup_sidebar_header, NULL, NULL);
        gtk_list_box_set_filter_func (GTK_LIST_BOX (setup_sidebar), setup_sidebar_filter, NULL, NULL);
        gtk_style_context_add_class (gtk_widget_get_style_context (setup_sidebar), "sidebar");
        atk_object_set_name (gtk_widget_get_accessible (setup_sidebar), _("Preferences pages"));
        for (i = 0; i < setup_pages->len; i++)
        {
                setup_page *page = &g_array_index (setup_pages, setup_page, i);
                page->row = gtk_list_box_row_new ();
                label = gtk_label_new (_(page->title));
                gtk_label_set_xalign (GTK_LABEL (label), 0);
                gtk_widget_set_margin_start (label, 12);
                gtk_widget_set_margin_end (label, 12);
                gtk_widget_set_margin_top (label, 9);
                gtk_widget_set_margin_bottom (label, 9);
                gtk_container_add (GTK_CONTAINER (page->row), label);
                gtk_container_add (GTK_CONTAINER (setup_sidebar), page->row);
        }
        sw = gtk_scrolled_window_new (NULL, NULL);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add (GTK_CONTAINER (sw), setup_sidebar);
        gtk_box_pack_start (GTK_BOX (sidebar), sw, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (box), sidebar, FALSE, FALSE, 0);
        gtk_box_reorder_child (GTK_BOX (box), sidebar, 0);
        separator = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
        gtk_box_pack_start (GTK_BOX (box), separator, FALSE, FALSE, 0);
        gtk_box_reorder_child (GTK_BOX (box), separator, 1);
        g_signal_connect (setup_sidebar, "row-selected", G_CALLBACK (setup_sidebar_selected), NULL);
        g_signal_connect (setup_sidebar, "map", G_CALLBACK (setup_scroll_selected_row), NULL);
        g_signal_connect (setup_sidebar, "size-allocate", G_CALLBACK (setup_sidebar_size_allocate), NULL);
        g_signal_connect (setup_search, "changed", G_CALLBACK (setup_search_changed), NULL);
        g_signal_connect (setup_search, "activate", G_CALLBACK (setup_search_activate), NULL);
        gtk_list_box_select_row (GTK_LIST_BOX (setup_sidebar),
                gtk_list_box_get_row_at_index (GTK_LIST_BOX (setup_sidebar), 0));
}

static void
setup_apply_to_sess (session_gui *gui)
{
        mg_update_xtext (gui->xtext);

        mg_apply_session_font_prefs (gui);

        if (prefs.hex_gui_ulist_buttons)
                gtk_widget_show (gui->button_box);
        else
                gtk_widget_hide (gui->button_box);

        /* update active languages */
        sexy_spell_entry_deactivate_language((SexySpellEntry *)gui->input_box,NULL);
        sexy_spell_entry_activate_default_languages((SexySpellEntry *)gui->input_box);

        sexy_spell_entry_set_checked ((SexySpellEntry *)gui->input_box, prefs.hex_gui_input_spell);
        sexy_spell_entry_set_parse_attributes ((SexySpellEntry *)gui->input_box, prefs.hex_gui_input_attr);
}

static void
unslash (char *dir)
{
        if (dir[0])
        {
                size_t len = strlen (dir) - 1;
#ifdef WIN32
                if (dir[len] == '/' || dir[len] == '\\')
#else
                if (dir[len] == '/')
#endif
                        dir[len] = 0;
        }
}

void
setup_apply_real (const ThemeChangedEvent *event)
{
        GSList *list;
        session *sess;
        int done_main = FALSE;

        /* remove trailing slashes */
        unslash (prefs.hex_dcc_dir);
        unslash (prefs.hex_dcc_completed_dir);

        g_mkdir (prefs.hex_dcc_dir, 0700);
        g_mkdir (prefs.hex_dcc_completed_dir, 0700);

        if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_PIXMAP))
        {
                if (channelwin_pix)
                        cairo_surface_destroy (channelwin_pix);
                channelwin_pix = pixmap_load_from_file (prefs.hex_text_background);
        }

        theme_manager_reload_input_style ();

        list = sess_list;
        while (list)
        {
                sess = list->data;
                if (sess->gui->is_tab)
                {
                        /* only apply to main tabwindow once */
                        if (!done_main)
                        {
                                done_main = TRUE;
                                setup_apply_to_sess (sess->gui);
                        }
                } else
                {
                        setup_apply_to_sess (sess->gui);
                }

                log_open_or_close (sess);

                if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_USERLIST))
                        userlist_rehash (sess);

                list = list->next;
        }

        mg_apply_setup ();
        menu_update_quit_accel ();
        tray_apply_setup ();
        zoitechat_reinit_timers ();

        if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_LAYOUT))
                menu_change_layout ();

        if (theme_changed_event_has_reason (event, THEME_CHANGED_REASON_IDENTD))
                handle_command (current_sess, "IDENTD reload", FALSE);
}

static void
setup_apply (struct zoitechatprefs *pr)
{
#ifdef WIN32
        PangoFontDescription *old_desc;
        PangoFontDescription *new_desc;
        char buffer[4 * FONTNAMELEN + 1];
#endif
	int noapply = FALSE;
	ThemeChangedEvent event;
	struct zoitechatprefs old_prefs = prefs;
	int old_dark_mode = prefs.hex_gui_dark_mode;
	mg_live_prefs live_changes;

	memset (&live_changes, 0, sizeof (live_changes));

#define DIFF(a) (pr->a != prefs.a)

#ifdef WIN32
        if (DIFF (hex_gui_lang))
                noapply = TRUE;
#endif
        /* server tabs are only created when a connection is opened, so
           existing connections keep their current tabs until a restart;
           the channel tree indentation is still re-applied below */
        if (DIFF (hex_gui_tab_server))
        {
                noapply = TRUE;
                live_changes.chanview = TRUE;
        }

        if (DIFF (hex_gui_compact))
        {
                live_changes.chanview = TRUE;
                live_changes.ulist_columns = TRUE;
        }
        if (DIFF (hex_gui_input_icon) || DIFF (hex_gui_input_nick))
                live_changes.input_box = TRUE;
        if (DIFF (hex_gui_lagometer) || DIFF (hex_gui_throttlemeter))
                live_changes.meters = TRUE;
        if (DIFF (hex_gui_mode_buttons_inline) || DIFF (hex_gui_topicbar_multiline))
                live_changes.topic_bar = TRUE;
        if (DIFF (hex_gui_tab_icons) || DIFF (hex_gui_tab_closebuttons) ||
                 DIFF (hex_gui_tab_small))
                live_changes.chanview = TRUE;
        if (pr->hex_gui_tab_layout == 0 &&
                strcmp (pr->hex_gui_tab_font, prefs.hex_gui_tab_font) != 0)
                live_changes.chanview = TRUE;
        if (DIFF (hex_gui_tab_sort))
                live_changes.tab_resort = TRUE;
        if (DIFF (hex_gui_tab_trunc))
                live_changes.tab_trunc = TRUE;
        if (DIFF (hex_gui_ulist_count))
                live_changes.ulist_count = TRUE;
        if (DIFF (hex_gui_ulist_icons))
                live_changes.ulist_rows = TRUE;
        if (DIFF (hex_gui_ulist_show_hosts))
        {
                live_changes.ulist_columns = TRUE;
                live_changes.ulist_rows = TRUE; /* hostnames may be stale */
        }
        if (DIFF (hex_gui_ulist_sort))
                live_changes.ulist_sort = TRUE;
        /* compare against the pre-dialog value: the slider previews the
           opacity live on the current window only, so OK still needs to
           apply it to the other windows */
        if (pr->hex_gui_transparency != setup_transparency_snapshot)
                live_changes.transparency = TRUE;

        if ((pr->hex_gui_tab_pos == 5 || pr->hex_gui_tab_pos == 6) &&
                 pr->hex_gui_tab_layout == 2 && pr->hex_gui_tab_pos != prefs.hex_gui_tab_pos)
                fe_message (_("You cannot place the tree on the top or bottom!\n"
                                                "Please change to the <b>Tabs</b> layout in the <b>View</b>"
                                                " menu first."),
                                                FE_MSG_WARN | FE_MSG_MARKUP);

        /* format cannot be blank, there is already a setting for this */
        if (pr->hex_stamp_text_format[0] == 0)
        {
                pr->hex_stamp_text = 0;
                strcpy (pr->hex_stamp_text_format, prefs.hex_stamp_text_format);
        }

        memcpy (&prefs, pr, sizeof (prefs));

	event = theme_manager_on_preferences_changed (&old_prefs, &prefs, old_dark_mode, &color_change);

#ifdef WIN32
        /* merge hex_font_main and hex_font_alternative into hex_font_normal */
        old_desc = pango_font_description_from_string (prefs.hex_text_font_main);
        sprintf (buffer, "%s,%s", pango_font_description_get_family (old_desc), prefs.hex_text_font_alternative);
        new_desc = pango_font_description_from_string (buffer);
        pango_font_description_set_weight (new_desc, pango_font_description_get_weight (old_desc));
        pango_font_description_set_style (new_desc, pango_font_description_get_style (old_desc));
        pango_font_description_set_size (new_desc, pango_font_description_get_size (old_desc));
        sprintf (prefs.hex_text_font, "%s", pango_font_description_to_string (new_desc));

        /* FIXME this is not required after pango_font_description_from_string()
        g_free (old_desc);
        g_free (new_desc);
        */
#endif

        if (prefs.hex_irc_real_name[0] == 0)
        {
                fe_message (_("The Real name option cannot be left blank. Falling back to \"realname\"."), FE_MSG_WARN);
                strcpy (prefs.hex_irc_real_name, "realname");
        }

	theme_manager_dispatch_setup_apply (&event);

	mg_apply_live_prefs (&live_changes);

        if (noapply)
                fe_message (_("Some settings were changed that require a"
                                                " restart to take full effect."), FE_MSG_WARN);

#ifndef WIN32
        if (prefs.hex_dcc_auto_recv == 2) /* Auto */
        {
                if (!strcmp ((char *)g_get_home_dir (), prefs.hex_dcc_dir))
                {
                        fe_message (_("*WARNING*\n"
                                                         "Auto accepting DCC to your home directory\n"
                                                         "can be dangerous and is exploitable. Eg:\n"
                                                         "Someone could send you a .bash_profile"), FE_MSG_WARN);
                }
        }
#endif
}

static gboolean
setup_sounds_stage (GError **error)
{
        GString *text;
        int i;
        setup_sound_pending = FALSE;
        if (!setup_sound_created) return TRUE;
        text = g_string_new ("");
        for (i = 0; i < NUM_XP; i++)
        {
                if (setup_sound_files[i] && strpbrk (setup_sound_files[i], "\r\n"))
                {
                        g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Sound filenames must stay on one line."));
                        g_string_free (text, TRUE);
                        return FALSE;
                }
                if (g_strcmp0 (setup_sound_files[i], setup_sound_original[i]) != 0) setup_sound_pending = TRUE;
                if (setup_sound_files[i] && *setup_sound_files[i])
                        g_string_append_printf (text, "event=%s\nsound=%s\n\n", te[i].name, setup_sound_files[i]);
        }
        if (setup_sound_pending)
        {
                for (i = 0; i < NUM_XP; i++)
                        if (g_strcmp0 (sound_files[i], setup_sound_original[i]) != 0)
                        {
                                g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_AGAIN, _("Sounds changed outside Preferences. Reopen Preferences before editing them."));
                                g_string_free (text, TRUE);
                                return FALSE;
                        }
                preferences_persistence_stage_file ("sound.conf", text->str);
        }
        g_string_free (text, TRUE);
        return TRUE;
}

/* Cell editors can still be active when Save is activated from the keyboard.
 * Finish the GtkCellEditable before serializing any list model. */
static void
setup_finish_cell_edits (GtkWidget *widget)
{
        GList *children, *entry;
        if (!GTK_IS_CONTAINER (widget)) return;
        children = gtk_container_get_children (GTK_CONTAINER (widget));
        for (entry = children; entry; entry = entry->next)
        {
                GtkWidget *child = entry->data;
                if (GTK_IS_TREE_VIEW (widget) && GTK_IS_CELL_EDITABLE (child))
                {
                        g_object_ref (child);
                        gtk_cell_editable_editing_done (GTK_CELL_EDITABLE (child));
                        gtk_cell_editable_remove_widget (GTK_CELL_EDITABLE (child));
                        g_object_unref (child);
                }
                else setup_finish_cell_edits (child);
        }
        g_list_free (children);
}

static void
setup_show_error (const char *message)
{
        gtk_label_set_text (GTK_LABEL (setup_error_label), message);
        gtk_widget_show (setup_error_bar);
}

static void
setup_error_dismiss (GtkInfoBar *bar, int response, gpointer data)
{
        gtk_widget_hide (GTK_WIDGET (bar));
}

static void
setup_ok_cb (GtkWidget *but, GtkWidget *win)
{
        PreferencesPersistenceResult save_result;
        struct zoitechatprefs old_prefs;
        char buffer[192];
        GError *error = NULL;
        guint page_index;

        gtk_widget_hide (setup_error_bar);
        setup_finish_cell_edits (win);
        for (page_index = 0; page_index < setup_pages->len; page_index++)
        {
                setup_page *page = &g_array_index (setup_pages, setup_page, page_index);
                if (!strcmp (page->id, "typing") && page->body &&
                    !preferences_typing_page_stage (page->body, &error))
                {
                        setup_show_error (error->message);
                        g_clear_error (&error);
                        return;
                }
                if (!strcmp (page->id, "logging") && page->body &&
                    !preferences_history_page_stage (page->body, &error))
                {
                        setup_reveal_widget (page->body);
                        setup_show_error (error->message);
                        g_clear_error (&error);
                        return;
                }
                if (!strcmp (page->id, "appearance") && page->body &&
                    !theme_preferences_color_editor_stage (page->body, &error))
                {
                        setup_show_error (error->message);
                        g_clear_error (&error);
                        return;
                }
        }

        /* Move focus out of cell editors before serializing their models. */
        gtk_widget_grab_focus (but ? but : cancel_button);
        preferences_persistence_clear_staged_files ();
        if (!editlist_preferences_stage (&error) ||
            !key_preferences_stage (&error) || !pevent_preferences_stage (&error) || !setup_sounds_stage (&error))
        {
                setup_show_error (error ? error->message : _("Could not validate preferences."));
                g_clear_error (&error);
                return;
        }

        /* Network List owns identity and list state independently of this window. */
        g_strlcpy (setup_prefs.hex_irc_nick1, prefs.hex_irc_nick1, sizeof setup_prefs.hex_irc_nick1);
        g_strlcpy (setup_prefs.hex_irc_nick2, prefs.hex_irc_nick2, sizeof setup_prefs.hex_irc_nick2);
        g_strlcpy (setup_prefs.hex_irc_nick3, prefs.hex_irc_nick3, sizeof setup_prefs.hex_irc_nick3);
        g_strlcpy (setup_prefs.hex_irc_user_name, prefs.hex_irc_user_name, sizeof setup_prefs.hex_irc_user_name);
        g_strlcpy (setup_prefs.hex_irc_real_name, prefs.hex_irc_real_name, sizeof setup_prefs.hex_irc_real_name);
        setup_prefs.hex_gui_slist_select = prefs.hex_gui_slist_select;
        setup_prefs.hex_gui_slist_fav = prefs.hex_gui_slist_fav;
        setup_prefs.hex_gui_slist_skip = prefs.hex_gui_slist_skip;

        memcpy (&old_prefs, &prefs, sizeof (prefs));
        setup_apply (&setup_prefs);
        theme_preferences_stage_apply ();
        save_result = preferences_persistence_save_all ();
        if (save_result.success)
        {
                int i;
                editlist_preferences_commit ();
                key_preferences_commit ();
                pevent_preferences_commit ();
                if (setup_sound_pending)
                        for (i = 0; i < NUM_XP; i++)
                        {
                                g_free (sound_files[i]);
                                sound_files[i] = g_strdup (setup_sound_files[i]);
                        }
                theme_preferences_stage_commit ();
                /* the previewed values were accepted, so the close handler
                   must not revert them */
                setup_transparency_snapshot = prefs.hex_gui_transparency;
                gtk_widget_destroy (win);
                return;
        }

        setup_apply (&old_prefs);
        theme_preferences_stage_revert_runtime ();

        if (save_result.partial_failure)
        {
                setup_show_error (_("Some preference files could not be restored after a save failure. Recovery copies remain in the configuration folder. Retry is possible."));
                return;
        }

        g_snprintf (buffer, sizeof (buffer), _("Could not save preferences (%s). Retry is possible."), save_result.failed_file ? save_result.failed_file : _("unknown file"));
        setup_show_error (buffer);
}

static GtkWidget *
setup_window_open (void)
{
        GtkWidget *wid, *win, *vbox, *hbox, *hbbox;
        GdkDisplay *display;
        GdkMonitor *monitor;
        char buf[128];

        g_snprintf(buf, sizeof(buf), _("Preferences - %s"), _(DISPLAY_NAME));
        win = gtkutil_window_new (buf, "prefs", 940, 700, 2);
        setup_window = win;
        display = gtk_widget_get_display (win);
        monitor = parent_window && gtk_widget_get_window (parent_window)
                ? gdk_display_get_monitor_at_window (display, gtk_widget_get_window (parent_window))
                : gdk_display_get_primary_monitor (display);
        if (!monitor)
                monitor = gdk_display_get_monitor (display, 0);
        if (monitor)
        {
                GdkRectangle area;
                gdk_monitor_get_workarea (monitor, &area);
                gtk_window_set_default_size (GTK_WINDOW (win),
                        MIN (940, MAX (1, area.width - 48)),
                        MIN (700, MAX (1, area.height - 48)));
        }

        vbox = gtkutil_box_new (GTK_ORIENTATION_VERTICAL, FALSE, 5);
        gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);
        gtk_container_add (GTK_CONTAINER (win), vbox);

        hbox = gtkutil_box_new (GTK_ORIENTATION_HORIZONTAL, FALSE, 4);
        gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

        setup_create_pages (hbox);
        setup_empty = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
        gtk_widget_set_halign (setup_empty, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (setup_empty, GTK_ALIGN_CENTER);
        gtk_box_pack_start (GTK_BOX (setup_empty), gtk_label_new (_("No matching settings")), FALSE, FALSE, 0);
        wid = gtk_button_new_with_mnemonic (_("_Clear search"));
        g_signal_connect (wid, "clicked", G_CALLBACK (setup_search_clear), NULL);
        gtk_box_pack_start (GTK_BOX (setup_empty), wid, FALSE, FALSE, 0);
        gtk_box_pack_start (GTK_BOX (hbox), setup_empty, TRUE, TRUE, 0);
        setup_create_sidebar (hbox);
        g_signal_connect (win, "key-press-event", G_CALLBACK (setup_key_press), NULL);

        setup_error_bar = gtk_info_bar_new ();
        gtk_info_bar_set_message_type (GTK_INFO_BAR (setup_error_bar), GTK_MESSAGE_ERROR);
        gtk_info_bar_set_show_close_button (GTK_INFO_BAR (setup_error_bar), TRUE);
        setup_error_label = gtk_label_new (NULL);
        gtk_label_set_xalign (GTK_LABEL (setup_error_label), 0);
        gtk_label_set_line_wrap (GTK_LABEL (setup_error_label), TRUE);
        gtk_label_set_max_width_chars (GTK_LABEL (setup_error_label), 72);
        gtk_label_set_selectable (GTK_LABEL (setup_error_label), TRUE);
        gtk_widget_set_hexpand (setup_error_label, TRUE);
        gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (setup_error_bar))), setup_error_label);
        g_signal_connect (setup_error_bar, "response", G_CALLBACK (setup_error_dismiss), NULL);
        gtk_box_pack_start (GTK_BOX (vbox), setup_error_bar, FALSE, FALSE, 0);

        gtk_box_pack_start (GTK_BOX (vbox), gtk_separator_new (GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

        /* prepare the button box */
        hbbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
        gtk_button_box_set_layout (GTK_BUTTON_BOX (hbbox), GTK_BUTTONBOX_END);
        gtk_box_set_spacing (GTK_BOX (hbbox), 8);
        gtk_container_set_border_width (GTK_CONTAINER (hbbox), 12);
        gtk_box_pack_end (GTK_BOX (vbox), hbbox, FALSE, FALSE, 0);

        cancel_button = wid = gtkutil_button_new_from_stock ("gtk-cancel", _("_Cancel"));
        g_signal_connect (G_OBJECT (wid), "clicked",
                                                        G_CALLBACK (gtkutil_destroy), win);
        gtk_box_pack_start (GTK_BOX (hbbox), wid, FALSE, FALSE, 0);

        wid = gtkutil_button_new_from_stock ("gtk-ok", _("_Save"));
        gtk_style_context_add_class (gtk_widget_get_style_context (wid), "suggested-action");
        g_signal_connect (G_OBJECT (wid), "clicked",
                                                        G_CALLBACK (setup_ok_cb), win);
        gtk_box_pack_start (GTK_BOX (hbbox), wid, FALSE, FALSE, 0);

        gtk_widget_show_all (win);
        gtk_widget_hide (setup_empty);
        gtk_widget_hide (setup_error_bar);

        return win;
}

static void
setup_close_cb (GtkWidget *win, GtkWidget **swin)
{
        guint i;
        *swin = NULL;
        for (i = 0; setup_pages && i < setup_pages->len; i++)
                g_free (g_array_index (setup_pages, setup_page, i).search_text);
        if (setup_pages)
                g_array_free (setup_pages, TRUE);
        setup_pages = NULL;
        g_clear_pointer (&setup_query, g_free);
        setup_book = setup_sidebar = setup_search = setup_empty = NULL;
        setup_error_bar = setup_error_label = NULL;
        setup_topicbar_multiline_toggle = NULL;
        for (i = 0; i < NUM_XP; i++)
        {
                g_clear_pointer (&setup_sound_files[i], g_free);
                g_clear_pointer (&setup_sound_original[i], g_free);
        }
        setup_sound_created = setup_sound_pending = FALSE;
        sndfile_entry = NULL;
        setup_sound_tree = NULL;
        preferences_persistence_clear_staged_files ();

        /* drop any pending transparency preview and undo the previewed
           opacity when the dialog is cancelled; after OK the snapshot
           was updated so this is a no-op */
        if (setup_trans_apply_tag)
        {
                g_source_remove (setup_trans_apply_tag);
                setup_trans_apply_tag = 0;
        }
        if (prefs.hex_gui_transparency != setup_transparency_snapshot)
        {
                mg_live_prefs revert_changes;

                prefs.hex_gui_transparency = setup_transparency_snapshot;
                memset (&revert_changes, 0, sizeof (revert_changes));
                revert_changes.transparency = TRUE;
                mg_apply_live_prefs (&revert_changes);
        }

        theme_preferences_stage_discard ();

        if (font_dialog)
        {
                gtk_widget_destroy (font_dialog);
                font_dialog = NULL;
        }
}

void
setup_open (void)
{
        if (setup_window)
        {
                gtk_window_present (GTK_WINDOW (setup_window));
                return;
        }

        memcpy (&setup_prefs, &prefs, sizeof (prefs));

        color_change = FALSE;
        setup_transparency_snapshot = prefs.hex_gui_transparency;
        theme_preferences_stage_begin ();
        setup_window = setup_window_open ();

        g_signal_connect (G_OBJECT (setup_window), "destroy",
                                                        G_CALLBACK (setup_close_cb), &setup_window);
}

void
setup_open_page (const char *id)
{
        guint i;
        const char *page_id = id;
        gboolean menu_editor;
        if (!id) { setup_open (); return; }
        if (!strcmp (id, "themes") || !strcmp (id, "colors")) page_id = "appearance";
        if (!strcmp (id, "completion-shortcuts")) page_id = "keyboard";
        menu_editor = !strcmp (id, "buttons") || !strcmp (id, "popup") ||
                      !strcmp (id, "dlgbuttons") || !strcmp (id, "usermenu");
        if (menu_editor) page_id = "menus";
        setup_open ();
        gtk_entry_set_text (GTK_ENTRY (setup_search), "");
        for (i = 0; i < setup_pages->len; i++)
        {
                setup_page *page = &g_array_index (setup_pages, setup_page, i);
                if (strcmp (page->id, page_id)) continue;
                gtk_list_box_select_row (GTK_LIST_BOX (setup_sidebar), GTK_LIST_BOX_ROW (page->row));
                if (!strcmp (id, "completion-shortcuts"))
                        preferences_shortcuts_find_action (page->body, "Complete nick/command");
                if (menu_editor)
                {
                        GtkWidget *selector = g_object_get_data (G_OBJECT (page->body), "menus-selector");
                        gtk_combo_box_set_active_id (GTK_COMBO_BOX (selector), id);
                }
                break;
        }
        gtk_window_present (GTK_WINDOW (setup_window));
}
