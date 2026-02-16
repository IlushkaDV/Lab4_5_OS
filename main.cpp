#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setStyle("Fusion");
    
    MainWindow w;
    w.resize(1000, 750);
    w.show();
    
    return app.exec();
}
