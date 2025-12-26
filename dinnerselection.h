#ifndef DINNERSELECTION_H
#define DINNERSELECTION_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class DinnerSelection;
}
QT_END_NAMESPACE

class DinnerSelection : public QWidget
{
    Q_OBJECT

public:
    DinnerSelection(QWidget *parent = nullptr);
    ~DinnerSelection();

private:
    Ui::DinnerSelection *ui;
};
#endif // DINNERSELECTION_H
