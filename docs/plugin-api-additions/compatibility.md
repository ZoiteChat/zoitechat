# Optional plugin API additions (discussion #144)

Existing hooks, callbacks, return values, context lookup, strings and menu behaviour
remain unchanged. The C plugin function table gains four fixed append-only slots;
unused slots are NULL. Older binaries use the unchanged prefix. A plugin must check
the documented get_info() feature key before accessing a new slot on an older host.
New Python modules are installed by Meson and the existing Windows copy/installer
pipeline. Only modules present in the installation are exported; imports of
zoitechat, hexchat and xchat retain their compatibility aliases.

All APIs run on the client main thread unless explicitly documented otherwise.
New reserved flags must be zero. These changes do not make old API calls thread-safe.

Nested hook dispatch defers deleted-hook reclamation until its outermost return,
so new observers can safely emit events and unhook themselves or other hooks.

The optional asyncio module requires Python 3.7 or newer; older supported
interpreters skip that module and retain the existing scripting API.

Python reload drops the script/API modules while retaining the embedding module
so callback and lib.ph pointer types keep the same CFFI identity.

Server-hook insertion matches both raw and attrs hooks so documented priorities
are honoured for mixed registrations, including new filtered attrs callbacks.
