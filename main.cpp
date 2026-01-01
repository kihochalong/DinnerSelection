#include "dinnerselection.h"
#include <QtWebView/QtWebView>
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qDebug() << QT_VERSION_STR;
    QtWebView::initialize();
    DinnerSelection w;
    w.show();
    return a.exec();
}
