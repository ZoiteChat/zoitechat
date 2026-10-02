"""Context restrictions evaluated by core before entering Python."""
import operator
import _zoitechat as api
from _zoitechat_embedded import ffi, lib

__all__ = ['hook_server_filtered', 'hook_print_filtered']


def _register(name, callback, userdata, priority, connection_id, channel, server):
    if not callable(callback):
        raise TypeError('callback must be callable')
    connection_id = -1 if connection_id is None else operator.index(connection_id)
    if connection_id < -1 or connection_id > 2147483647:
        raise ValueError('connection_id must fit the C API; -1 means any')
    plugin = api.__get_current_plugin()
    hook = plugin.add_hook(callback, userdata)
    register = lib.zoitechat_hook_server_filtered if server else lib.zoitechat_hook_print_filtered
    dispatch = lib._on_server_attrs_hook if server else lib._on_print_attrs_hook
    handle = register(lib.ph, name.encode(), priority, 0, connection_id,
                      ffi.NULL if channel is None else channel.encode(), dispatch, hook.handle)
    if handle == ffi.NULL:
        hook.is_unload = True
        plugin.remove_hook(id(hook))
        raise RuntimeError('unable to install filtered hook')
    hook.zoitechat_hook = handle
    return id(hook)


def hook_server_filtered(name, callback, userdata=None, priority=api.PRI_NORM,
                         connection_id=None, channel=None):
    return _register(name, callback, userdata, priority, connection_id, channel, True)


def hook_print_filtered(name, callback, userdata=None, priority=api.PRI_NORM,
                        connection_id=None, channel=None):
    return _register(name, callback, userdata, priority, connection_id, channel, False)
