# Core-filtered hooks

Python: `hook_server_filtered(name, callback, userdata=None, priority=PRI_NORM,
connection_id=None, channel=None)` and `hook_print_filtered(...)`.
Callbacks match the existing attrs variants: `(word, word_eol, userdata, attrs)`.
The connection ID distinguishes duplicate names; None/-1 means any connection
(including connection ID zero). A channel restriction uses that connection's
IRC comparator. None means any context; an empty string is an exact restriction.

C: the corresponding zoitechat_hook_*_filtered functions accept
`(ph, name, pri, flags, connection_id, channel, callback, userdata)`;
flags=0, ID=-1 for any and NULL channel for any. Check
`get_info("api_context_filters")` for "1" before accessing the new Windows slots.

Filtering takes place before Python/interpreter entry. Existing unrestricted
hooks, priority, EAT semantics and unload/unhook handling are unchanged.
Filters refer to the core-selected context, not an IRC target parsed from the
line: unknown queries/channels can still have a server-tab context, and QUIT
has its usual single server-hook context. Connection IDs last only this process.

```python
zoitechat.hook_print_filtered('Channel Message', callback,
                             connection_id=zoitechat.get_prefs('id'), channel='#zoitechat')
```
