ZoiteChat Lua Interface
=====================

Scripts
-------

Just like many other language plugins, the Lua plugin provides the
commands ``/load``, ``/unload``, and ``/reload`` which operate on Lua
scripts. You can forcibly load a Lua script with any extension by using
``/lua load``, ``/lua unload`` or ``/lua reload`` instead. All files
ending in ``.lua`` and ``.luac`` in the addons directory will be
automatically loaded on startup.

Every script gets its own isolated state. The state is initialized with
the Lua standard library, and has a global table called ``zoitechat``
providing the API, which is described below.

Commands
--------

.. program:: lua

.. option:: /lua load <filename>
.. option:: /load <filename>

Loads a script with the given filename. ``/load`` will only load files
ending in ``.lua`` and ``.luac``.

.. option:: /lua unload <filename>
.. option:: /unload <filename>

Unloads a script with the given filename.

.. option:: /reload <filename>`
.. option:: /lua reload <filename>`

Reloads a script with the given filename.

.. option:: /lua list

Lists loaded Lua scripts.

.. option:: /lua exec <code>

Executes given code in the interpreter.

.. option:: /lua console

Opens an interactive console. Messages to that tab are intercepted and
interpreted as code.

.. option:: /lua inject <filename> <code>

Executes given code in the context of a given script (which has to be
loaded).

.. option:: /lua reset

Reloads the interpreter (but not the scripts).

Environment
-----------

.. note::

    This only *directly* applies to **Windows** since other platforms rely on
    the Lua version and libraries installed on your system. It is most
    likely still of use, however.

The embedded Lua implementation is `LuaJIT`_ . LuaJIT by itself provides a
few `extensions`_, such as the the ``bit`` and ``ffi`` modules. It
generally follows Lua 5.1 and has a few backwards compatible additions from
Lua 5.2, which are explained on the `extensions`_ page.

.. _LuaJIT: http://luajit.org/luajit.html
.. _extensions: http://luajit.org/extensions.html

Hexchat then provides the global ``zoitechat`` table, which is described in
detail in the :ref:`lua-API` section below.

Additionally, the ``lgi`` module is available, which provides
GObject-introspection, since it is used by Hexchat internally. For looking
up mappings, the `Python GObject Introspection API reference <http://lazka.github.io/pgi-docs/index.html>`_
can be used, as it is very similar.

References
~~~~~~~~~~

LuaJIT
    http://luajit.org/luajit.html
LuaJIT Extensions
    http://luajit.org/extensions.html
BitOp Library
    http://bitop.luajit.org/api.html
FFI Library
    http://luajit.org/ext_ffi.html
LGI project page
    https://github.com/pavouk/lgi
LGI User Guide
    https://github.com/pavouk/lgi/blob/master/docs/guide.md
Gnome API Reference
    https://developer.gnome.org/references
Python GObject Introspection API Reference
    http://lazka.github.io/pgi-docs/index.html

.. _lua-API:

API
---

The ZoiteChat API is accessible through the ``zoitechat`` table.

General functions
~~~~~~~~~~~~~~~~~

zoitechat.register(name, version, description)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Upon initialization every script should introduce itself by calling
this function. Failure to do so will result in the script being
unloaded immediately.

zoitechat.command(cmd)
^^^^^^^^^^^^^^^^^^^^

Executes the command ``cmd`` in the current context, as if
``/cmd`` was typed by the user.

See ``/help`` for valid commands and their documentation.

zoitechat.print(...)
^^^^^^^^^^^^^^^^^^

Prints zero or more values to the current tab. This function also
replaces the global ``print`` function.

zoitechat.emit_print(event, ...)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Emits a text event (can be found in Settings->Text Events) into the
current tab. ``...`` are the strings that are passed as arguments to the
event. At the moment due to internal limitations, only 5 arguments are
passed as there aren't any text events with more arguments.

zoitechat.send_modes(targets, mode[, max])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Sets multiple modes in the current context, possibly grouping them
together up to the server limit. ``targets`` needs to be an array of
strings, ``mode`` needs to be a string of 2 characters: ``+`` or ``-``
followed by the mode letter. ``max`` is the number of modes on one line,
if omitted the serverside limit is used.

zoitechat.nickcmp(a, b)
^^^^^^^^^^^^^^^^^^^^^

