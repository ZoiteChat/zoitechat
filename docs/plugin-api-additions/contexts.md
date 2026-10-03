# Connection and context navigation

`get_connections()` returns one Connection snapshot per live connection:
`id`, `network`, `server`, `context`. Connection IDs distinguish simultaneous
connections with identical names. Prefer its server-tab context where present.
`get_connection(context=None)` finds the owner of a live context.

`get_contexts(connection_id=None, types=None)` lists live contexts, optionally
filtered by connection ID (or a Connection object) and session types: server=1,
channel=2, query=3. `connection.contexts(types=None)` is a convenience method.
`find_context_by_id(connection_id, channel=None)` / `connection.find(channel)`
resolve using that connection's IRC comparator; no channel selects the server tab.
Missing/closed contexts return None. Existing find_context() remains unchanged.

These helpers use the existing channels list's ID/context fields, so no new C
context ABI is needed. Snapshots are not persistent IDs across client restarts;
refresh them after closing/recreating connections. Always check Context.set()
when explicitly selecting a context that might have closed.

```python
connection = zoitechat.get_connection()
for context in connection.contexts(types=(2,)):
    context.prnt('A channel on this exact connection')
```
