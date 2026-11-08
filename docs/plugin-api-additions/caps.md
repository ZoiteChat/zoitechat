# Script-supported capabilities

`register_capability(name, userdata=None, connection_id=None)` returns a normal
hook handle. Register before connecting/reconnecting. Core requests the name only
if advertised in CAP LS or CAP NEW. Existing built-in requests remain enabled;
SASL and STS registration is rejected so scripts cannot change authentication or
transport policy through this API. None/-1 means any connection (zero is a valid
connection ID). Names are lowercase ASCII alphanumerics plus - . / _, 1-200 bytes.
Duplicate registrations do not duplicate requests for an advertised token.

`unhook(handle)` or script/plugin unload removes future request ownership. It
intentionally does not disable a capability already acknowledged on a live
connection, because another plugin/core may depend on it. Registering after CAP
LS does not retrospectively negotiate an already-advertised capability: reconnect
or wait for CAP NEW. Core retains ownership of CAP END; this API does not extend
SASL waiting or delay registration awaiting plugin-only CAP ACKs.

`hook_capability(name, callback, userdata=None, priority=PRI_NORM)` independently
observes negotiation through existing raw CAP hooks. Callback receives
`(CapabilityEvent, userdata)` with name, subcommand, value, enabled, context and
connection_id. LS/NEW mean advertised (`enabled=None`), ACK/LIST mean enabled
(unless a negative ACK), NAK means rejected and DEL means removed. Callback
returns are ignored so this observer cannot eat core negotiation. Existing raw
hooks may still eat CAP before this hook according to their normal priorities.

C: `zoitechat_register_capability(ph, name, flags, connection_id, userdata)`;
flags=0, ID=-1 for any. Check get_info("api_plugin_caps") for "1" before using
the appended Windows slot. Use existing server CAP hooks for notifications.
Requests are split at complete token boundaries within the existing wire limit.

```python
registration = zoitechat.register_capability('draft/example')
zoitechat.hook_capability('draft/example', lambda event, data:
                         event.context.prnt('{}: {}'.format(event.subcommand, event.enabled)))
```
