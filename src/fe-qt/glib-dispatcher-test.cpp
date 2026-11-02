/*
 * Check that Qt's GLib dispatcher drives ZoiteChat's default GLib context.
 * On Qt builds using other dispatchers the application uses the explicit
 * fallback bridge, so this test is a no-op.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <QAbstractEventDispatcher>
#include <QApplication>
#include <QEventLoop>
#include <QTimer>
#include <glib.h>
#include <cstdio>

struct Probe {
    QEventLoop *loop;
    bool fired;
};

static gboolean glibTimeout(gpointer data)
{
    auto *probe = static_cast<Probe *>(data);
    probe->fired = true;
    probe->loop->quit();
    return G_SOURCE_REMOVE;
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QAbstractEventDispatcher *dispatcher = QAbstractEventDispatcher::instance();
    if (!dispatcher || !dispatcher->inherits("QEventDispatcherGlib")) {
        std::puts("Non-GLib Qt event loop: fallback path, skipping native integration test");
        return 0;
    }

    QEventLoop loop;
    Probe probe{&loop, false};
    const guint source = g_timeout_add(10, glibTimeout, &probe);
    QTimer::singleShot(500, &loop, &QEventLoop::quit);
    loop.exec();
    if (!probe.fired) {
        g_source_remove(source);
        std::fputs("GLib default context is not being dispatched by Qt\n", stderr);
        return 1;
    }
    std::puts("Qt dispatched a native GLib timer without polling");
    return 0;
}
