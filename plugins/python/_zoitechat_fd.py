"""File descriptor readiness hooks; callbacks execute on the main thread."""
import operator
import _zoitechat as api
from _zoitechat_embedded import ffi, lib

__all__ = ['FD_READ', 'FD_WRITE', 'FD_EXCEPTION', 'FD_NOTSOCKET', 'hook_fd']
FD_READ, FD_WRITE, FD_EXCEPTION, FD_NOTSOCKET = 1, 2, 4, 8


def hook_fd(fd, callback, userdata=None, flags=FD_READ):
    """callback(fd, ready_flags, userdata): True keeps the watch, False removes it."""
    fd = operator.index(fd)
    flags = operator.index(flags)
    if fd < 0 or fd > 2147483647:
        raise ValueError('fd must fit the C API nonnegative int descriptor')
    if flags & ~15 or not flags & 7:
        raise ValueError('flags must select READ, WRITE or EXCEPTION')
    if not callable(callback):
        raise TypeError('callback must be callable')
    plugin = api.__get_current_plugin()
    context = api.get_context()

    def dispatch(descriptor, ready, data):
        previous = api.get_context()
        if not context.set():
            return False
        try:
            return callback(descriptor, ready, data)
        finally:
            previous.set()

    hook = plugin.add_hook(dispatch, userdata)
    handle = lib.zoitechat_hook_fd(lib.ph, fd, flags, lib._on_fd_hook, hook.handle)
    if handle == ffi.NULL:
        hook.is_unload = True
        plugin.remove_hook(id(hook))
        raise RuntimeError('unable to watch descriptor')
    hook.zoitechat_hook = handle
    return id(hook)