Compares 2 strings case-insensitively, in accordance with current
server's casemapping. Returns a negative number if ``a`` is less than
``b``, zero if they are equal, and a positive number if ``a`` is more
than ``b``.

zoitechat.strip(string[, keep_colors[, keep_attrs]])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Removes color codes from the given string. If ``keep_colors`` is a
truthy value, colors are not removed. If ``keep_attrs`` is a truthy
value, attributes such as bold or underline are not removed. Returns the
resulting string.

zoitechat.get_info(id)
^^^^^^^^^^^^^^^^^^^^

Returns information about the current context. ``id`` is a string
determining the information you want. It can be one of the following
(case sensitive):

==================== ====================================================================================================
 ID                   Return value
==================== ====================================================================================================
 away                 Away reason or ``nil`` if you are not away
 channel              Current context's name
 charset              Character set used in the current context
 configdir            ZoiteChat config directory, e.g. ``/home/user/.config/zoitechat``
 event_text <name>    Text event format string for ``<name>``
 host                 Real hostname of the server you connected to
 inputbox             The input box contents, what the user has typed
 libdirfs             Library directory. e.g. ``/usr/lib/zoitechat``. The same directory is used for autoloading plugins
 modes                Channel modes, or ``nil`` if not known
 network              Current network name, or ``nil`` if not known
 nick                 Your current nickname
 password             Password for this network or ``nil``
 server               Current server name (what the server claims to be) or ``nil`` if you are not connected
 topic                Current channel topic
 version              ZoiteChat version number
 win_status           Window status: ``active``, ``hidden`` or ``normal``
 win_ptr              A light userdata pointer to the native window. GtkWindow on unix, HWND on windows
 gtkwin_ptr           A light userdata pointer to a GtkWindow. (Usable within lgi)
==================== ====================================================================================================

zoitechat.iterate(list)
^^^^^^^^^^^^^^^^^^^^^

Iterate through the list ``list``. To be used with generic for-loops in
the following fashion:

.. code-block:: lua

    for chan in zoitechat.iterate("channels") do
        print(chan.server .. ": " .. chan.channel)
    end

List of possible values of ``list``, along with respective keys:

+--------------+--------------------------------------------------------------------+
| ``channels`` | List of channels, queries and their servers                        |
+--------------+--------------------------------------------------------------------+
| Name         | Description                                                        |
+==============+====================================================================+
| channel      | Tab name                                                           |
+--------------+--------------------------------------------------------------------+
| channelkey   | Channel's key or ``nil``                                           |
+--------------+--------------------------------------------------------------------+
| chanmodes    | Channel modes, e.g. ``beI,k,l`` (2.12.2+)                          |
+--------------+--------------------------------------------------------------------+
| chantypes    | Channel types, e.g. ``#!&``                                        |
+--------------+--------------------------------------------------------------------+
| context      | Tab's context object                                               |
+--------------+--------------------------------------------------------------------+
| flags        | - 2^0 = Connected                                                  |
|              | - 2^1 = Connecting                                                 |
|              | - 2^2 = Marked away                                                |
|              | - 2^3 = End of MOTD                                                |
|              | - 2^4 = Has WHOX                                                   |
|              | - 2^5 = Has IDMSG                                                  |
|              | - 2^6 = Hide Join/Parts                                            |
|              | - 2^7 = Hide Join/Parts unset                                      |
|              | - 2^8 = Beep on Message                                            |
|              | - 2^9 = Beep on Message unset                                      |
|              | - 2^10 = <unused>                                                  |
|              | - 2^11 = Logging                                                   |
|              | - 2^12 = Logging unset                                             |
|              | - 2^13 = Scrollback                                                |
|              | - 2^14 = Scrollback unset                                          |
|              | - 2^15 = Strip colors                                              |
|              | - 2^16 = Strip colors unset                                        |
|              | - 2^17 = Flash tray                                                |
|              | - 2^18 = Flash tray unset                                          |
|              | - 2^19 = Flash taskbar                                             |
|              | - 2^20 = Flash taskbar unset                                       |
+--------------+--------------------------------------------------------------------+
| id           | Unique server ID                                                   |
+--------------+--------------------------------------------------------------------+
| lag          | Lag in milliseconds                                                |
+--------------+--------------------------------------------------------------------+
| maxmodes     | Maximum modes per line                                             |
+--------------+--------------------------------------------------------------------+
| network      | Network name                                                       |
+--------------+--------------------------------------------------------------------+
| nickprefixes | Nickname prefixes, e.g. ``@+``                                     |
+--------------+--------------------------------------------------------------------+
| nickmodes    | Nickname mode chars, e.g. ``ov``                                   |
+--------------+--------------------------------------------------------------------+
| queue        | Number of bytes in the send-queue                                  |
+--------------+--------------------------------------------------------------------+
| server       | Server name to which this channel belongs                          |
+--------------+--------------------------------------------------------------------+
| type         | - 1 = Server                                                       |
|              | - 2 = Channel                                                      |
|              | - 3 = Dialog                                                       |
|              | - 4 = Notice                                                       |
|              | - 5 = SNotice                                                      |
+--------------+--------------------------------------------------------------------+
| users        | Number of users in this channel                                    |
+--------------+--------------------------------------------------------------------+


