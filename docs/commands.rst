Commands
========

Commands in ZoiteChat are prefixed with **/** and to escape them you can type it twice e.g. **//**

ZoiteChat will first try to run plugin commands, then user commands, then client commands, and lastly send it directly to the server.

User Commands
-------------

User commands can be used to create aliases, to run multiple commands at a time, or more complex custom commands. They are set in :menuselection:`Settings --> User Commands`.

An alias is just a shortcut refering to an existing command, for example */j* refers to */join &2*

Naming two user commands the same thing will run both in the order they are listed.

For more complex commands you can use these codes:

- **%c** Current channel
- **%e** Current network
- **%m** Machine info
- **%n** Your nick
- **%t** Time/date
- **%v** ZoiteChat version
- **%<num>** Word
- **&<num>** Word from end of line

.. _commands_list:

List of Commands
----------------

To get a full list of commands and what they do type :command:`/help -l`.
