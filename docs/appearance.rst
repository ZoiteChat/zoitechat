Appearance
==========

Theming
-------

Gtk Theme
~~~~~~~~~

Starting with version 2.18.0, ZoiteChat uses GTK 3. Theming is handled differently than it was in 2.17.x, HexChat, or X-Chat 2.

On Unix, the default is your system GTK theme. You can change the theme ZoiteChat uses in :menuselection:`Settings --> Preferences --> Appearance --> GTK3 theme`. Themes can be imported here too, which will only be available for ZoiteChat.
Themes installed via your system package manager or desktop environment control panel will also be available for ZoiteChat, as well as the rest of your GTK3 applications.

On Windows, the default theme is `Redstone <https://www.opendesktop.org/p/1013482/>`_ which is bundled with the ZoiteChat installer. In :menuselection:`Settings --> Preferences --> Appearance --> GTK3 theme`, you can import other themes as well, just like on Unix.

On both platforms, the theme selection drop down will show (user) for themes installed through ZoiteChat and (system) for system themes.

GTK 3 themes can be found on `openDesktop <https://www.opendesktop.org/browse?cat=135>`_ and other places like github. Any theme should work as long as it supports GTK 3.20 or higher.

Colors
~~~~~~

Colors are defined in :menuselection:`Settings --> Preferences --> Appearance --> Manage all client colors`. Text Colors set the palette for events to use. The rest like background color directly affect parts of the UI.

`mIRC <http://www.mirc.com/colors.html>`_ colors (00-15) are what you refer to when sending colored text over IRC for others to see and vice versa, because of this they should somewhat follow a set of standards so clients can agree 04 is red.

Local colors (16-31) can be changed to anything you wish, for instance to use in `text events <appearance.html#text-events>`_. However, ZoiteChat also supports the later `99 colors <https://modern.ircdocs.horse/formatting.html#colors-16-98>`_ standard so if you change the 16-31 colors it may conflict with that.

HexChat and X-Chat 2 supported color scheme "themes" as :file:`.hct` files, which are ZIP archives. Some of them are available from `HexChat Themes <https://hexchat.github.io/themes.html>`_, and you may have managed them with the discontinued Theme Manager.

If you want to use a HexChat/X-Chat theme, open the :file:`.hct` file in an application like 7-Zip or Ark and extract :file:`colors.conf`. Then go to :menuselection:`Settings --> Preferences --> Appearance --> Import colors.conf colors` to import it.
If the archive includes :file:`pevents.conf`, that can go in your `config folder <settings.html#config-files>`_ but may not work the same way it did in HexChat.

Text Events
~~~~~~~~~~~

Text events control the look of every event you see. They can be customized in :menuselection:`Settings --> Text Events` using these codes to format it:

- **%C<fg>,<bg>** Color code (e.g. %C02,00 is blue on grey)
- **%R** Reverse color
- **%U** Underlined text
- **%B** Bold text
- **%I** Italic text (2.10.0+)
- **%H** Hide text
- **%O** Normal text
- **%%** Escaped %
- **$t** Text separator (tab character)
- **$aXXX** Ascii value
- **$<num>** Event information

.. note::

    Always hit enter after editing a field.

Icons
~~~~~

ZoiteChat comes with built in icons for the tray, user list, and channel tree (which can be disabled in Preferences). You can use custom icons by placing png icons (16x16 recommended) in an :file:`icons` subdir, which may need to be created, within your `config folder <settings.html#config-files>`_. The icons must be named exactly as follows including file extensions:

- User List

  - ulist_netop.png
  - ulist_founder.png
  - ulist_owner.png
  - ulist_op.png
  - ulist_halfop.png
  - ulist_voice.png

- Channel Tree

  - tree_channel.png
  - tree_dialog.png
  - tree_server.png
  - tree_util.png

- Tray Icon

  - tray_normal.png
  - tray_fileoffer.png
  - tray_highlight.png
  - tray_message.png
  - zoitechat.png



Buttons, Menus, and Popups
--------------------------

Userlist Popup
~~~~~~~~~~~~~~

Popups are shown when you right click on a nickname, either in the userlist or in the main chat itself. These can be edited in :menuselection:`Settings --> Userlist Popup`

The Name column can take either just the name of the entry, *SUB*/*ENDSUB* for submenus, *SEP* for separators, and *TOGGLE* for toggleable options.
Prefix a character with *_* for keyboard shortcuts (e.g. N_ame will bind a).

The Command column can take any `command <commands.html>`_ with text formatted using the same codes as `text events <appearance.html#text-events>`_ and on top of that they also have their own codes:

- **%a** all selected nicks
- **%c** current channel
- **%h** selected nick's hostname
- **%m** machine info
- **%n** your nickname
- **%s** selected nickname
- **%t** time/date
- **%u** selected nick's account

The **gui_ulist_doubleclick** setting can run a command using these codes when double-clicking a nick in the userlist.

Userlist Buttons
~~~~~~~~~~~~~~~~

Buttons are shown below the userlist, can be edited in :menuselection:`Settings --> Userlist Buttons`, and take the same syntax as Userlist Popups.

Usermenu
~~~~~~~~

In order to add custom entries to your menu you need to first enable the usermenu with the command :command:`/set gui_usermenu on` which may require a restart. Once this is enabled you can go to :menuselection:`Usermenu --> Edit this Menu` to add any `command <commands.html>`_  you would like. For menu entries it supports the same syntax as Userlist Popups.