+------------+----------------------------------------------+
| ``users``  | Current contexts users                       |
+------------+----------------------------------------------+
| Name       | Description                                  |
+============+==============================================+
| account    | Account name or ``nil``                      |
+------------+----------------------------------------------+
| away       | Away message or ``nil``                      |
+------------+----------------------------------------------+
| host       | Host name in form of ``user@host`` or ``nil``|
+------------+----------------------------------------------+
| lasttalk   | Time stamp of last time they spoke           |
+------------+----------------------------------------------+
| nick       | Nick name                                    |
+------------+----------------------------------------------+
| prefix     | Prefix character such as ``@``               |
+------------+----------------------------------------------+
| realname   | Real name                                    |
+------------+----------------------------------------------+
| selected   | If they are selected in the userlist         |
+------------+----------------------------------------------+

+------------+----------------------------------------------------------------------+
| ``dcc``    | List of DCC file transfers                                           |
+------------+----------------------------------------------------------------------+
| Name       | Description                                                          |
+============+======================================================================+
| address32  | Address of the remote user (ipv4 address)                            |
+------------+----------------------------------------------------------------------+
| cps        | Bytes per second (speed)                                             |
+------------+----------------------------------------------------------------------+
| destfile   | Destination full pathname                                            |
+------------+----------------------------------------------------------------------+
| file       | File name                                                            |
+------------+----------------------------------------------------------------------+
| nick       | Nickname of person who the file is from/to                           |
+------------+----------------------------------------------------------------------+
| port       | TCP port number                                                      |
+------------+----------------------------------------------------------------------+
| pos        | Bytes sent/received                                                  |
+------------+----------------------------------------------------------------------+
| poshigh    | Bytes sent/received, high order 32 bits                              |
+------------+----------------------------------------------------------------------+
| resume     | Point at which this file was resumed (or zero if it was not resumed) |
+------------+----------------------------------------------------------------------+
| resumehigh | Point at which this file was resumed, high order 32 bits             |
+------------+----------------------------------------------------------------------+
| size       | File size in bytes, low order 32 bits (cast it to unsigned)          |
+------------+----------------------------------------------------------------------+
| sizehigh   | File size in bytes, high order 32 bits                               |
+------------+----------------------------------------------------------------------+
| status     | - 0 = Queued                                                         |
|            | - 1 = Active                                                         |
|            | - 2 = Failed                                                         |
|            | - 3 = Done                                                           |
|            | - 4 = Connecting                                                     |
|            | - 5 = Aborted                                                        |
+------------+----------------------------------------------------------------------+
| type       | - 0 = Send                                                           |
|            | - 1 = Recieve                                                        |
|            | - 1 = ChatRecv                                                       |
|            | - 1 = ChatSend                                                       |
+------------+----------------------------------------------------------------------+


+------------+----------------------------------------------+
| ``ignore`` | Current ignore list                          |
+------------+----------------------------------------------+
| Name       | Description                                  |
+============+==============================================+
| mask       | Ignore mask, .e.g. ``\*\!\*\@\*.aol.com``    |
+------------+----------------------------------------------+
| flags      | - 2^0 = Private                              |
|            | - 2^1 = Notice                               |
|            | - 2^2 = Channel                              |
|            | - 2^3 = CTCP                                 |
|            | - 2^4 = Invite                               |
|            | - 2^5 = Unignore                             |
|            | - 2^6 = NoSave                               |
|            | - 2^7 = DCC                                  |
+------------+----------------------------------------------+


