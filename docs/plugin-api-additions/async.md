# Optional Python asyncio integration

`run_async(coroutine, context=None, interval=10)` returns an AsyncTask with
`done()`, `cancel()` and `result()`. Each invocation owns a separate event loop
pumped by an existing main-loop timer. A ready stop callback ensures selector
polling never blocks the UI. The captured context is restored after every pump;
a closed context cancels the work. No process-wide event-loop policy is changed.

Existing synchronous scripts are unaffected. Use `asyncio.get_running_loop()`
inside the coroutine. Child tasks use the same loop/context and are cancelled
when the main coroutine finishes or the script unloads. Cancellation cleanup is
bounded; tasks must not suppress cancellation indefinitely. Cleanup uses the
captured context while it remains open; after it closes, cancellation cleanup uses
the current valid context. Use explicit Context methods for context-specific
output in finally blocks, and do not issue context-dependent commands there. This provides normal
asyncio sockets/timers, not Windows subprocess support or a new HTTP library.
Asyncio hostname resolution can use its executor; use a resolver appropriate to
your library if you require no worker threads. Blocking work inside a coroutine
still blocks the client and must be avoided.

```python
import asyncio
import zoitechat

async def later():
    await asyncio.sleep(1)
    zoitechat.prnt('Still in the context where this task was started')

task = zoitechat.run_async(later())
```
