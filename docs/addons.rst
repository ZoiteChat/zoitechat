Addons
======

ZoiteChat ships with a handful of useful plugins that extend the functionality of the client. In order to auto-load custom ones you just place them in an :file:`addons` subdir within your `config folder <settings.html#config-files>`_, manually with :command:`/load` and :command:`/unload`, or with :menuselection:`Window --> Plugins and Scripts`.

With the included scripting interfaces you can also use easier to create but equally powerful scripts in languages such as Python. These are loaded/unloaded in the same manner as plugins.

A directory of existing ZoiteChat addons can be found at `https://zoitechat.org/addons.php <https://zoitechat.org/addons.php>`_. In addition, since ZoiteChat is based on HexChat, most HexChat and many X-Chat 2 scripts and addons are compatible too, as long as they are written in a `supported scripting language <developers.html>`_.


Exec
----

The exec plugin provides the :command:`/exec` command for Windows. On Unix platforms there are built-in commands for this and this documentation does not apply to the behavior of those, see :command:`/help` for more information.

With Exec you can perform commands as if you ran them in the command line. The output will be printed at once, in the end of execution. If the command takes more than 10 seconds to complete, it gets aborted to avoid locking down ZoiteChat. Usage:

:command:`/exec ping google.com`


FISHLiM
-------

Adds FiSH encryption support.  You set a password for a conversation/channel, and then all your messages get encrypted. Only those who know the password will be able to read your messages. Usage is simple: first you set the password:

:command:`/setkey yoursecretkey`

Then you let your fellow chatters know this password. Once they also set the password on their sides, they'll receive messages decrypted and send them encrypted, too.

You can remove the key with:

:command:`/delkey #channel`

Update Checker
--------------

Windows only. Automatically checks for available updates. Can also be manually checked via :menuselection:`Help --> Check for Updates` or by the command:

:command:`/updchk`


Sysinfo
-------

Prints out basic system information on both Windows and Unix. You can either activate in :menuselection:`Window --> Display System Info` or type:

:command:`/sysinfo`


Checksum
--------

Automatically calculates the SHA-256 checksum of files sent and recieved via DCC.


Winamp
------

Displays your currently playing song via :menuselection:`Window --> Display Current Song` or by command:

:command:`/winamp`

.. note::
    - Windows-only and untested, but may work
    - Foobar2000 can also be used with the `foo_winamp_spam <https://github.com/RazielZ/foo_winamp_spam>`_ plugin.