+------------+----------------------------------------------------------------------+
| ``notify`` | List of people on notify                                             |
+------------+----------------------------------------------------------------------+
| Name       | Description                                                          |
+============+======================================================================+
| networks   | Networks to which this nick applies. Comma separated. May be ``nil`` |
+------------+----------------------------------------------------------------------+
| nick       | Nickname                                                             |
+------------+----------------------------------------------------------------------+
| flags      | Bit field of flags. 0=Is online.                                     |
+------------+----------------------------------------------------------------------+
| on         | Unix timestamp of when user came online.                             |
+------------+----------------------------------------------------------------------+
| off        | Unix timestamp of when user went offline.                            |
+------------+----------------------------------------------------------------------+
| seen       | Unix timestamp of when user the user was last verified still online. |
+------------+----------------------------------------------------------------------+

zoitechat.props
^^^^^^^^^^^^^

A table containing the values of a ``"channels"`` list for the current
context.

Preferences
~~~~~~~~~~~

zoitechat.prefs
^^^^^^^^^^^^^

You can access ZoiteChat's settings via this pseudo-table, see ``/set``
for a list of keys. Note that you cannot modify the table. Instead,
you should use ``zoitechat.command("set -quiet <key> <value>")``

There are also these special preferences:

- ``id`` - Unique server id
- ``state_cursor`` - Location of cursor in input box

Hooks
~~~~~

Some hooks are executed in a priority order, and hooks executed earlier
can prevent later hooks from being invoked. The following constants
determine priorities of such hooks and are passed to the hooking
function:

-  ``zoitechat.PRI_HIGHEST`` - The highest priority.
-  ``zoitechat.PRI_HIGH``
-  ``zoitechat.PRI_NORM`` - The default priority.
-  ``zoitechat.PRI_LOW``
-  ``zoitechat.PRI_LOWEST`` - The lowest priority.

The following constants determine whether to pass the event on after the
hook has finished. One of these has to be returned from the callback:

-  ``zoitechat.EAT_NONE`` - Let other hooks see the event.
-  ``zoitechat.EAT_ZOITECHAT`` - Let other hooks see the event, but prevent
   ZoiteChat itself from seeing it.
-  ``zoitechat.EAT_PLUGIN`` - Don't let remaining hooks see the event, but
   let ZoiteChat know about it.
-  ``zoitechat.EAT_ALL`` - Consume this event completely, don't let anyone
   else know about it.

All hooking functions return an object which can be later used to remove
the hook, but the hooks are also removed automatically when the script
is unloaded or reloaded.

Unlike the C and Python APIs, there isn't a userdata value passed to the
hooks. Instead you should use upvalues, closures, and/or anonymous
functions.

zoitechat.hook_command(command, callback[, help[, priority]])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Hooks the function ``callback`` to be executed whenever ``/command`` is
entered. ``help`` is the helptext for the ``/help`` command. Returns a
hook object. The callback receives an array of words, and an array of
word\_eols as arguments.

If ``command`` is nil, then all non-command text is hooked instead,
including ``/say``.

zoitechat.hook_print(event, callback[, priority])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Hooks the function ``callback`` to be executed whenever the text event
``event`` is to be printed. Returns a hook object. The callback receives
the array of event's arguments as its only argument.

There are also a few extra events you can hook using this function:

* ``Open Context`` - Emitted when a new context is created.
* ``Close Context`` - Emitted when a context is closed.
* ``Focus Tab`` - Emitted when a tab is brought to the front.
* ``Focus Window`` - Emitted when a toplevel window is focused, or the main tab-window is focused by the window manager.
* ``DCC Chat Text`` - Emitted when some text from a DCC Chat arrives. It provides these elements in the word list:
    * Address
    * Port
    * Nick
    * Message
* ``Key Press`` - Emitted when some keys are pressed in the input box. It provides these elements in the word list:
    * Key Value
    * Modifier bitfield (Shift, CapsLock, Alt, etc)
    * String version of the key
    * Length of the string (may be 0 for unprintable keys)

