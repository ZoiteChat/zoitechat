"""Opt-in post-display print observers."""
import _zoitechat as api
from _zoitechat_embedded import ffi, lib

__all__ = ['hook_print_after']


def hook_print_after(name, callback, userdata=None, priority=api.PRI_NORM):
    """callback(word, word_eol, userdata, attrs); its return value is ignored."""
    if not callable(callback):
        raise TypeError('callback must be callable')
    plugin = api.__get_current_plugin()
    hook = plugin.add_hook(callback, userdata)
    handle = lib.zoitechat_hook_print_after(lib.ph, name.encode(), priority, 0,
                                           lib._on_print_attrs_hook, hook.handle)
    if handle == ffi.NULL:
        hook.is_unload = True
        plugin.remove_hook(id(hook))
        raise RuntimeError('unable to install post-display hook')
    hook.zoitechat_hook = handle
    return id(hook)
