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

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("QtDashboard");
    app.setOrganizationName("LiaisonOS");

    MainWindow w;
    w.show();

    return app.exec();
}
