#include "mainwindow.h"
#include "theme.h"
#include <QApplication>
#include <QFont>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("TSBoss");
    app.setApplicationDisplayName("TSBoss – T/S Parameter Manager");
    app.setOrganizationName("TSBoss");
    app.setOrganizationDomain("tsboss.local");
    app.setApplicationVersion("1.2.0");

    // Window / taskbar icon — multi-size .ico embedded via resources.qrc.
    app.setWindowIcon(QIcon(":/TSBoss.ico"));

    // Clean base style
    app.setStyle(QStyleFactory::create("Fusion"));

    // Theme is loaded from QSettings; default is Dark.
    Theme::instance().applyToApplication(&app);

    // Default font: IBM Plex Sans @ 10pt
    QFont font("IBM Plex Sans");
    font.setStyleHint(QFont::SansSerif);
    font.setPointSize(10);
    app.setFont(font);

    MainWindow w;
    w.show();

    return app.exec();
}
