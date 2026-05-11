//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : QtDashboard — LiaisonOS operational dashboard
//           Replaces et-dashboard (Python/GTK) with a native Qt application.
//           Supports Desktop Mode and Touch Mode via ~/.config/emcomm-tools/user.json
//

#include "MainWindow.h"
#include <QApplication>
#include <QScreen>
#include <QAbstractNativeEventFilter>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <xcb/xcb.h>

// Waits for MapNotify on our window, then sends _NET_WM_STATE SKIP_TASKBAR+SKIP_PAGER.
// This is reliable regardless of system speed — no arbitrary timer.
class SkipTaskbarFilter : public QAbstractNativeEventFilter
{
public:
    SkipTaskbarFilter(WId wid) : m_wid(wid) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, long *) override
    {
        if (eventType != "xcb_generic_event_t") return false;
        auto *ev = static_cast<xcb_generic_event_t*>(message);
        if ((ev->response_type & ~0x80) != XCB_MAP_NOTIFY) return false;
        auto *mn = reinterpret_cast<xcb_map_notify_event_t*>(ev);
        if (mn->window != (xcb_window_t)m_wid) return false;

        // Window is now mapped — apply skip flags via Xlib client message
        Display *dpy = XOpenDisplay(nullptr);
        if (dpy) {
            Window xwin      = m_wid;
            Window root      = DefaultRootWindow(dpy);
            Atom wmState     = XInternAtom(dpy, "_NET_WM_STATE",              False);
            Atom skipTaskbar = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
            Atom skipPager   = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER",   False);

            // Set property (for session restore)
            Atom atoms[2] = { skipTaskbar, skipPager };
            XChangeProperty(dpy, xwin, wmState, XA_ATOM, 32, PropModeReplace,
                            (unsigned char*)atoms, 2);

            // Send client message to WM
            auto send = [&](Atom state) {
                XClientMessageEvent e = {};
                e.type         = ClientMessage;
                e.window       = xwin;
                e.message_type = wmState;
                e.format       = 32;
                e.data.l[0]    = 1; // _NET_WM_STATE_ADD
                e.data.l[1]    = (long)state;
                e.data.l[3]    = 1;
                XSendEvent(dpy, root, False,
                           SubstructureNotifyMask | SubstructureRedirectMask,
                           (XEvent*)&e);
            };
            send(skipTaskbar);
            send(skipPager);
            XFlush(dpy);
            XCloseDisplay(dpy);
        }

        // Done — remove this filter (safe to delete immediately, no Qt event loop needed)
        QCoreApplication::instance()->removeNativeEventFilter(this);
        delete this;
        return false;
    }

private:
    WId m_wid;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QtDashboard");
    app.setOrganizationName("LiaisonOS");

    MainWindow w;

    // Install filter before show() so we catch the MapNotify event
    app.installNativeEventFilter(new SkipTaskbarFilter(w.winId()));

    w.show();

    return app.exec();
}
