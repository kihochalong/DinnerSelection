#include "dinnerselection.h"
#include "ui_dinnerselection.h"

DinnerSelection::DinnerSelection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DinnerSelection)
{
    ui->setupUi(this);
}

DinnerSelection::~DinnerSelection()
{
    delete ui;
}
