"""Structured, opt-in hooks before the IRC command's core processing."""
from collections import namedtuple
import _zoitechat as api

__all__ = ['IRCEvent', 'hook_event', 'on_join', 'on_part', 'on_quit',
           'on_nick', 'on_kick', 'on_message', 'on_notice']
IRCEvent = namedtuple('IRCEvent', 'command prefix nick user host params context connection_id time')
_SUPPORTED = frozenset(('JOIN', 'PART', 'QUIT', 'NICK', 'KICK', 'PRIVMSG', 'NOTICE'))


def _parse(line):
    prefix = ''
    if line.startswith(':'):
        prefix, separator, line = line[1:].partition(' ')
        if not separator:
            return None
    command, separator, rest = line.partition(' ')
    params = []
    while rest:
        rest = rest.lstrip(' ')
        if rest.startswith(':'):
            params.append(rest[1:])
            break
        param, separator, rest = rest.partition(' ')
        if param:
            params.append(param)
    nick, marker, userhost = prefix.partition('!')
    user, marker, host = userhost.partition('@')
    return command.upper(), prefix, nick, user, host, tuple(params)


def hook_event(command, callback, userdata=None, priority=api.PRI_NORM):
    """callback(event, userdata) returns existing EAT_* constants."""
    command = command.upper()
    if command not in _SUPPORTED:
        raise ValueError('unsupported semantic event: ' + command)
    if not callable(callback):
        raise TypeError('callback must be callable')

    def dispatch(word, word_eol, data, attrs):
        if not word_eol:
            return api.EAT_NONE
        parsed = _parse(word_eol[0])
        if parsed is None or parsed[0] != command:
            return api.EAT_NONE
        event = IRCEvent(*(parsed + (api.get_context(), api.get_prefs('id'), attrs.time)))
        return callback(event, data)

    return api.hook_server_attrs(command, dispatch, userdata, priority)


def on_join(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('JOIN', callback, userdata, priority)


def on_part(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('PART', callback, userdata, priority)


def on_quit(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('QUIT', callback, userdata, priority)


def on_nick(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('NICK', callback, userdata, priority)


def on_kick(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('KICK', callback, userdata, priority)


def on_message(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('PRIVMSG', callback, userdata, priority)


def on_notice(callback, userdata=None, priority=api.PRI_NORM):
    return hook_event('NOTICE', callback, userdata, priority)
