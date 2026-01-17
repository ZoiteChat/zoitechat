ZoiteChat ChangeLog
=================

2.17.3 (2026-01-17)
-------------------

SNI Extension Handling
- Added RFC 6066-compliant validation to ensure SNI host names are ASCII-only and not IP literals before setting SNI on SSL connections. 
- Emit an SSL message when an invalid SNI host name is skipped to aid troubleshooting.
-  Integrated theme functionality in preferences.  Removed Theme Manager application. 
-  Added colour Emoji support / selector menu. 
- Changed Python version to v3.14.2
- Fixed newline issue causing issues when syncing Windows/Linux server list files. 

Issues with current build
- When opening a theme file (.zct or .hct file).  ZoiteChat will open a new instance and then import the theme. So importing works but leaves a window you need to close afterwards. 
- Emojis only show colour in all builds except Windows.   Having issues with GTk2 in windows showing colour emojis. 

2.17.2 (2026-01-13)
-------------------

- Improved Auto Replace functionality.  

2.17.1 (2026-01-13)
-------------------

- Feature/dark mode toggle by @deepend-tildeclub in https://github.com/ZoiteChat/zoitechat/pull/3

2.17.0 (2026-01-07)
-------------------

- Currently Flatpak and Windows only. Debian in progress.
- Flatpak does not include the Python plugin due to issues.  (Windows version unaffected by this issue)

2.16.2 (2024-02-07)
-------------------

- change ``/server`` to use TLS by default, use ``-insecure`` or the ``-`` prefix on the port to disable
- change preferences dialog to be modal
- add support for the ``extended-monitor`` capability
- add support for ``SCRAM`` SASL mechanisms
- add option to hide nick from window title
- increase max server password length to 1024
- fix some detecting some URLs causing a crash
- fix long SASL auth strings not being split
- fix ``PROTOCOL_NAMESX`` support and only use it when not using ``multi-prefix``
- fix dialog topic not being updated with ``CHGHOST``
- fix notify.conf being reversed when saved
- lua: fix crash when using pluginprefs
- python: fix API break in ``hook_timer()``
- python: fix timers that unhook themselves
- checksum: Various improvements including not blocking the UI
- win32: Add installer options to control start menu, desktop, and quick launch shortcuts
- win32: Update python3 to 3.8
- win32: Remove python2

2.16.1 (2022-02-12)
-------------------

- add ``-NOOVERRIDE`` flag to the ``GUI COLOR`` command
- add ``-q`` (quiet) flag to ``EXECWRITE`` command
- rename installed icon on Linux to match app-id (Fixes notification icon)
- fix escaping already escaped URLs when opening them
- fix Python scripts not being opened as UTF-8
- fix ``TIMER`` command supporting decimals regardless of locale
- fishlim: fix building with OpenSSL 3

2.16.0 (2021-10-01)
-------------------

- add support for IRCv3 SETNAME, invite-notify, account-tag, standard replies, and UTF8ONLY
- add support for strikethrough formatting
- update network list (including Libera.Chat as the default)
- update OpenSSL on Windows
- fix text clipping issues by respecting font line height
- fix URLs not being escaped when opened
- fix misc IRC message parsing issues
- remove libnotify dependency on Linux, fixing hangs when notifications are shown
- remove libproxy dependency on Linux
- print ChanServ notices in the front tab by default
- fishlim: add support for CBC mode
- python: rewrite plugin in python with CFFI
  This lowers memory usage and fixed conflicts with many C Python modules including pygobject

2.14.3 (2019-12-20)
-------------------

- fix various incorrect parsing of IRC messages relating to trailing parameters
- fix SASL negotiation combined with multi-line cap
- fix input box theming with Yaru theme
- python: Work around Python 3.7 regression causing crash on unload
- sysinfo: Add support for /etc/os-release
- sysinfo: Ignore irrelevant mounts when calculating storage size


2.14.2 (2018-08-29)
-------------------

- remove shift+click binding to close tabs
- re-add option to build against legacy perl
- add appstream metainfo for plugins
- add build option to set perl binary
- add option to build without appstream
- fix not unminimizing when restoring from tray
- fix translations containing invalid text events
- fix server passwords starting with ``:``
- update libraries on windows, fixing CVE-2018-15120 (and emoji!)

