#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/assets/icon2.png"));
    MainWindow w;
    w.show();
    return app.exec();
}
