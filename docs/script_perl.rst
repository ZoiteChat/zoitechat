ZoiteChat Perl Interface
======================

Introduction
------------

This is the Perl interface for ZoiteChat.

Constants
---------

Priorities
~~~~~~~~~~

- **ZoiteChat::PRI_HIGHEST**
- **ZoiteChat::PRI_HIGH**
- **ZoiteChat::PRI_NORM**
- **ZoiteChat::PRI_LOW**
- **ZoiteChat::PRI_LOWEST**

Return values
~~~~~~~~~~~~~

- **ZoiteChat::EAT_NONE** - pass the event along
- **ZoiteChat::EAT_ZOITECHAT** - don't let ZoiteChat see this event
- **ZoiteChat::EAT_PLUGIN** - don't let other scripts and plugins see
  this event but ZoiteChat will still see it
- **ZoiteChat::EAT_ALL** - don't let anything else see this event

Timer and fd hooks
^^^^^^^^^^^^^^^^^^

- **ZoiteChat::KEEP** - keep the timer going or hook watching the
  handle
- **ZoiteChat::REMOVE** - remove the timer or hook watching the handle

hook\_fd flags
~~~~~~~~~~~~~~

- **ZoiteChat::FD_READ** - invoke the callback when the handle is ready
  for reading
- **ZoiteChat::FD_WRITE** - invoke the callback when the handle is
  ready for writing
- **ZoiteChat::FD_EXCEPTION** - invoke the callback if an exception
  occurs
- **ZoiteChat::FD_NOTSOCKET** - indicate that the handle being hooked
  is not a socket

Exports
~~~~~~~

The following tags are supported:

- ``:all`` - exports everything
- ``:constants`` - exports all the constants
- ``:hooks`` - exports ``hook_*`` functions as well us ``unhook``
- ``:util`` - everything else

By default only the constants are exported.

Functions
---------

