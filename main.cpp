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
#include <QWindow>
#include <QTimer>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QtDashboard");
    app.setOrganizationName("LiaisonOS");

    MainWindow w;
    w.show();

    // Skip taskbar and pager — same as et-dashboard set_skip_taskbar_hint(True)
    QTimer::singleShot(0, [&w]() {
        Display *dpy = XOpenDisplay(nullptr);
        if (!dpy) return;
        Window xwin = w.winId();
        Atom wmState       = XInternAtom(dpy, "_NET_WM_STATE", False);
        Atom skipTaskbar   = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
        Atom skipPager     = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER", False);
        Atom atoms[2] = { skipTaskbar, skipPager };
        XChangeProperty(dpy, xwin, wmState, XA_ATOM, 32, PropModeAppend,
                        (unsigned char*)atoms, 2);
        XFlush(dpy);
        XCloseDisplay(dpy);
    });

    return app.exec();
}
