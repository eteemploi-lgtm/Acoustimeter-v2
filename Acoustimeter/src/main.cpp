#include <QApplication>
#include "LoginWindow.h"
#include "MainWindow.h"
#include "HardwareManager.h"
#include "StyleManager.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Acoustimeter");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Acoustique");

    StyleManager::instance().setTheme(StyleManager::Dark);
    app.setStyleSheet(StyleManager::instance().appStyleSheet());

    HardwareManager hw;                      // thread NI, partage entre connexion et supervision

    ConnectionConfig cfg;
    {
        LoginWindow login(&hw);
        QObject::connect(&login, &LoginWindow::connectionEstablished,
                         [&cfg](const ConnectionConfig& c) { cfg = c; });
        if (login.exec() != QDialog::Accepted) return 0;
    }

    MainWindow mainWin(cfg, &hw);
    mainWin.setStyleSheet(StyleManager::instance().mainStyleSheet());
    mainWin.showMaximized();
    return app.exec();
}
