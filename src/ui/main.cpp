#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QString>
#include <QSurfaceFormat>

#include "MainWindow.h"

static void applyFont()
{
    const QStringList preferred = {
        "Noto Sans",
        "JetBrainsMonoNL Nerd Font",
        "JetBrains Mono",
        "DejaVu Sans",
        "Liberation Sans",
    };
    for (const QString &name : preferred) {
        if (QFontDatabase::hasFamily(name)) {
            QApplication::setFont(QFont(name, 10));
            return;
        }
    }
    QFont fallback;
    fallback.setStyleHint(QFont::SansSerif);
    fallback.setPointSize(10);
    QApplication::setFont(fallback);
}

int main(int argc, char *argv[])
{
    QSurfaceFormat fmt;
    fmt.setVersion(4, 1);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName("HyperVane");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("HyperVane");

    applyFont();

    MainWindow window;
    window.start();
    window.showFullScreen();

    return app.exec();
}
