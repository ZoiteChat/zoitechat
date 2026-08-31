Tips & Tricks
=============

Spell Check
-----------

Windows
~~~~~~~

.. image:: _static/img/tips_spellcheck.png

ZoiteChat comes with spellcheck already installed. To use additional dictionaries, add them to Windows:

1. In the Start menu, go to :menuselection:`Settings -> Time & Language -> Region & Language`.
2. Add new language(s).
3. Restart ZoiteChat.


Unix
~~~~

ZoiteChat comes with spellcheck already installed and enabled by default on most platforms.

.. note:: If it's missing, install enchant and your spelling dictionaries via your package manager (e.g. hunspell-en-us for English). Then make sure to enable spelling under :menuselection:`Settings --> Preferences --> Interface --> Input box`.

You can list your languages in :menuselection:`Settings --> Preferences --> Interface --> Input box` by their language codes (i.e. de_DE for german), separated by commas.

Localization
------------

In order to start ZoiteChat in a different language (for which a translation exists `here <https://www.transifex.com/projects/p/zoitechat/>`_) you can use the regional settings of Windows, or set the LC_ALL user environmental variable. The value of the variable must be the two letter country code for your country. If in doubt, have a look at the share\locale folder. You have to restart ZoiteChat for the changes to apply.

You can also use a batch file to affect only ZoiteChat:

.. code-block:: bat

    @echo off
    set LC_ALL=en
    start zoitechat.exe

This sets the language to English. You may use *fr* for French, *de* for German, etc. Save the code above as :file:`run.bat`, and copy it to the ZoiteChat install folder. You can then start ZoiteChat in the desired language by running the batch file.

Special Glyphs
--------------

There are many symbols which may not be supported by the main font you selected to use in ZoiteChat, especially Asian glyphs and special characters, like a peace sign. In this case, you'll see "lego blocks" instead of them.

To circumvent this, you need to have alternative fonts for glyphs not supported by your current font. On Unix this is handled automatically. On Windows you can specify them in :menuselection:`Settings --> Preferences --> Chatting --> Advanced --> Alternative fonts`. By default, it is set to *Arial Unicode MS,Segoe UI Emoji,Lucida Sans Unicode,Meiryo,Symbola,Unifont*, which should cover most characters (note that Unifont does not come with Windows).

There are many available fonts that try to cover most of unicode:

- `Unifont <http://unifoundry.com/unifont.html>`_
- `Symbola <http://users.teilar.gr/~g1951d/>`_
- `Quivira <http://www.quivira-font.com/>`_

In case you still get lego blocks, you'll need to add additional fonts to the list which support those obscure glyphs. Feel free to extend the list. You only need to specify font names, other info (such as size, weight, style etc.) should be omitted, otherwise those entries will be ignored. All font names must be separated by a comma and there mustn't be spaces before and/or after commas.

Please bear in mind that for some reason certain fonts that can display a certain glyph when used as the main font may not work when specified as an alternative font so you might have to play around it a bit.

Client Certificates
-------------------

Client certificates identify you to IRC network services. This is separate from accepting invalid server certificates.

Use the network list dialog to configure client certification for each network:

#. Open :menuselection:`ZoiteChat --> Network List`.
#. Select a network and click :command:`Edit`.
#. In the client SSL cert area, use the buttons shown for your current state:

   - Use :command:`Generate client SSL cert` to create a new certificate for that network, or :command:`Import client SSL cert` to copy an existing one.
   - If a certificate already exists, use :command:`Client SSL cert info` to inspect it or :command:`Delete cert` to remove it.

On networks that support it you can use SASL EXTERNAL in the network list. If a network does not support this but does support normal SASL usually that would be the better option.

4. Save the network settings and reconnect.

Note on Custom Server Certificates
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Windows it is possible to edit *cert.pem* file in ZoiteChat main installation directory and add custom certificate there. But this method isn't very effective as *cert.pem* is overwritten each time ZoiteChat installer is used.

Notice Placement
----------------

Other than channel messages and private messages, IRC has a notice type of message. This is intended to be used as a reply, something that will not cause the other client to send any acknowledgement back. When ZoiteChat displays these messages, it shows them in a tab that it figures is appropriate.

Why replies from ChanServ may not appear in the current tab
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When ZoiteChat decides where to print a notice, it does so in the following order:

1. In a query window you have with that user
2. In the front tab, if the tab is a channel, the other user is on that channel, and you are on the correct network
3. In the last joined channel you have in common with the other user
4. The current tab, if you are on the same network
5. The last tab you looked at that shares the correct network with the other user

This means that if you issue a :command:`/cs info #yourchannel` from your channel, the reply may show up elsewhere if ChanServ isn't in your channel, but is in some other channel.

How to make notices show up in a consistent location
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest method is to set the location in :menuselection:`Settings --> Preferences --> Channel switcher --> Placement of notices`, and select "in an extra tab" or "in the front tab". The former will cause all server notices to go into a (snotices) tab, and all user notices to go into a (notices) tab. The latter will always print the notices where you are, this can cause odd positioning of channel notices but you will never miss them.

If you know who will notice you before hand, you can simply query the user before they notice you. This way, all notices from that user will show up in the query tab. In the case of ChanServ, this may allow an easier archive of commands you have done anyway.

For other locations, a separate script would be required. While not currently implemented, it would be possible with a script to treat all notices like private messages (open a new query window when received), or place them in a specific existing tab, such as the server tab. At this point, the choice is up to you (or whoever designs the script).

How the marker line works
-------------------------

The marker line is a very useful tool to keep track of what you have and have not read in a channel but it's behavior is non-obvious at times. It just follows a few simple rules though.

A line is created when new information is printed in a context that is not currently visible. This means the window is in the background, another tab is selected, or you are scrolled up.

This line by design only automatically resets when it is seen. One common issue here is that the marker line is at the very top of your scrollback so you very unlikely to see it. This can happen with bnc playback for example where you get a lot of messages at once.

ZoiteChat has two shortcuts to reset the marker line also. Ctrl+M will reset the the marker line directly. Ctrl+Shift+M will scroll to where the marker was which is quite useful if you actually care about the scrollback.

Once a marker line is "reset" it does not instantly get created at the bottom it will only be created if it matches the conditions mentioned above (not being visible).

Tor
---

1. Find if the network you wish to connect to allows connections from the Onion Network (check their official website and the Message of The Day).
2. Get Tor working. Refer to the tutorials from the `official Tor support center <https://support.torproject.org/>`_.
3. Set up proxy in :menuselection:`Settings --> Preferences --> Network Setup`.

Example (with defaults):

.. image:: _static/img/tips_tor_1.png

4. Setup the network in :menuselection:`ZoiteChat --> Network List`. (Note: use only the updated information from the official website of the IRC network you wish to connect to)

Example:

.. image:: _static/img/tips_tor_2.png

Twitch
------

Twitch.tv uses irc for chat so you can use a regular client for chat but it is a very customized irc that has some extra requirements.

In the Network List add a new network and for the server use *irc.chat.twitch.tv* with SSL. You must have your nickname match your twitch account. For the login method choose *Server Password* and generate a password on this website `<http://twitchapps.com/tmi>`_

To enhance your experience I recommend using the `twitch.lua <https://github.com/TingPing/plugins/blob/master/HexChat/twitch.lua>`_ script.