ZoiteChat::register( $name, $version, [$description,[$callback]] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``$name`` - The name of this script
- ``$version`` - This script's version
- ``$description`` - A description for this script
- ``$callback`` - This is a function that will be called when the is
  script unloaded. This can be either a reference to a function or an
  anonymous sub reference.

This is the first thing to call in every script.


ZoiteChat::hook_server( $message, $callback, [\%options] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ZoiteChat::hook_command( $command, $callback, [\%options] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ZoiteChat::hook_print( $event,$callback, [\%options] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ZoiteChat::hook_timer( $timeout,$callback, [\%options | $data] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
ZoiteChat::hook_fd( $handle, $callback, [ \%options ] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These functions can be to intercept various events. ``hook_server`` can be
used to intercept any incoming message from the IRC server.
``hook_command`` can be used to intercept any command, if the command
doesn't currently exist then a new one is created. ``hook_print`` can be
used to intercept any of the events listed in *Setttings* ``->`` *Text
Events*. ``hook_timer`` can be used to create a new timer

- **$message** - server message to hook such as PRIVMSG
- **$command** - command to intercept, without the leading /
- **$event** - one of the events listed in *Settings* ``->`` *Text
  Events*
- **$timeout** - timeout in milliseconds
- **$handle** - the I/O handle you want to monitor with hook\_fd.
  This must be something that has a fileno. See perldoc -f fileno or
  `fileno <http://perldoc.perl.org/functions/fileno.html>`_
- **$callback** - callback function, this is called whenever the
  hooked event is trigged, the following are the conditions that will
  trigger the different hooks. This can be either a reference to a
  function or an anonymous sub reference.
- **\%options** - a hash reference containing addional options for
  the hooks

Valid keys for %options:

+------------+--------------------------------------------------------------------------+
| data       | Additional data that is to be associated with the hook. For timer hooks  |
|            | this value can be provided either as ``ZoiteChat::hook_timer( $timeout,    |
|            | cb,{data=> data})`` or ``ZoiteChat::hook_timer( $timeout, $cb, $data )``.  |
|            | However, this means that hook\_timer cannot be provided with a hash      |
|            | reference containing data as a key.                                      |
|            |                                                                          |
|            | Example:                                                                 |
|            |                                                                          |
|            | .. code-block:: none                                                     |
|            |                                                                          |
|            |    my $options = { data => [@arrayOfStuff] };                            |
|            |    ZoiteChat::hook_timer( $timeout, $cb, $options );                       |
|            |                                                                          |
|            | In this example, the timer's data will be ``[@arrayOfStuff]`` and not    |
|            | ``{ data => [@arrayOfStuff] }``. This key is valid for all of the hook   |
|            | functions. Default is ``undef``.                                         |
+------------+--------------------------------------------------------------------------+
| priority   | Sets the priority for the hook. It can be set to one of the              |
|            | ``ZoiteChat::PRI_*`` constants. This key only applies to server, command   |
|            | and print hooks. Default is ``ZoiteChat::PRI_NORM``.                       |
+------------+--------------------------------------------------------------------------+
| help\_text | Text displayed for /help $command. This key only applies to command      |
|            | hooks. Default is "".                                                    |
+------------+--------------------------------------------------------------------------+
| flags      | Specify the flags for a fd hook. See hook fd flags section for valid     |
|            | values. On Windows if the handle is a pipe you specify                   |
|            | ``ZoiteChat::FD_NOTSOCKET`` in addition to any other flags you might be    |
|            | using. This key only applies to fd hooks. Default is                     |
|            | ``ZoiteChat::FD_READ``.                                                    |
+------------+--------------------------------------------------------------------------+


When callbacks are invoked
^^^^^^^^^^^^^^^^^^^^^^^^^^

Each of the hooks will be triggered at different times depending on the
type of hook.

+---------------+------------------------------------------------------------------------------------+
| Hook Type     | When the callback will be invoked                                                  |
+===============+====================================================================================+
| server hooks  | A ``$message`` message is received from the server.                                |
+---------------+------------------------------------------------------------------------------------+
| command hooks | The ``$command`` command is executed, either by the user or from a script.         |
+---------------+------------------------------------------------------------------------------------+
| print hooks   | X-Chat is about to print the message for the ``$event`` event.                     |
+---------------+------------------------------------------------------------------------------------+
| timer hooks   | Called every ``$timeout`` milliseconds (1000 milliseconds are 1 second).           |
|               | The callback will be executed in the same context where the ``hook_timer`` was     |
|               | called, or if the context no longer exists then it will execute in a random        |
|               | context.                                                                           |
+---------------+------------------------------------------------------------------------------------+
| fd hooks      | Depends on the flags that were passed to ``hook_fd``. See ``hook_fd`` flags in the |
|               | section above.                                                                     |
+---------------+------------------------------------------------------------------------------------+


The value return from these hook functions can be passed to
``ZoiteChat::unhook`` to remove the hook.

Callback Arguments
^^^^^^^^^^^^^^^^^^

All callback functions will receive their arguments in ``@_`` like every
other Perl subroutine.

- Server and command callbacks

  ``$_[0]`` - array reference containing the IRC message or command and
  arguments broken into words example:
  /command arg1 arg2 arg3

  - ``$_[0][0]`` - command
  - ``$_[0][1]`` - arg1
  - ``$_[0][2]`` - arg2
  - ``$_[0][3]`` - arg3

  ``$_[1]`` - array reference containing the Nth word to the last word
  example:
  /command arg1 arg2 arg3

  - ``$_[1][0]`` - command arg1 arg2 arg3
  - ``$_[1][1]`` - arg1 arg2 arg3
  - ``$_[1][2]`` - arg2 arg3
  - ``$_[1][3]`` - arg3

  ``$_[2]`` - the data that was passed to the hook function

- Print callbacks

  ``$_[0]`` - array reference containing the values for the text event,
  see *Settings* ``->`` *Text Events*
  ``$_[1]`` - the data that was passed to the hook function

- Timer callbacks

  ``$_[0]`` - the data that was passed to the hook function

- fd callbacks

  ``$_[0]`` - the handle that was passed to hook\_fd ``$_[1]`` - flags
  indicating why the callback was called ``$_[2]`` - the data that was
  passed to the hook function

Callback return values
^^^^^^^^^^^^^^^^^^^^^^

All server, command and print callbacks should return one of the
``ZoiteChat::EAT_*`` constants.
Timer callbacks can return ``ZoiteChat::REMOVE`` to remove the timer or
``ZoiteChat::KEEP`` to keep it going.

Miscellaneous Hook Related Information
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For server hooks, if ``$message`` is "RAW LINE" then ``$cb`` will be
called for every IRC message that ZoiteChat receives.

For command hooks if ``$command`` is "" then ``$cb`` will be called for
messages entered by the user that is not a command.

For print hooks besides those events listed in *Settings* ``->`` *Text
Events*, these additional events can be used.

+-----------------+--------------------------------------------------------------------------------------------+
| Event           | Description                                                                                |
+=================+============================================================================================+
| "Open Context"  | a new context is created                                                                   |
+-----------------+--------------------------------------------------------------------------------------------+
| "Close Context" | a context has been closed                                                                  |
+-----------------+--------------------------------------------------------------------------------------------+
| "Focus Tab"     | when a tab is brought to the front                                                         |
+-----------------+--------------------------------------------------------------------------------------------+
| "Focus Window"  | when a top level window is focused or the main tab window is focused by the window manager |
+-----------------+--------------------------------------------------------------------------------------------+
| "DCC Chat Text" | when text from a DCC Chat arrives. $_[0] will have these values                            |
|                 |                                                                                            |
|                 | - ``$_[0][0]`` - Address                                                                   |
|                 | - ``$_[0][1]`` - Port                                                                      |
|                 | - ``$_[0][2]`` - Nick                                                                      |
|                 | - ``$_[0][3]`` - Message                                                                   |
+-----------------+--------------------------------------------------------------------------------------------+
| "Key Press"     | used for intercepting key presses                                                          |
|                 |                                                                                            |
|                 | - ``$_[0][0]`` - key value                                                                 |
|                 | - ``$_[0][1]`` - state bitfield, 1 - shift, 4 - control, 8 - alt                           |
|                 | - ``$_[0][2]`` - string version of the key which might be empty for unprintable keys       |
|                 | - ``$_[0][3]`` - length of the string in ``$_[0][2]``                                      |
+-----------------+--------------------------------------------------------------------------------------------+


ZoiteChat::unhook( $hook )
~~~~~~~~~~~~~~~~~~~~~~~~

- **$hook** - the hook that was previously returned by one of the
  ``ZoiteChat::hook_*`` functions

This function is used to removed a hook previously added with one of the
``ZoiteChat::hook_*`` functions.

It returns the data that was passed to the ZoiteChat::hook_* function
when the hook was added.

ZoiteChat::print( $text | \@lines, [$channel,[$server]] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$text** - the text to print
- **\@lines** - array reference containing lines of text to be
  printed all the elements will be joined together before printing
- **$channel** - channel or tab with the given name where ``$text``
  will be printed
- **$server** - specifies that the text will be printed in a channel
  or tab that is associated with ``$server``

The first argument can either be a string or an array reference of
strings. Either or both of ``$channel`` and ``$server`` can be ``undef``.

If called as ZoiteChat::print( $text ), it will always return true. If
called with either the channel or the channel and the server specified
then it will return true if a context is found and false otherwise. The
text will not be printed if the context is not found. The meaning of
setting ``$channel`` or ``$server`` to ``undef`` is the same as
find\_context.

ZoiteChat::printf( $format, LIST )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$format** - a format string, see "perldoc -f
  `sprintf <http://perldoc.perl.org/functions/sprintf.html>`_" for
  further details
- **LIST** - list of values for the format fields

ZoiteChat::command( $command | \@commands, [$channel,[$server]] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$command** - the command to execute, without the leading /
- **\@commands** - array reference containing a list of commands to
  execute
- **$channel** - channel or tab with the given name where
  ``$command`` will be executed
- **$server** - specifies that the command will be executed in a
  channel or tab that is associated with ``$server``

The first argument can either be a string or an array reference of
strings. Either or both of ``$channel`` and ``$server`` can be ``undef``.

If called as ZoiteChat::command( $command ), it will always return true.
If called with either the channel or the channel and the server
specified then it will return true if a context is found and false
otherwise. The command will not be executed if the context is not found.
The meaning of setting ``$channel`` or ``$server`` to ``undef`` is the same
as find\_context.

ZoiteChat::commandf( $format, LIST )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$format** - a format string, see "perldoc -f
  `sprintf <http://perldoc.perl.org/functions/sprintf.html>`_" for
  further details
- **LIST** - list of values for the format fields

ZoiteChat::find_context( [$channel, [$server]] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$channel** - name of a channel
- **$server** - name of a server

Either or both of ``$channel`` and ``$server`` can be ``undef``. Calling
``ZoiteChat::find_context()`` is the same as calling
``ZoiteChat::find_context( undef, undef)`` and
``ZoiteChat::find_context( $channel )`` is the same as
``ZoiteChat::find_context( $channel, undef )``.

If ``$server`` is ``undef``, find any channel named ``$channel``. If
``$channel`` is ``undef``, find the front most window or tab named
``$server``.If both ``$channel`` and ``$server`` are ``undef``, find the
currently focused tab or window.

Return the context found for one of the above situations or ``undef`` if
such a context cannot be found.

ZoiteChat::get_context()
~~~~~~~~~~~~~~~~~~~~~~

Returns the current context.

ZoiteChat::set_context( $context | $channel,[$server] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$context** - context value as returned from ``get_context``,
  ``find_context`` or one of the fields in the list of hashrefs
  returned by ``list_get``
- **$channel** - name of a channel you want to switch context to
- **$server** - name of a server you want to switch context to

See ``find_context`` for more details on ``$channel`` and ``$server``.

Returns true on success, false on failure.

ZoiteChat::get_info( $id )
~~~~~~~~~~~~~~~~~~~~~~~~

- **$id** - one of the following case sensitive values

+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| ID                      | Return value                                                                                                        | Associated Command(s) |
+=========================+=====================================================================================================================+=======================+
| away                    | away reason or ``undef`` if you are not away                                                                        | AWAY, BACK            |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| channel                 | the original name of the tab, not affected by SETTAB                                                                | SETTAB                |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| charset                 | character-set used in the current context                                                                           | CHARSET               |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| configdir               | ZoiteChat config directory encoded in UTF-8. Examples:                                                                |                       |
|                         | /home/user/.config/zoitechat                                                                                          |                       |
|                         | C:\Users\user\Appdata\Roaming\ZoiteChat                                                                               |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| event_text <Event Name> | text event format string for <Event name>                                                                           |                       |
|                         | Example:                                                                                                            |                       |
|                         |                                                                                                                     |                       |
|                         | ``my $channel_msg_format = ZoiteChat::get_info( "event_text Channel Message" );``                                     |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| host                    | real hostname of the current server                                                                                 |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| id                      | connection id                                                                                                       |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| inputbox                | contents of the inputbox                                                                                            | SETTEXT               |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| libdirfs                | the system wide directory where ZoiteChat will look for plugins. this string is in the same encoding                  |                       |
|                         | as the local file system                                                                                            |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| modes                   | the current channels modes or ``undef`` if not known                                                                | MODE                  |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| network                 | current network name or ``undef``, this value is taken from the Network List                                        |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| nick                    | current nick                                                                                                        | NICK                  |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| nickserv                | nickserv password for this network or ``undef``, this value is taken from the Network List                          |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| server                  | current server name                                                                                                 |                       |
|                         | (what the server claims to be) ``undef`` if not connected                                                           |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| state\_cursor           | current inputbox cursor position in characters                                                                      | SETCURSOR             |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| topic                   | current channel topic                                                                                               | TOPIC                 |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| version                 | ZoiteChat's version number                                                                                            |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| win_status              | status of the ZoiteChat window, possible values are "active", "hidden" and "normal"                                   | GUI                   |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| win\_ptr                | native window pointer, GtkWindow * on Unix, HWND on Win32.                                                          |                       |
|                         | On Unix if you have the Glib module installed you can use my ``$window =                                            |                       |
|                         | Glib::Object->new\_from\_pointer( ZoiteChat::get_info( "win\_ptr" ) );`` to get a Gtk2::Window object.                |                       |
|                         | Additionally when you have detached tabs, each of the windows will return a different win\_ptr                      |                       |
|                         | for the different ``Gtk2::Window`` objects.                                                                         |                       |
|                         | See `char\_count.pl <http://xchat.cvs.sourceforge.net/viewvc/xchat/xchat2/plugins/perl/char_count.pl?view=markup>`_ |                       |
|                         | for a longer example of a script that uses this to show how many characters you currently have in your input box.   |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+
| gtkwin_ptr              | similar to win_ptr except it will always be a GtkWindow *                                                           |                       |
+-------------------------+---------------------------------------------------------------------------------------------------------------------+-----------------------+

This function is used to retrieve certain information about the current
context. If there is an associated command then that command can be used
to change the value for a particular ID.

ZoiteChat::get_prefs( $name )
~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$name** - name of a ZoiteChat setting (available through the /set
  command)

This function provides a way to retrieve ZoiteChat's setting information.

Returns ``undef`` if there is no setting called called ``$name``.

ZoiteChat::emit_print( $event, LIST )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$event** - name from the Event column in *Settings* ``->`` *Text
  Events*
- **LIST** - this depends on the Description column on the bottom of
  *Settings* ``->`` *Text Events*

This functions is used to generate one of the events listed under
*Settings* ``->`` *Text Events*.

Note: when using this function you **must** return ``ZoiteChat::EAT_ALL``
otherwise you will end up with duplicate events. One is the original and
the second is the one you emit.

Returns true on success, false on failure.

ZoiteChat::send_modes( $target | \@targets, $sign, $mode, [ $modes_per_line ] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$target** - a single nick to set the mode on
- **\@targets** - an array reference of the nicks to set the mode
  on
- **$sign** - the mode sign, either '+' or '-'
- **$mode** - the mode character such as 'o' and 'v', this can only
  be one character long
- **$modes_per_line** - an optional argument maximum number of modes
  to send per at once, pass 0 use the current server's maximum
  (default)

Send multiple mode changes for the current channel. It may send multiple
MODE lines if the request doesn't fit on one.

Example:

.. code-block:: perl

    use strict;
    use warnings;
    use ZoiteChat qw(:all);

    hook_command( "MODES", sub {
        my (undef, $who, $sign, $mode) = @{$_[0]};
        my @targets = split /,/, $who;
        if( @targets > 1 ) {
           send_modes( \@targets, $sign, $mode, 1 );
        } else {
           send_modes( $who, $sign, $mode );
        }
        return EAT_ZOITECHAT;
    });

ZoiteChat::nickcmp( $nick1, $nick2 )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$nick1, $nick2** - the two nicks or channel names that are to be
  compared

The comparsion is based on the current server. Either an
`RFC1459 <http://www.ietf.org/rfc/rfc1459.txt>`_ compliant string
compare or plain ascii will be using depending on the server. The
comparison is case insensitive.

Returns a number less than, equal to or greater than zero if ``$nick1``
is found respectively, to be less than, to match, or be greater than
``$nick2``.

ZoiteChat::get_list( $name )
~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$name** - name of the list, one of the following: "channels",
  "dcc", "ignore", "notify", "users"

This function will return a list of hash references. The hash references
will have different keys depend on the list. An empty list is returned
if there is no such list.

"channels" - list of channels, querys and their server

+--------------+-----------------------------------------------------------------------------+
| Key          | Description                                                                 |
+==============+=============================================================================+
| channel      | tab name                                                                    |
+--------------+-----------------------------------------------------------------------------+
| channelkey   | the key used to get into the channel                                        |
+--------------+-----------------------------------------------------------------------------+
| chantypes    | channel types supported by the server, typically "#&"                       |
+--------------+-----------------------------------------------------------------------------+
| context      | can be used with set_context                                                |
+--------------+-----------------------------------------------------------------------------+
| flags        | Server Bits:                                                                |
|              |                                                                             |
|              | - 0 Connected                                                               |
|              | - 1 Connecting                                                              |
|              | - 2 Away                                                                    |
|              | - 3 EndOfMotd(Login complete)                                               |
|              | - 4 Has WHOX                                                                |
|              | - 5 Has IDMSG (FreeNode)                                                    |
|              |                                                                             |
|              | The following correspond to the /chanopt command                            |
|              |                                                                             |
|              | - 6 Hide Join/Part Message (text_hidejoinpart)                              |
|              | - 7 unused (was for color paste)                                            |
|              | - 8 Beep on message (alert_beep)                                            |
|              | - 9 Blink Tray (alert_tray)                                                 |
|              | - 10 Blink Task Bar (alert_taskbar)                                         |
|              |                                                                             |
|              | Example of checking if the current context has Hide Join/Part messages set: |
|              |                                                                             |
|              | - 1                                                                         |
|              | - 2                                                                         |
|              | - 3                                                                         |
|              |                                                                             |
|              | .. code-block:: perl                                                        |
|              |                                                                             |
|              |    if( ZoiteChat::context_info->{flags} & (1 << 6) ) {                        |
|              |       ZoiteChat::print( "Hide Join/Part messages is enabled" );               |
|              |    }                                                                        |
+--------------+-----------------------------------------------------------------------------+
| id           | Unique server ID                                                            |
+--------------+-----------------------------------------------------------------------------+
| lag          | lag in milliseconds                                                         |
+--------------+-----------------------------------------------------------------------------+
| maxmodes     | Maximum modes per line                                                      |
+--------------+-----------------------------------------------------------------------------+
| network      | network name to which this channel belongs                                  |
+--------------+-----------------------------------------------------------------------------+
| nickmodes    | Nickname mode chars e.g. "vo"                                               |
+--------------+-----------------------------------------------------------------------------+
| nickprefixes | Nickname prefixes e.g. "+@"                                                 |
+--------------+-----------------------------------------------------------------------------+
| queue        | number of bytes in the send queue                                           |
+--------------+-----------------------------------------------------------------------------+
| server       | server name to which this channel belongs                                   |
+--------------+-----------------------------------------------------------------------------+
| type         | the type of this context                                                    |
|              | - 1 - server                                                                |
|              | - 2 - channel                                                               |
|              | - 3 - dialog                                                                |
|              | - 4 - notices                                                               |
|              | - 5 - server notices                                                        |
+--------------+-----------------------------------------------------------------------------+
| users        | Number of users in a channel                                                |
+--------------+-----------------------------------------------------------------------------+

"dcc" - list of DCC file transfers

+------------+------------------------------------------------------------------+
| Key        | Value                                                            |
+============+==================================================================+
| address32  | address of the remote user(ipv4 address)                         |
+------------+------------------------------------------------------------------+
| cps        | bytes per second(speed)                                          |
+------------+------------------------------------------------------------------+
| destfile   | destination full pathname                                        |
+------------+------------------------------------------------------------------+
| file       | file name                                                        |
+------------+------------------------------------------------------------------+
| nick       | nick of the person this DCC connection is connected to           |
+------------+------------------------------------------------------------------+
| port       | TCP port number                                                  |
+------------+------------------------------------------------------------------+
| pos        | bytes sent/received                                              |
+------------+------------------------------------------------------------------+
| poshigh    | bytes sent/received, high order 32 bits                          |
+------------+------------------------------------------------------------------+
| resume     | point at which this file was resumed                             |
|            | (zero if it was not resumed)                                     |
+------------+------------------------------------------------------------------+
| resumehigh | point at which this file was resumed, high order 32 bits         |
+------------+------------------------------------------------------------------+
| size       | file size in bytes low order 32 bits                             |
+------------+------------------------------------------------------------------+
| sizehigh   | file size in bytes, high order 32 bits (when the files is > 4GB) |
+------------+------------------------------------------------------------------+
| status     | DCC Status:                                                      |
|            |                                                                  |
|            | - 0 - queued                                                     |
|            |                                                                  |
|            | - 2 - failed                                                     |
|            | - 3 - done                                                       |
|            | - 4 - connecting                                                 |
|            | - 5 - aborted                                                    |
+------------+------------------------------------------------------------------+
| type       | DCC Type:                                                        |
|            |                                                                  |
|            | - 0 - send                                                       |
|            | - 1 - receive                                                    |
|            | - 2 - chatrecv                                                   |
|            | - 3 - chatsend                                                   |
+------------+------------------------------------------------------------------+

"ignore" - current ignore list

+-------+-----------------------------------+
| Key   | Value                             |
+=======+===================================+
| mask  | ignore mask. e.g: \*!*\@*.aol.com |
+-------+-----------------------------------+
| flags | Bit field of flags.               |
|       |                                   |
|       | - 0 - private                     |
|       | - 1 - notice                      |
|       | - 2 - channel                     |
|       | - 3 - ctcp                        |
|       | - 4 - invite                      |
|       | - 5 - unignore                    |
|       | - 6 - nosave                      |
|       | - 7 - dcc                         |
+-------+-----------------------------------+

"notify" - list of people on notify

+----------+----------------------------------------------------------------------------------------------+
| Key      | Value                                                                                        |
+==========+==============================================================================================+
| networks | comma separated list of networks where you will be notfified about this user's               |
|          | online/offline status or ``undef`` if you will be notificed on every network you are         |
|          | connected to                                                                                 |
+----------+----------------------------------------------------------------------------------------------+
| nick     | nickname                                                                                     |
+----------+----------------------------------------------------------------------------------------------+
| flags    | 0 = is online                                                                                |
+----------+----------------------------------------------------------------------------------------------+
| on       | time when user came online                                                                   |
+----------+----------------------------------------------------------------------------------------------+
| off      | time when user went offline                                                                  |
+----------+----------------------------------------------------------------------------------------------+
| seen     | time when user was last verified still online                                                |
+----------+----------------------------------------------------------------------------------------------+

The values indexed by on, off and seen can be passed to localtime and
gmtime, see perldoc -f
`localtime <http://perldoc.perl.org/functions/localtime.html>`_ and
perldoc -f `gmtime <http://perldoc.perl.org/functions/gmtime.html>`_ for
more details.

"users" - list of users in the current channel

+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| Key      | Value                                                                                                                             |
+==========+===================================================================================================================================+
| account  | account name or ``undef`` (2.9.6+)                                                                                                |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| away     | away status (boolean)                                                                                                             |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| host     | host name in the form: user@host or ``undef`` if not known                                                                        |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| lasttalk | last time a user was seen talking, this is the an epoch time                                                                      |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| nick     | nick name                                                                                                                         |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| prefix   | prefix character, .e.g: @ or +                                                                                                    |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| realname | Real name or ``undef``                                                                                                            |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+
| selected | selected status in the user list, only works when retrieving the user list of the focused tab.                                    |
|          | You can use the /USELECT command to select the nicks                                                                              |
+----------+-----------------------------------------------------------------------------------------------------------------------------------+

"networks" - list of networks and the associated settings from network list

+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| Key               | Value                                                                                                                                                                           |
+===================+=================================================================================================================================================================================+
| autojoins         | An object with the following methods:                                                                                                                                           |
|                   |                                                                                                                                                                                 |
|                   | - ``channels()``  - returns a list of this networks' autojoin channels in list context, a count of the number autojoin channels in scalar context                               |
|                   | - ``keys()``      - returns a list of the keys to go with the channels, the order is the same as the channels, if a channel doesn't have a key, '' will be returned in its      |
|                   |   place                                                                                                                                                                         |
|                   | - ``pairs()``     - a combination of channels() and keys(), returns a list of (channels, keys) pairs. This can be assigned to a hash for a mapping from channel to key.         |
|                   | - ``as_hash()``   - return the pairs as a hash reference                                                                                                                        |
|                   | - ``as_string()`` - the original string that was used to construct this autojoin object, this can be used with the JOIN command to join all the channels in the autojoin list   |
|                   | - ``as_array()``  - return an array reference of hash references consisting of the keys "channel" and "key"                                                                     |
|                   | - ``as_bool()``   - returns true if the network has autojoins and false otherwise                                                                                               |
|                   |                                                                                                                                                                                 |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| connect_commands  | An array reference containing the connect commands for a network. An empty array if there aren't any                                                                            |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| encoding          | the encoding for the network                                                                                                                                                    |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| flags             | a hash reference corresponding to the checkboxes in the network edit window                                                                                                     |
|                   |                                                                                                                                                                                 |
|                   |                                                                                                                                                                                 |
|                   | - allow_invalid - true if "Accept invalid SSL certificate" is checked                                                                                                           |
|                   | - autoconnect   - true if "Auto connect to this network at startup" is checked                                                                                                  |
|                   | - cycle         - true if "Connect to selected server only" is NOT checked                                                                                                      |
|                   | - use_global    - true if "Use global user information" is checked                                                                                                              |
|                   | - use_proxy     - true if "Bypass proxy server" is NOT checked                                                                                                                  |
|                   | - use_ssl       - true if "Use SSL for all the servers on this network" is checked                                                                                              |
|                   |                                                                                                                                                                                 |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| irc_nick1         | Corresponds with the "Nick name" field in the network edit window                                                                                                               |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| irc_nick2         | Corresponds with the "Second choice" field in the network edit window                                                                                                           |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| irc_real_name     | Corresponds with the "Real name" field in the network edit window                                                                                                               |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| irc_user_name     | Corresponds with the "User name" field in the network edit window                                                                                                               |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| network           | Name of the network                                                                                                                                                             |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| nickserv_password | Corresponds with the "Nickserv password" field in the network edit window                                                                                                       |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| selected          | Index into the list of servers in the "servers" key, this is used if the "cycle" flag is false                                                                                  |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| server_password   | Corresponds with the "Server password" field in the network edit window                                                                                                         |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
| servers           | An array reference of hash references with a "host" and "port" key. If a port is not specified then 6667 will be used.                                                          |
+-------------------+---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+

ZoiteChat::user_info( [$nick] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$nick** - the nick to look for, if this is not given your own
  nick will be used as default

This function is mainly intended to be used as a shortcut for when you
need to retrieve some information about only one user in a channel.
Otherwise it is better to use ``get_list``. If ``$nick`` is found a hash
reference containing the same keys as those in the "users" list of
``get_list`` is returned otherwise ``undef`` is returned. Since it relies on
``get_list`` this function can only be used in a channel context.

ZoiteChat::context_info( [$context] )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$context** - context returned from ``get_context``,
  ``find_context`` and ``get_list``, this is the context that you want
  infomation about. If this is omitted, it will default to current
  context.

This function will return the information normally retrieved with
``get_info``, except this is for the context that is passed in. The
information will be returned in a hashref. The keys of the hash
are the ``$id`` you would normally supply to ``get_info`` as well as all
the keys that are valid for the items in the "channels" list from
``get_list``. Use of this function is more efficient than calling
``get_list( "channels" )`` and searching through the result.

Example:

.. code-block:: perl

    use strict;
    use warnings;
    use ZoiteChat qw(:all); # imports all the functions documented on this page

    register( "User Count", "0.1",
       "Print out the number of users on the current channel" );
    hook_command( "UCOUNT", \&display_count );
    sub display_count {
        prnt "There are " . context_info()->{users} . " users in this channel.";
        return EAT_ZOITECHAT;
    }

ZoiteChat::plugin_pref_set( $setting, $value )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$setting** - name of the setting you want to store
- **$value** - value of that setting

This function allows you to store settings in addon_perl.conf via ZoiteChat.
Returns 1 on success, 0 on failure.

ZoiteChat::plugin_pref_get( $setting )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$setting** - name of the setting you want to retrieve

Returns the value of the specified setting or undef it is not found.

ZoiteChat::plugin_pref_delete( $setting )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$setting** - name of the setting you want to delete

Returns 1 on success (including deletion of not existing settings), 0 on failure.

ZoiteChat::plugin_pref_list( )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Returns a hashref of all stored settings or an empty hashref on failure.

ZoiteChat::strip_code( $string )
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- **$string** - string to remove codes from

This function will remove bold, color, beep, reset, reverse and
underline codes from ``$string``. It will also remove ANSI escape codes
which might get used by certain terminal based clients. If it is called
in void context ``$string`` will be modified otherwise a modified copy
of ``$string`` is returned.

Examples
--------

Asynchronous DNS resolution with hook\_fd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: perl

    use strict;
    use warnings;
    use ZoiteChat qw(:all);
    use Net::DNS;

    hook_command( "BGDNS", sub {
        my $host = $_[0][1];
        my $resolver = Net::DNS::Resolver->new;
        my $sock = $resolver->bgsend( $host );

        hook_fd( $sock, sub {
            my $ready_sock = $_[0];
            my $packet = $resolver->bgread( $ready_sock );

            if( $packet->authority && (my @answers = $packet->answer ) ) {

                if( @answers ) {
                    prnt "$host:";
                    my $padding = " " x (length( $host ) + 2);
                    for my $answer ( @answers ) {
                        prnt $padding . $answer->rdatastr . ' ' . $answer->type;
                    }
                }
            } else {
                prnt "Unable to resolve $host";
            }

            return REMOVE;
        },
        {
            flags => FD_READ,
        });

        return EAT_ZOITECHAT;
    });

Author Information
-------------------

This manual was originally written by Lian Wan Situ at <atmcmnky [at] yahoo.com>.
