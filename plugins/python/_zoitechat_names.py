"""Explicit immutable IRC names; existing string-returning APIs are unchanged."""
import _zoitechat as api

__all__ = ['IRCName', 'irc_casefold']


def irc_casefold(text, casemapping='rfc1459'):
    """Fold ASCII IRC characters only; never apply Unicode lower()/casefold()."""
    if casemapping not in ('ascii', 'strict-rfc1459', 'rfc1459'):
        raise ValueError('unsupported IRC casemapping')
    extra = {'[': '{', ']': '}', '\\': '|'} if casemapping != 'ascii' else {}
    if casemapping == 'rfc1459':
        extra['^'] = '~'
    return ''.join(chr(ord(c) + 32) if 'A' <= c <= 'Z' else extra.get(c, c)
                   for c in text)


class IRCName:
    """Equality/hash include connection identity and the captured casemapping."""
    __slots__ = ('_text', '_casemapping', '_connection_id', '_key')

    def __init__(self, text, context=None, casemapping=None, connection_id=None):
        context = context or api.get_context()
        if casemapping is None:
            casemapping = context.get_info('casemapping')
            if casemapping is None:
                raise RuntimeError('host does not expose its comparison casemapping')
        if connection_id is None:
            previous = api.get_context()
            if not context.set():
                raise ValueError('context has closed')
            try:
                connection_id = api.get_prefs('id')
            finally:
                previous.set()
        object.__setattr__(self, '_text', text)
        object.__setattr__(self, '_casemapping', casemapping)
        object.__setattr__(self, '_connection_id', connection_id)
        object.__setattr__(self, '_key', (connection_id, casemapping,
                                       irc_casefold(text, casemapping)))

    def __setattr__(self, name, value):
        raise AttributeError('IRCName is immutable')

    @property
    def text(self):
        return self._text

    @property
    def casemapping(self):
        return self._casemapping

    @property
    def connection_id(self):
        return self._connection_id

    def __str__(self):
        return self._text

    def __repr__(self):
        return 'IRCName({!r}, casemapping={!r}, connection_id={!r})'.format(
            self._text, self._casemapping, self._connection_id)

    def __eq__(self, other):
        if not isinstance(other, IRCName):
            return NotImplemented
        return self._key == other._key

    def __ne__(self, other):
        result = self.__eq__(other)
        return NotImplemented if result is NotImplemented else not result

    def __hash__(self):
        return hash(self._key)