2.14.1 (2018-03-13)
-------------------

- fix performance regression on Unix
- fix building plugins with some compilers
- add missing gtk pixbuf theme engine in Windows installer

2.14.0 (2018-03-10)
-------------------

- rewrite build system in `Meson <http://mesonbuild.com/>`_
- replace ``intltool`` build depenency with ``gettext`` >= 0.19.6
- rename data files to use *io.github.Hexchat* name
- add option (``irc_reconnect_rejoin``) to disable auto-rejoin on reconnect
- add ability to set custom tray icon separate of app icon
- fix building against OpenSSL 1.1.0
- fix Enchant 2.0+ support
- fix input box theming with Adwaita-dark
- fix custom sounds not respecting omit if away option
- fix detecting if a tray doesn't exist on x11
- fix cutting off ctcp text after ending ``\01``
- fix /ignore not accepting full hosts
- fix characters getting cut off when their width changes (on Unix)
- fix various possible crashes
- change preference window to be scroll-able
- remove ctrl+w binding by default
- remove mpcinfo plugin
- doat: fix channels with ``/`` in them
- fishlim: fix key exchange
- fishlim: fix building against LibreSSL
- sysinfo: fix pci.ids file not being found on some distros
- sysinfo: make libpci optional
- lua: avoid loading the same script multiple times
- update translations
- update libraries on Windows, including Python to 3.6

2.12.4 (2016-12-10)
-------------------

- fix issue with timers causing ping timeouts
- fix building against OpenSSL 1.1
- fix /exec output printing invalid utf8
- replace doat plugin with an internal command
- change how tab colors interact with plugins


2.12.3 (2016-10-22)
-------------------

- fix crash with bad translations
- fix crash and leaks in mpcinfo plugin
- add mhop command
- change ping timeout to 60 by default
- update translations


2.12.2 (2016-10-08)
-------------------

- fix input box theme with Adwaita 3.20
- fix return value of hexchat_pluginpref_get_int()
- fix tab color changing when print events are eaten
- fix network name not being sanitized for scrollback files
- fix building sysinfo on OS X <= 10.9
- fix resume with DCC GET
- fix possible assertion when decoding incoming text
- fix possible crashes when plugins modify the UI during context close
- add "chanmodes" to channel list in plugin api
- lua:

  - add automatic return and = handling in console
  - fix pluginpref usage

- fishlim:

  - fix saving nicks containing [ or ]
  - add commands: /topic+, /msg+, and /notice+
  - add support for /me
  - add /keyx command to do DH1080 key exchanges

- improve efficiency of various timers
- reduce updates of user count in titlebar/userlist
- download extra redist for perl on Windows
- update appdata file
- update translations
- update dependencies on Windows


2.12.1-2 (2016-05-06)
---------------------

- fix crash in dependencies on Windows (no HexChat changes)

2.12.1 (2016-05-01)
-------------------

- add lua plugin
- change desktop file to open urls in existing instance on Unix
- misc chanopt fixes

  - fix not being saved properly
  - fix values other than 0/1/2 being set
  - fix flags in plugin API

- misc identd fixes

  - fix ipv6 support on Windows
  - fix respecting USE_GLOBAL network flag
  - fix connections not always closed
  - add responses for INVALID-PORT and NO-USER

- misc challengeauth fixes

  - fix lower casing username
  - don't respond to challenge notices when disabled

- re-add support for old versions of libnotify
- bundle luajit and lgi on Windows
- update network list

2.12.0 (2016-03-12)
-------------------

- add support for native Windows 8+ spell check
- add support for native Windows 8.1+ and OSX notifications
- add support for IRCv3.2

  - add cap 3.2
  - add sasl 3.2
  - add chghost
  - add cap-notify

- add support for twitch.tv/membership cap
- add support for SNI (Server Name Indication)
- add ability to do DnD reordering in some settings dialogs
- add option to disable middle-click closing tabs
- rewrite sysinfo plugin

  - add support for OSX
  - add multi-gpu info on win32
  - add hdd info on win32
  - add ability to print single peice of info
  - change formatting, unified across platforms
  - fix handling unsupported CPU arch on unix

- rewrite identd plugin

  - add support for unix
  - add ability to change port
  - fix handling multiple connections
  - fix threading issues

