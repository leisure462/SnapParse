#include <QApplication>
#include <QFont>
#include "InstallerWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("SnapParse Installer");
    app.setOrganizationName("EcoPasteHub");
    app.setFont(QFont("Microsoft YaHei UI", 9));

    InstallerWindow window;
    window.show();

    return app.exec();
}
