#include "dinnerselection.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    DinnerSelection w;
    w.show();
    return a.exec();
}
