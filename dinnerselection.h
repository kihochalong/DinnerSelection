#ifndef DINNERSELECTION_H
#define DINNERSELECTION_H

#include <QWidget>
#include <QQuickWidget>

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
private slots:
    void snapSliderToStep(int value);
private slots:
    void increasePrice();
    void decreasePrice();
private:
    QQuickWidget *mapWidget;
};
#endif // DINNERSELECTION_H
