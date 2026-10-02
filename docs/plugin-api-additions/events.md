# Semantic IRC hooks

`on_join`, `on_part`, `on_quit`, `on_nick`, `on_kick`, `on_message`, and
`on_notice` accept `(callback, userdata=None, priority=PRI_NORM)`. The general
form is `hook_event(command, callback, userdata=None, priority=PRI_NORM)`.
Callbacks receive `(event, userdata)`, where the immutable IRCEvent has
command, prefix, nick, user, host, params, context, connection_id and time.
`params` preserves the trailing parameter, including spaces; JOIN parameters
also retain extended-join account/realname data when supplied by the server.

These wrappers use existing server hooks, before core command processing;
return the ordinary EAT_* constants. EAT_ZOITECHAT suppresses the entire IRC
command (including client state updates), not just its text. Use EAT_NONE for
observation. Other plugins' priority and eating rules remain unchanged.
The context is the core-selected context, which can be the server tab when a
query/channel does not yet exist. QUIT is one event per incoming command,
not one callback per affected channel. `event.time` is the existing server-time
attribute (zero when unavailable); other message tags are not exposed here.

```python
def joined(event, userdata):
    event.context.prnt('{} joined {}'.format(event.nick, event.params[0]))
    return zoitechat.EAT_NONE

zoitechat.on_join(joined)
```
