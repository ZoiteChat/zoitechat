# Explicit IRC comparison keys

`IRCName(text, context=None, casemapping=None, connection_id=None)` is an
immutable helper, not a str subclass. Construct it explicitly for dictionary/set
keys; existing get_info/list/event values remain normal strings.
Equality and hashing include the connection ID and captured casemapping, so
identical-looking names on different connections are different keys. An IRCName
never compares equal to a plain str. `str(key)` / `key.text` recover the spelling.

`irc_casefold(text, casemapping='rfc1459')` supports ascii, strict-rfc1459 and
rfc1459. Only ASCII IRC characters fold; Unicode casefold would be incorrect.
The new `get_info('casemapping')` reports the comparator actually used by core.
Current master handles ASCII and RFC1459; this patch does not change its handling
of advertised strict-rfc1459. An explicit mapping is available for scripts that
need strict-rfc1459 independently. Keys capture their mapping so hashes stay
stable; rebuild a collection if the connection's mapping changes.

```python
seen = {zoitechat.IRCName('[SomeNick]')}
assert zoitechat.IRCName('{somenick}') in seen  # RFC1459 connection
```
