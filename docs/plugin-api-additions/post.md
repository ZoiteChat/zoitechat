# Post-display print observers

Python: `hook_print_after(name, callback, userdata=None, priority=PRI_NORM)`.
The callback is `(word, word_eol, userdata, attrs)`, matching hook_print_attrs().
It runs after display_event() has handled the text, so prnt() follows the original
text. Return values are ignored. Events eaten by a pre-hook or hidden by client
preferences do not invoke these observers. This is post-display notification,
not a notification that the entire incoming IRC command has finished updating state.

C: `zoitechat_hook_print_after(ph, name, priority, flags, callback, userdata)`;
flags must be zero. Check `get_info("api_hook_print_after")` for "1" before
accessing the appended Windows function-table slot. Existing pre-hook timing is
unchanged. Unhook/unload use existing lifecycle functions. Recursive emission
skips an already-running observer to prevent it immediately invoking itself.

```python
def after(word, word_eol, userdata, attrs):
    zoitechat.prnt('This follows the original channel message')

zoitechat.hook_print_after('Channel Message', after)
```
