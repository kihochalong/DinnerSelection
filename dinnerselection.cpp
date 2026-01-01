#include "dinnerselection.h"
#include "ui_dinnerselection.h"

DinnerSelection::DinnerSelection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DinnerSelection)
{
    ui->setupUi(this);
    connect(ui->horizontalSlider,
            &QSlider::valueChanged,
            this,
            &DinnerSelection::snapSliderToStep);
    connect(ui->btnPlus,  &QPushButton::clicked,
            this, &DinnerSelection::increasePrice);

    connect(ui->btnMinus, &QPushButton::clicked,
            this, &DinnerSelection::decreasePrice);
    mapWidget = new QQuickWidget(this);
    mapWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    mapWidget->setSource(QUrl("qrc:/map.qml"));

    ui->mapLayout->addWidget(mapWidget);
    ui->labelMap->hide();
}

DinnerSelection::~DinnerSelection()
{
    delete ui;
}

void DinnerSelection::snapSliderToStep(int value)
{
    const int step = 50;

    int snapped = (value + step / 2) / step * step;

    if (snapped != value) {
        ui->horizontalSlider->setValue(snapped);
    }
    ui->label_3->setText(QString("%1 元").arg(snapped));
}
void DinnerSelection::increasePrice()
{
    const int step = 50;

    int value = ui->horizontalSlider->value();
    int max   = ui->horizontalSlider->maximum();

    value += step;
    if (value > max) value = max;

    ui->horizontalSlider->setValue(value);
}

void DinnerSelection::decreasePrice()
{
    const int step = 50;

    int value = ui->horizontalSlider->value();
    int min   = ui->horizontalSlider->minimum();

    value -= step;
    if (value < min) value = min;

    ui->horizontalSlider->setValue(value);
}

