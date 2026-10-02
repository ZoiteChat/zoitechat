"""Script-owned capability requests and structured negotiation observations."""
from collections import namedtuple
import operator
import _zoitechat as api
from _zoitechat_embedded import ffi, lib

__all__ = ['CapabilityEvent', 'register_capability', 'hook_capability']
CapabilityEvent = namedtuple('CapabilityEvent',
                            'name subcommand value enabled context connection_id')


def register_capability(name, userdata=None, connection_id=None):
    """Request an advertised capability on next negotiation; unhook releases ownership."""
    connection_id = -1 if connection_id is None else operator.index(connection_id)
    if connection_id < -1 or connection_id > 2147483647:
        raise ValueError('connection_id must fit the C API; -1 means any')
    plugin = api.__get_current_plugin()
    hook = plugin.add_hook(None, userdata)
    handle = lib.zoitechat_register_capability(lib.ph, name.encode(), 0,
                                              connection_id, hook.handle)
    if handle == ffi.NULL:
        hook.is_unload = True
        plugin.remove_hook(id(hook))
        raise ValueError('invalid capability name, reserved capability or connection ID')
    hook.zoitechat_hook = handle
    return id(hook)


def hook_capability(name, callback, userdata=None, priority=api.PRI_NORM):
    """Observe LS/NEW/ACK/NAK/DEL/LIST; this hook does not itself request the capability."""
    if not callable(callback):
        raise TypeError('callback must be callable')

    def dispatch(word, word_eol, data):
        if not word_eol:
            return api.EAT_NONE
        line = word_eol[0]
        if line.startswith(':'):
            unused, separator, line = line.partition(' ')
        parts = line.split(' ', 3)
        if len(parts) != 4 or parts[0].upper() != 'CAP':
            return api.EAT_NONE
        subcommand, tokens = parts[2].upper(), parts[3]
        if subcommand not in ('LS', 'NEW', 'ACK', 'NAK', 'DEL', 'LIST'):
            return api.EAT_NONE
        if tokens.startswith('* '):
            tokens = tokens[2:]
        tokens = tokens.lstrip(':')
        for token in tokens.split():
            negative = token.startswith('-')
            cap, equals, value = token.lstrip('-').partition('=')
            if cap != name:
                continue
            enabled = None
            if subcommand in ('ACK', 'LIST'):
                enabled = not negative
            elif subcommand in ('DEL', 'NAK'):
                enabled = False
            event = CapabilityEvent(cap, subcommand, value if equals else None,
                                    enabled, api.get_context(), api.get_prefs('id'))
            callback(event, data)
        return api.EAT_NONE  # observing CAP must not suppress core negotiation

    return api.hook_server('CAP', dispatch, userdata, priority)
