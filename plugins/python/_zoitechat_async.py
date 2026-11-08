"""Opt-in asyncio tasks pumped without blocking the client main loop."""
import asyncio
import operator
import traceback
import _zoitechat as api

__all__ = ['AsyncTask', 'run_async']


class AsyncTask:
    """A script-owned task and isolated event loop; cancel() is cooperative."""
    def __init__(self, coroutine, plugin, context, interval):
        self._plugin = plugin
        self._context = context
        self._loop = asyncio.new_event_loop()
        self._task = self._loop.create_task(coroutine)
        self._closed = False
        self._unload_hook = api.hook_unload(lambda data: self._close())
        self._timer_hook = api.hook_timer(interval, self._tick)

    def done(self):
        return self._task.done()

    def cancel(self):
        if not self._closed:
            return self._task.cancel()
        return False

    def result(self):
        return self._task.result()

    def _pump(self):
        # A ready stop callback forces the selector's timeout to zero.
        self._loop.call_soon(self._loop.stop)
        self._loop.run_forever()

    def _close(self):
        if self._closed:
            return
        self._closed = True
        previous = api.get_context()
        self._context.set()
        try:
            pending = asyncio.all_tasks(self._loop)
            for task in pending:
                task.cancel()
            # Bound cleanup work; tasks must cooperate with cancellation.
            for unused in range(3):
                if not any(not task.done() for task in pending):
                    break
                self._pump()
            for task in pending:
                if not task.done():
                    self._loop.call_exception_handler({
                        'message': 'Script task ignored cancellation during unload',
                        'task': task,
                    })
        finally:
            self._loop.close()
            previous.set()

    def _tick(self, userdata):
        if self._closed:
            return False
        previous = api.get_context()
        if not self._context.set():
            self._close()
            self._plugin.remove_hook(self._unload_hook)
            return False
        try:
            self._pump()
            if not self._task.done():
                return True
            try:
                self._task.result()
            except asyncio.CancelledError:
                pass
            except Exception:
                traceback.print_exc()
            self._close()
            self._plugin.remove_hook(self._unload_hook)
            return False
        except Exception:
            traceback.print_exc()
            self._close()
            self._plugin.remove_hook(self._unload_hook)
            return False
        finally:
            previous.set()


def run_async(coroutine, context=None, interval=10):
    """Schedule a coroutine in a captured context; return an AsyncTask handle."""
    interval = operator.index(interval)
    if interval < 1 or interval > 2147483647:
        raise ValueError('interval must be a positive C int in milliseconds')
    if not asyncio.iscoroutine(coroutine):
        raise TypeError('run_async expects a coroutine object')
    plugin = api.__get_current_plugin()
    return AsyncTask(coroutine, plugin, context or api.get_context(), interval)