- rewrite update plugin

  - fix locking up UI during check
  - show graphical dialog on update
  - host update file on https://dl.hexchat.net

- rewrite checksum plugin

  - fix support for very large files
  - remove openssl dependency

- remove DH-{AES,BLOWFISH} mechanisms (insecure)
- remove "IRC" encoding, replaced with UTF-8
- remove "System Default" encoding, replaced with UTF-8
- remove configure option to disable ipv6
- remove msproxy and socks5 library support (unused)
- change tab-complete to favor other user nicks over own
- change url detection to support unicode
- change decoding to not attempt ISO-8859-1 fixing corruption
- change pluginpref to escape values
- change installer on Windows to not overwrite gtkrc file
- minor changes to icons
- fix numerous crashes (but not #600)

  - fix crash on printing very long lines
  - fix crash on failing to open log file
  - fix crash when using unsorted userlist
  - fix crash when timestamp format set to nothing
  - fix crash when tab-completeing long strings
  - fix crash with long values in pluginpref API
  - fix various other unsafe string handling throughout

- fix poor performance with nick indent enabled
- fix UTF-8 text in winamp plugin
- fix fishlim plugin handling networks with server-time
- fix logging hostname of users in new queries
- fix Key Press event sending non-UTF-8 text to plugins
- fix VERSION response on windows 10 
- fix get_info('win_ptr') from python
- fix running portable-mode from another directory
- fix duplicate timestamps on selection
- fix --cfgdir argument
- fix mode-button text being cut off
- fix scrollback timestamps with server-time
- fix url handler accepting quoted paths with spaces
- fix using correct encoding when jumping networks
- improve DCC handling large files
- improve python detection in configure
- improve scrollback file handling (corruption, line endings)
- improve building in cygwin
- improve build options on unix to be more secure
- update to VS 2015 on windows
- update deps on windows (openssl, python 3.5, etc)
- update translations
- update network list

2.10.2 (2014-11-25)
-------------------

- verify hostnames of certificates
- use more secure openssl options (No SSLv2/3)
- detect utf8 urls in chat
- fix using multiple client certs at the same time
- fix checking for Perl on some distros
- fix friends list not properly updating
- fix building with format-security
- fix more non-ascii path issues on Windows
- fix opening utf8 urls on Windows and OSX
- update deps on Windows
- update translations

2.10.1 (2014-07-28)
-------------------

- add configure check for python 3.4
- improve spell check handling apostrophes
- disable unsupported tray feature on Elementary OS
- disable installing on Vista (would just crash on x64)
- fix possible crash in banlist
- fix some crashes in pluginpref API
- fix crash in spell check
- fix notify list being sent to wrong networks
- fix scrolling the chanview on Unix
- fix building plugin support on some platforms (OS X 10.8)
- fix FiSHLiM decryption when identify-msg is enabled
- fix expanding *~user* on Unix
- fix missing license files on Windows
- fix silent installer on Windows
- update translations
- update network list
- update Python versions on Windows

2.10.0 (2014-06-01)
-------------------

- fix SASL on InspIRCd networks
- fix building on OpenBSD
- fix crash when using invalid timestamps on Windows
- fix Lag Meter reporting invalid numbers
- fix drag and drop on User List/Channel Switcher
- fix various Unicode issues on Windows
- add fullscreen display mode
- add ``/getbool`` command
- add support for userhost-in-names capability
- add --command command line flag on Windows
- add message parameter to ``/query``
- add help messages to user commands
- add ``plugin_pref`` to Perl
- add regex search to the Channel List on Windows
- add option to hide nick change messages
- redesign various settings windows (notably key bindings)
- render colors and attributes in the Input Box and Topic Bar
- bind middle-click to close tabs
- build the GTK+ PixBuf theme engine on Windows
- change ``/list`` to open the Channel List window
- change the format key bindings are stored in (will convert automatically)
- enable italics
- rewrite ``/dns`` adding support for Windows and replacing the plugin
- rebrand the Perl plugin
- replace the Search window with a Search Bar
- replace Text Box transparency with full window transparency (may come back)
- remove all deprecated GTK+ usage
- remove all direct use of X11, fixing XWayland support
- hide unsupported channel modes from the Topic Bar
- improve spell check (handle contractions)
- improve rendering and selection in the Text Box
- improve OS X integration (custom theme, app menu, dock icon, spell check, app bundle, key bindings)
- improve the Windows installer (downloads requirements when ran)
- update Visual Studio to 2013
- update Perl to 5.20 on Windows
- update Python to 3.4 on Windows
- update OpenSSL on Windows to fix "Heartbleed"
- update translations

2.9.6.1 (2013-09-15)
--------------------

- fix some utilities causing crash `#740 <https://github.com/hexchat/hexchat/issues/740>`_

2.9.6 (2013-09-11)
------------------

- redesign edit window in network list
- rename favorites to autojoin
- improve URL detection yet again (this time with more IPv6!)
- implement ``/exec -o`` on Windows
- improvements to the DCC window
- improvements to sysinfo on Unix, including ``-e`` to print info instead of saying
- add support for BLOWFISH, AES, and EXTERNAL SASL mechanisms
- add reload command and button in plugingui
- add support for server-time and znc.in/server-time[-iso] capabilities
- add attributes to ``hook_print``/``server`` and ``emit_print`` for information such as server-time
- add support for QuakeNet's challangeauth
- add chanopt for stripping colors
- add copy option to banlist entries
- add autoconnect option to context menu of networks
- add option for omitting alerts while window is focused
- add python3 support along with various bugfixes
- add libcanberra support on Unix
- add tracking of users accounts
- add ``%u`` to userlist popups for accounts
- add channelkey to channel lists in plugin api
- add ``MONITOR`` support for the friends list
- add ``QUIET`` and ``UNQUIET`` commands
- add support for the away-notify, account-notify, and extended-join capabilities
- add notifications for friends away status (requires away-notify)
- add events for quiet, unquiet, and quietlist
- add Ctrl+N (New Server Window) keybinding
- add Ctrl+Home/End keybinding for scrolling to top/bottom
- add theme manager to Unix build system
- fix compilation on FreeBSD
- fix running as root
- fix splitting ctcps and notices
- fix alerts and scrollback chanopts
- fix crash when attaching/detaching tabs
- fix sending limited channel messages (op messages) to the wrong tab
- change ``/load -e`` to load from config dir
- remove Ctrl+L (Clear Text) keybinding
- remove custom sound applications
- remove away announce, replaced by away-notify on supported servers (`alternative python script <https://github.com/hexchat/hexchat-addons/blob/master/python/awayannounce/awayanounce.py>`_)
- update network list

2.9.5 (2013-04-01)
------------------

- fix Checksum plugin with DCC download directory set
- fix false positives with Update Checker
- fix sound directory option on Unix
- fix loading custom icons
- fix tray icon not reappearing if the tray crashes
- fix restoring maximized windows from tray
- fix ``/QUERY -nofocus``
- fix reconnecting to channels with keys
- fix compilation on FreeBSD
- fix showing the join dialog when autojoining channels
- fix Plugin-Tray menu not closing on Windows
- fix close dialog minimizing to tray before selection
- fix Python plugin compilation on Ubuntu 13.04
- fix Theme Manager crashing with read-only files
- fix channel tree indentation without server tab or with icons
- add auto-away support to Plugin-Tray
- add Plugin-Tray option to disable blinking
- add option to always show notices on current tab
- add support for notification filtering in GNOME 3.8
- add support for channel keys in URLs
- add option to color nicks in the user list the same way as in the chat area
- add ability to automatically switch to last activity on change-page hotkey
- add ability to save divider position between combined user list and channel tree
- add global real name option to Preferences
- add Safe Mode shortcut to the Start Menu group on Windows
- add helpful links to the setup wizard on Windows
- make the source tree compliant with Debian policies
- install SVG icon on Unix
- enable Plugin-Tray menu on Windows
- enable IPv6 by default on Unix
- show ``/WHOIS`` response on current tab by default
- redesign the Ban List window to show invites, bans, exemptions and quiets
- make user list icons slightly smaller
- close all utility windows with the Esc key
- improve URL and username detection in the chat area
- make ``/JOIN`` focus the existing channel if already joined
- change default DCC download directory to ~/Downloads on Unix
- allow Plugins and Scripts utility to be opened in a tab
- only beep when the HexChat window is not active
- use the certs subfolder of the config folder for loading custom certificates
- disable tray icon when using Unity
- remove Lua and Tcl
- remove HexTray in favor of built-in Plugin-Tray
- remove installer theming on Windows
- cease support for Perl 5.12 and 5.14 on Windows
- rebuild every dependency with Visual C++ on Windows
- stop using the WDK on Windows and depend on the Visual C++ Redistributable
- update GTK+ to 2.24 on Windows
- update default text events
- update translations
- update the network list

2.9.4 (2012-11-11)
------------------

- fix alerts when omit alerts in away option is set
- fix dialog icon in userlist popup
- fix opening links on Mac
- fix default network in the Network List
- fix initial folder in file dialogs
- fix positioning the nick change dialog
- fix error message for busy servers
- fix filename encoding errors
- fix Fedora spec file
- fix Raw Log content being impossible to copy when auto-copy is disabled
- fix rough icon rendering in most windows on Windows
- fix config folder when specified with -d argument
- add built-in support for SASL authentication via CAP
- add support for identify-msg/multi-prefix server capabilities
- add text events for CAP related messages
- add support for the SysInfo plugin on Unix
- add option to change update check frequency and delay for first check
- add option to change GUI language on Windows
- add Ignore entry to userlist popup
- add Afrikaans, Asturian, Danish, Gujarati, Indonesian, Kinyarwanda and Malayalam translations
- add ChangeLog and ReadMe links to Start Menu during installation on Windows
- add manual page on Unix
- add icon support for 3 levels above op user mode
- change default colors, text events and user list/channel tree icons
- make Esc key close the Raw Log window
- use Consolas as the default font where available
- open dialog window for double-clicking in the user list by default
- variable separation, cleanup and renaming
- check in the installers whether Windows release is supported by HexChat
- display previous value after ``/SET``
- reorganize the Settings menu and add new options
- redesign the About dialog
- show certain help messages in GTK+ dialogs instead of command line
- disable faulty one instance option
- build system cosmetics on Unix
- reorganize repo file structure
- rebranding
- update translations
- update the network list

2.9.3 (2012-10-14)
------------------

- fix various URL detection bugs
- fix default folders for file transfers in portable mode
- fix Autotools warnings with recent releases
- add ``/ADDSERVER`` command
- add option to save URLs to disk on-the-fly
- add option to omit alerts when marked as being away
- add default icons for channel tree and option to turn them off
- change certain default colors
- enhance Non-BMP filtering performance
- accept license agreement by default on Windows
- update the network list

2.9.2 (2012-10-05)
------------------

- fix compilation on Red Hat and Fedora
- fix portable to non-portable migrations on Windows
- fix ban message in HexTray
- fix icon in Connection Complete dialog
- fix determining if the log folder path is full or relative
- fix desktop notification icons on Unix
- fix URL grabber saving an unlimited number of URLs by default
- fix URL grabber memory leaks under certain circumstances
- fix URL grabber trying to export URL lists to system folders by default
- fix opening URLs without ``http(s)://``
- add support for regenerating text events during compilation on Windows
- add support for the theme manager on Unix
- add Unifont to the default list of alternative fonts
- add option to retain colors in the topic
- allow the installer to preserve custom GTK+ theme settings on Windows
- use the icons subfolder of the config folder for loading custom icons
- use port 6697 for SSL connections by default
- install the SASL plugin by default on Windows
- ``/lastlog`` improvements
- build system cosmetics on Unix
- open links with just left click by default
- enable timestamps and include seconds by default
- make libproxy an optional dependency on Unix
- update German translation
- update the network list

2.9.1 (2012-07-27)
------------------

- fix installing/loading plugins on Unix
- fix restoring the HexChat window via shortcuts on Windows
- fix HexTray icon rendering for certain events
- fix the Show marker line option in Preferences
- fix ``/lastlog`` regexp support on Windows
- add support for the Checksum, Do At, FiSHLiM and SASL plugins on Unix
- add option to retain colors when displaying scrollback
- add MS Gothic to the default list of alternative fonts
- rebranding and cleanup
- eliminate lots of compiler warnings
- Unix build system fixes and cosmetics
- make Git ignore Unix-specific intermediate files
- use better compression for Windows installers
- switch to GTK+ file dialogs on Windows
- restructure the Preferences window
- use the addons subfolder of the config folder for auto-loading plugins/scripts
- improve the dialog used for opening plugins/scripts
- remember user limits in channel list between sessions
- remember last search pattern during sessions
- update XChat to r1521

2.9.0 (2012-07-14)
------------------

- rebranding
- migrate code to GitHub
- update XChat to r1515
- fix x64 Perl interface installation for Perl 5.16
- improve URL detection with new TLDs and file extensions

1508-3 (2012-06-17)
~~~~~~~~~~~~~~~~~~~

- add XChat Theme Manager
- fix problems with Turkish locale

1508-2 (2012-06-15)
~~~~~~~~~~~~~~~~~~~

- add support for Perl 5.16
- update Do At plugin
- fix drawing of chat area bottom
- avoid false hits when restoring from tray via shortcut
- migrate from NMAKE to Visual Studio

1508 (2012-06-02)
~~~~~~~~~~~~~~~~~

- remove Real Name from Network List
- search window improvements
- restore XChat-WDK from tray via shortcut if X-Tray is used

1507 (2012-05-13)
~~~~~~~~~~~~~~~~~

- update OpenSSL to 1.0.1c
- FiSHLiM updates

1506 (2012-05-04)
~~~~~~~~~~~~~~~~~

- update OpenSSL to 1.0.1b
- update German translation

1503 (2012-03-16)
~~~~~~~~~~~~~~~~~

- update OpenSSL to 1.0.1
- URL grabber updates
- FiSHLiM updates

1500 (2012-02-16)
~~~~~~~~~~~~~~~~~

- add option for specifying alternative fonts
- fix crash due to invalid timestamp format
- X-Tray cosmetics

1499-7 (2012-02-08)
~~~~~~~~~~~~~~~~~~~

- fix update notifications
- fix compilation on Linux
- add IPv6 support to built-in identd

1499-6 (2012-01-20)
~~~~~~~~~~~~~~~~~~~

- add DNS plugin

1499-5 (2012-01-20)
~~~~~~~~~~~~~~~~~~~

- built-in fix for client crashes
- update OpenSSL to 1.0.0g

1499-4 (2012-01-18)
~~~~~~~~~~~~~~~~~~~

- add Non-BMP plugin to avoid client crashes

1499-3 (2012-01-15)
~~~~~~~~~~~~~~~~~~~

- rework and extend plugin config API
- add ``ADD``/``DEL``/``LIST`` support to X-SASL

1499-2 (2012-01-11)
~~~~~~~~~~~~~~~~~~~

- add X-SASL plugin

1499 (2012-01-09)
~~~~~~~~~~~~~~~~~

- fix saving FiSHLiM keys
- update OpenSSL to 1.0.0f

1498-4 (2011-12-05)
~~~~~~~~~~~~~~~~~~~

- fix updates not overwriting old files
- display WinSys output in one line for others
- use Strawberry Perl for building

1498-3 (2011-12-02)
~~~~~~~~~~~~~~~~~~~

- add plugin config API
- add Exec plugin
- add WinSys plugin
- perform periodic update checks automatically

1498-2 (2011-11-25)
~~~~~~~~~~~~~~~~~~~

- add FiSHLiM plugin
- add option to allow only one instance of XChat to run

1498 (2011-11-23)
~~~~~~~~~~~~~~~~~

- separate x86 and x64 installers (uninstall any previous version!)
- downgrade GTK+ to 2.16
- re-enable the transparent background option
- various X-Tray improvements
- add WMPA plugin
- add Do At plugin
- automatically save set variables to disk by default
- update OpenSSL to 1.0.0e

1496-6 (2011-08-09)
~~~~~~~~~~~~~~~~~~~

- add option to auto-open new tab upon ``/MSG``
- fix the update checker to use the git repo
- disable update checker cache

1496-5 (2011-08-07)
~~~~~~~~~~~~~~~~~~~

- fix attach/detach keyboard shortcut
- add multi-language support to the spell checker

1496-4 (2011-07-27)
~~~~~~~~~~~~~~~~~~~

- recognize Windows 8 when displaying OS info
- update OpenSSL certificate list
- fix X-Tray blinking on unselected events
- fix X-Tray keyboard shortcut handling
- cease support for Perl 5.10
- use Strawberry Perl for 5.12 DLLs

1496-3 (2011-06-16)
~~~~~~~~~~~~~~~~~~~

- add option for changing spell checker color

1496-2 (2011-06-05)
~~~~~~~~~~~~~~~~~~~

- add support for custom license text

1496 (2011-05-30)
~~~~~~~~~~~~~~~~~

- display build type in CTPC VERSION reply
- add support for Perl 5.14

1494 (2011-04-16)
~~~~~~~~~~~~~~~~~

- update Visual Studio to 2010 SP1
- update OpenSSL to 1.0.0d
- ship MySpell dictionaries in a separate installer

1489 (2011-01-26)
~~~~~~~~~~~~~~~~~

- fix unloading the Winamp plugin
- enable the Favorite Networks feature
- add Channel Message event support to X-Tray
- add mpcInfo plugin

1486 (2011-01-16)
~~~~~~~~~~~~~~~~~

- fix a possible memory leak in the update checker
- fix XChat-Text shortcut creation
- fix XChat version check via the plugin interface
- add option for limiting the size of files to be checksummed
- add X-Tray as an install option
- disable Plugin-Tray context menu completely

1479-2 (2011-01-10)
~~~~~~~~~~~~~~~~~~~

- improve command-line argument support
- add auto-copy options
- enable XChat-Text
- disable faulty tray menu items

1479 (2010-12-29)
~~~~~~~~~~~~~~~~~

- update GTK+ to 2.22.1
- update OpenSSL to 1.0.0c
- update Python to 2.7.1
- replace X-Tray with Plugin-Tray

1469-3 (2010-10-20)
~~~~~~~~~~~~~~~~~~~

- add Checksum plugin
- menu integration for Update Checker and Winamp

1469-2 (2010-10-09)
~~~~~~~~~~~~~~~~~~~

- fix DCC file sending
- native open/save dialogs
- make the version info nicer
- register XChat-WDK as IRC protocol handler
- add option to run XChat-WDK after installation
- disable erroneous uninstall warnings
- disable Plugin-Tray, provide X-Tray only
- cease support for Perl 5.8
- replace EasyWinampControl with Winamp

1469 (2010-10-08)
~~~~~~~~~~~~~~~~~

- use Visual C++ 2010 for all WDK builds
- build Enchant with WDK and update it to 1.6.0
- fix SSL validation
- fix opening the config folder from GUI in portable mode
- further improve dialog placement for closing network tabs

1468-2 (2010-10-02)
~~~~~~~~~~~~~~~~~~~

- update GTK+ to 2.22
- spelling support
- more config compatibility with official build
- improve dialog placement for closing network tabs
- remove themes from the installer
- disable toggle for favorite networks until it's usable
- disable transparent backgrounds
- hide mnemonic underlines until Alt key pressed
- fix XP lagometer and throttlemeter rendering

1468 (2010-09-19)
~~~~~~~~~~~~~~~~~

- update Perl to 5.12.2
- update Tcl to 8.5.9
- fix scrollback shrinking
- enable advanced settings pane
- retain emoticon settings
- add ``/IGNALL`` command

1464-6 (2010-09-06)
~~~~~~~~~~~~~~~~~~~

- fix Perl interface breakage
- update checker plugin

1464-5 (2010-08-30)
~~~~~~~~~~~~~~~~~~~

- primitive update checker

1464-4 (2010-08-30)
~~~~~~~~~~~~~~~~~~~

- selectable tray icon
- selectable theme for portable
- selectable plugins

1464-3 (2010-08-29)
~~~~~~~~~~~~~~~~~~~

- black theme for portable

1464-2 (2010-08-29)
~~~~~~~~~~~~~~~~~~~

- make Perl version selectable during install

1464 (2010-08-26)
~~~~~~~~~~~~~~~~~

- Perl interface updates

1462 (2010-08-25)
~~~~~~~~~~~~~~~~~

- update XChat to r1462
- build system cleanup

1459-3 (2010-08-23)
~~~~~~~~~~~~~~~~~~~

- more installer changes (uninstall any previous version!)

1459-2 (2010-08-23)
~~~~~~~~~~~~~~~~~~~

- universal installer
- update build dependencies

1459 (2010-08-19)
~~~~~~~~~~~~~~~~~

- portable mode and installer fixes

1457 (2010-08-17)
~~~~~~~~~~~~~~~~~

- disable GUI warnings

1455-2 (2010-08-17)
~~~~~~~~~~~~~~~~~~~

- unified installer for standard and portable

1455 (2010-08-15)
~~~~~~~~~~~~~~~~~

- support for gtkwin\_ptr in the Perl interface

1454 (2010-08-14)
~~~~~~~~~~~~~~~~~

- gtkwin\_ptr for plugins introduced

1452 (2010-08-14)
~~~~~~~~~~~~~~~~~

- fix taskbar alerts on x86
- upgrade Perl to 5.12 and make 5.8/5.10 builds available separately

1451-6 (2010-08-12)
~~~~~~~~~~~~~~~~~~~

- include Lua-WDK with the installer

1451-5 (2010-08-12)
~~~~~~~~~~~~~~~~~~~

- switch to Inno Setup (uninstall any previous version!)
- add Lua support

1451-4 (2010-08-11)
~~~~~~~~~~~~~~~~~~~

- enable the XDCC plugin

1451-3 (2010-08-11)
~~~~~~~~~~~~~~~~~~~

- enable Python support

1451-2 (2010-08-11)
~~~~~~~~~~~~~~~~~~~

- enable SSL support
- fix simultaneous connections
- re-enable identd by default

1451 (2010-08-10)
~~~~~~~~~~~~~~~~~

- update XChat to r1451
- disable identd by default
- remove DNS plugin

1444 (2010-07-30)
~~~~~~~~~~~~~~~~~

- update XChat to r1444
- downgrade Tcl to 8.5
- add Tcl support to the x64 build

1441 (2010-06-15)
~~~~~~~~~~~~~~~~~

- update XChat to r1441
- enable transfer of files bigger than 4 GB

1439 (2010-05-30)
~~~~~~~~~~~~~~~~~

- update XChat to r1439 (2.8.8)

1431-6 (2010-05-30)
~~~~~~~~~~~~~~~~~~~

- re-enable the transparent background option
- add branding to Plugin-Tray
- installer updates

1431-5 (2010-05-29)
~~~~~~~~~~~~~~~~~~~

- fix installer
- add DNS plugin status messages

1431-4 (2010-05-28)
~~~~~~~~~~~~~~~~~~~

- disable the transparent background option
- downgrade GTK+ to more stable 2.16

1431-3 (2010-05-23)
~~~~~~~~~~~~~~~~~~~

- add portable build support

1431-2 (2010-05-22)
~~~~~~~~~~~~~~~~~~~

- replace X-Tray with Plugin-Tray

1431 (2010-05-21)
~~~~~~~~~~~~~~~~~

- update XChat to r1431
- include a lot of XChat translations added since 2.8.6

1412-3 (2010-05-02)
~~~~~~~~~~~~~~~~~~~

- fix GTK function call

1412-2 (2010-05-02)
~~~~~~~~~~~~~~~~~~~

- re-enable taskbar alerts on x64

1412 (2010-05-02)
~~~~~~~~~~~~~~~~~

- update XChat to r1412
- update GTK+ and friends
- update Visual Studio to 2010
- fix Perl warning message
- include GTK L10n with the installer

1409-9 (2010-04-18)
~~~~~~~~~~~~~~~~~~~

- fix loading of scrollback

1409-8 (2010-04-03)
~~~~~~~~~~~~~~~~~~~

- fix X-Tray on x64

1409-7 (2010-04-02)
~~~~~~~~~~~~~~~~~~~

- disable taskbar notification options

1409-6 (2010-03-31)
~~~~~~~~~~~~~~~~~~~

- display version numbers everywhere

1409-5 (2010-03-31)
~~~~~~~~~~~~~~~~~~~

- add DNS plugin
- add EasyWinampControl plugin
- disable Plugin-Tray settings

1409-4 (2010-03-30)
~~~~~~~~~~~~~~~~~~~

- add X-Tray

1409-3 (2010-03-29)
~~~~~~~~~~~~~~~~~~~

- plugin linkage fixes

1409-2 (2010-03-29)
~~~~~~~~~~~~~~~~~~~

- enable IPv6 support
- enable NLS support
- enable Perl support
- enable Tcl support

1409 (2010-03-29)
~~~~~~~~~~~~~~~~~

- initial release
