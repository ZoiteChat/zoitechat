"""Navigate live connections by ID instead of ambiguous server/network names."""
from collections import namedtuple
import operator
import _zoitechat as api

__all__ = ['Connection', 'get_connections', 'get_connection', 'get_contexts',
           'find_context_by_id']


class Connection(namedtuple('_Connection', 'id network server context')):
    __slots__ = ()

    def contexts(self, types=None):
        return get_contexts(self.id, types)

    def find(self, channel=None):
        return find_context_by_id(self.id, channel)


def _id(connection):
    return operator.index(connection.id if isinstance(connection, Connection) else connection)


def get_connections():
    """Return one snapshot per live connection, including disconnected ones."""
    entries = api.get_list('channels')
    by_id = {}
    order = []
    for entry in entries:
        if entry.id not in by_id:
            order.append(entry.id)
        if entry.id not in by_id or entry.type == 1:
            by_id[entry.id] = Connection(entry.id, entry.network, entry.server,
                                         entry.context)
    return [by_id[connection_id] for connection_id in order]


def get_connection(context=None):
    """Find the owning connection of a Context; return None when it has closed."""
    context = context or api.get_context()
    for entry in api.get_list('channels'):
        if entry.context == context:
            for connection in get_connections():
                if connection.id == entry.id:
                    return connection
    return None


def get_contexts(connection_id=None, types=None):
    """Return contexts, optionally restricted to a connection and session types."""
    if connection_id is not None:
        connection_id = _id(connection_id)
    if types is not None:
        types = frozenset(types)
    return [entry.context for entry in api.get_list('channels')
            if (connection_id is None or entry.id == connection_id)
            and (types is None or entry.type in types)]


def find_context_by_id(connection_id, channel=None):
    """Resolve a live context on exactly one connection; None selects its server tab."""
    connection_id = _id(connection_id)
    previous = api.get_context()
    try:
        for entry in api.get_list('channels'):
            if entry.id != connection_id:
                continue
            if channel is None:
                if entry.type == 1:
                    return entry.context
            elif entry.context.set() and api.nickcmp(entry.channel, channel) == 0:
                return entry.context
    finally:
        previous.set()
    return None
