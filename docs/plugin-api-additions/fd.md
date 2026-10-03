# Python descriptor hooks

`hook_fd(fd, callback, userdata=None, flags=FD_READ)` returns an ordinary hook
handle accepted by `unhook()`. Callback arguments are `(fd, ready_flags, userdata)`;
return True to retain the watch and False/None to remove it. Exceptions print a
traceback and remove the watch. Unloading the script removes its watches.
The callback runs in the context captured during registration, and that context
is restored afterwards. A closed context removes the watch.

Use nonblocking IO and bounded reads/writes. The descriptor belongs to the script;
unhook before closing it. On Windows socket descriptors use the default flags;
CRT descriptors need FD_NOTSOCKET. The existing C API uses int descriptors: values
outside its range are rejected rather than truncated. This does not add threads.

Example:
```python
watch = zoitechat.hook_fd(sock.fileno(), on_readable, flags=zoitechat.FD_READ)
# def on_readable(fd, flags, userdata): ...; return True
```