zoitechat.hook_server(command, callback[, priority])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Hooks the function ``callback`` to be executed whenever ``command`` is
received from the server. Returns a hook object. The callback receives
an array of words, and an array of word\_eols as arguments.

If ``command`` is nil, then the callback is called for every received
line.

zoitechat.hook_timer(interval, callback)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Hooks the function ``callback`` to be executed after ``inverval``
milliseconds. Returns a hook object. As long as the callback returns a
truthy value, it is scheduled to happen after the same preiod of time.

zoitechat.hook_unload(callback)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Hooks the function ``callback`` to be executed when the current script
is unloaded. Returns a hook object.

hook:unhook() and zoitechat.unhook(hook)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Removes the given hook. A hook can only be removed once.

Contexts
~~~~~~~~

A context corresponds to a ZoiteChat window or tab. Some of the functions
in ``zoitechat.*`` will do something in the current tab. Using contexts
you can perform such actions in other tabs instead. Two context objects
can be tested for equality using the ``==`` operator, which will return
true if the contexts refer to the same tab. All methods of a context
object will error if the object is invalidated and points to a tab that
is closed.

zoitechat.get_context()
^^^^^^^^^^^^^^^^^^^^^

Returns a context object for the current context.

zoitechat.find_context(server_name, channel_name)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Finds a context object for a tab on the given channel of the given
channel. If ``server_name`` is nil, it searches for the given channel or
query across all servers. If ``channel_name`` is nil, finds the
frontmost tab of the given server. If both are ``nil``, returns current
context. In any case, if the specified tab was not found, the function
returns nil.

ctx:set() and zoitechat.set_context(ctx)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Makes ``ctx`` the "current" context. All ``zoitechat.*`` functions will be using
this context. This setting only persists within one event. Next time any
of the callbacks is called, the current context will be set to the
actual one.

Returns a boolean indicating whether the context was successfully set. Failure
will only occur if the context has been closed.

ctx:find_context(server_name, channel_name)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Identical to ``zoitechat.find_context``, except the defaults are based on
the current context.

ctx:print(...)
^^^^^^^^^^^^^^

Prints zero or more values in the given context.

ctx:emit_print(event, ...)
^^^^^^^^^^^^^^^^^^^^^^^^^^

Emits a text event into the given context. See ``zoitechat.emit_print``.

Returns a boolean indicating success.

ctx:command(cmd)
^^^^^^^^^^^^^^^^

Executes the command ``/cmd`` in the given context. See
``zoitechat.command``.

ctx:nickcmp(a, b)
^^^^^^^^^^^^^^^^^

Compares 2 strings using casemapping from the given context. See
``zoitechat.nickcmp``.

ctx:get_info(id)
^^^^^^^^^^^^^^^^

Returns information about the given context. See ``zoitechat.get_info``.

ctx:iterate(list)
^^^^^^^^^^^^^^^^^

Iterate through a list within the given context. See
``zoitechat.iterate``.

Plugin preferences
~~~~~~~~~~~~~~~~~~

zoitechat.pluginprefs
^^^^^^^^^^^^^^^^^^^

To persistently store your script's settings, you can use this pseudo-table.
The values inside will persist across script reloads, ZoiteChat restarts, and
reboots. Currently, you can only store and read strings and numbers associated
to string keys, and iterate through the table with ``pairs()``.

Attributes
~~~~~~~~~~

Attributes correspond to extra metadata for messages, such as
server-time (currently the only supported attribute). Some functions
have attributes-enhanced versions.

zoitechat.attrs()
^^^^^^^^^^^^^^^

Returns a new attributes object. It has only one field:
``server_time_utc``.

zoitechat.emit_print_attrs(attrs, event, ...) and ctx:emit_print_attrs(attrs, event, ...)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Analogous to ``zoitechat.emit_print`` and ``ctx:emit_print`` respectively,
but passes an extra attributes argument.

zoitechat.hook_print_attrs(event, callback[, priority])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Identical to ``zoitechat.hook_print``, except that the callback receives
an additional second argument with an attributes object and that the
aforementioned extra events cannot be hooked.

zoitechat.hook_server_attrs(command, callback[, priority])
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Identical to ``zoitechat.hook_server``, except that the callback receives
an additional third argument with an attributes object.
