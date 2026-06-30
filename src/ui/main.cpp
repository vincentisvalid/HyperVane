#include <QApplication>
#include <QSurfaceFormat>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QSurfaceFormat fmt;
    fmt.setVersion(4, 6);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("HyperVane");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("HyperVane");

    MainWindow window;
    window.show();

    return app.exec();
}
