#include <QApplication>
#include "mainwindow_v3.h"

int main(int argc, char **argv){
    QApplication app(argc, argv);
    MainWindowV3 w;
    w.show();
    return app.exec();
}
