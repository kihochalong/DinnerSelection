#include "dinnerselection.h"
#include "qtextedit.h"
#include "ui_dinnerselection.h"
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QRandomGenerator>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>
#include <QFormLayout>
#include <QDialogButtonBox>
#include <QSpinBox>

DinnerSelection::DinnerSelection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DinnerSelection)
{
    setFixedSize(1200, 800);
    setMinimumSize(600, 500);

    ui->setupUi(this);

    connect(ui->horizontalSlider, &QSlider::valueChanged, this, &DinnerSelection::snapSliderToStep);
    connect(ui->btnPlus, &QPushButton::clicked, this, &DinnerSelection::increasePrice);
    connect(ui->btnMinus, &QPushButton::clicked, this, &DinnerSelection::decreasePrice);

    connect(ui->checkBox, &QCheckBox::clicked, [=] {
        ui->checkBox_2->setChecked(false);
        ui->checkBox_3->setChecked(false);
    });
    connect(ui->checkBox_2, &QCheckBox::clicked, [=] {
        ui->checkBox->setChecked(false);
        ui->checkBox_3->setChecked(false);
    });
    connect(ui->checkBox_3, &QCheckBox::clicked, [=] {
        ui->checkBox->setChecked(false);
        ui->checkBox_2->setChecked(false);
    });

    connect(ui->sliderDistance, &QSlider::valueChanged, this, &DinnerSelection::onDistanceChanged);
    connect(ui->pushButton_4, &QPushButton::clicked, this, [=] {
        int v = ui->sliderDistance->value();
        if (v < ui->sliderDistance->maximum())
            ui->sliderDistance->setValue(v + 1);
    });
    connect(ui->pushButton_5, &QPushButton::clicked, this, [=] {
        int v = ui->sliderDistance->value();
        if (v > ui->sliderDistance->minimum())
            ui->sliderDistance->setValue(v - 1);
    });

    connect(ui->pushButton, &QPushButton::clicked, this, &DinnerSelection::applyFiltersAndShow);

    connect(ui->btnPick, &QPushButton::clicked, this, [=]() {
        if (currentFilteredRestaurants.isEmpty()) {
            ui->labelRandomResult->setText("🎲 隨機選取\n請先進行篩選");
            return;
        }

        int randomIndex = QRandomGenerator::global()->bounded(currentFilteredRestaurants.size());
        QJsonObject picked = currentFilteredRestaurants[randomIndex];

        QString name = picked["name"].toString();
        QJsonObject locObj = picked["geometry"].toObject()["location"].toObject();
        double lat = locObj["lat"].toDouble();
        double lon = locObj["lng"].toDouble();

        QObject *rootObj = mapWidget->rootObject();
        if (rootObj) {
            QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                      Q_ARG(QVariant, lat),
                                      Q_ARG(QVariant, lon),
                                      Q_ARG(QVariant, name));
        }

        double rating = picked["rating"].toDouble(-1);
        int priceLevel = picked["price_level"].toInt(-1);

        double dLat = (lat - 23.7019) * 111.0;
        double dLon = (lon - 120.4307) * 111.0 * cos(23.7019 * M_PI / 180.0);
        double distanceKm = sqrt(dLat * dLat + dLon * dLon);

        QString priceRange;
        if (picked.contains("custom_price_text")) {
            priceRange = picked["custom_price_text"].toString();
        } else {
            switch (priceLevel) {
            case 0: priceRange = "100內"; break;
            case 1: priceRange = "100~200"; break;
            case 2: priceRange = "200~300"; break;
            case 3: priceRange = "300~500"; break;
            case 4: priceRange = "500以上"; break;
            default: priceRange = "未知"; break;
            }
        }

        ui->labelRandomResult->setText(
            QString("🎲 隨機結果：\n店名：%1\n⭐ 評分：%2\n💰 價位：%3\n📍 距離：%4 km")
                .arg(name)
                .arg(rating < 0 ? "無" : QString::number(rating))
                .arg(priceRange)
                .arg(QString::number(distanceKm, 'f', 2))
            );
    });

    connect(ui->btngood, &QPushButton::clicked, this, &DinnerSelection::showAddConfirmation);

    connect(ui->btnAdd, &QPushButton::clicked, this, [=]() {
        QObject *rootObj = mapWidget->rootObject();
        if (!rootObj) return;
        double currentLat = rootObj->property("centerLat").toDouble();
        double currentLon = rootObj->property("centerLng").toDouble();
        prepareManualAdd(currentLat, currentLon);
    });

    mapWidget = new QQuickWidget(this);
    mapWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    mapWidget->setSource(QUrl("qrc:/map.qml"));
    ui->mapLayout->addWidget(mapWidget);
    ui->labelMap->hide();

    ui->listRestaurant->setStyleSheet(
        "QListWidget::item { border-bottom: 1px solid #C0C0C0; padding: 8px; }"
        "QListWidget::item:selected { background-color: #e0f0ff; color: black; }"
        );

    connect(ui->listRestaurant, &QListWidget::itemClicked, this, [=]() {
        int currentRow = ui->listRestaurant->currentRow();
        if (currentRow >= 0 && currentRow < currentFilteredRestaurants.size()) {
            QJsonObject picked = currentFilteredRestaurants[currentRow];
            QJsonObject loc = picked["geometry"].toObject()["location"].toObject();
            double lat = loc["lat"].toDouble();
            double lng = loc["lng"].toDouble();
            QString name = picked["name"].toString();

            QObject *rootObj = mapWidget->rootObject();
            if (rootObj) {
                QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                          Q_ARG(QVariant, lat),
                                          Q_ARG(QVariant, lng),
                                          Q_ARG(QVariant, name));
            }
        }
    });

    network = new QNetworkAccessManager(this);
    connect(network, &QNetworkAccessManager::finished, this, &DinnerSelection::onPlacesReply);

    ui->label->hide();

    fetchPlaces(23.7019, 120.4307);
}

DinnerSelection::~DinnerSelection()
{
    delete ui;
}

void DinnerSelection::snapSliderToStep(int value)
{
    const int step = 100;
    int snapped = (value + step / 2) / step * step;
    if (snapped != value) {
        ui->horizontalSlider->setValue(snapped);
    }
    ui->label_3->setText(QString("%1 元").arg(snapped));
}

void DinnerSelection::increasePrice()
{
    const int step = 100;
    int value = ui->horizontalSlider->value();
    int max = ui->horizontalSlider->maximum();
    value += step;
    if (value > max) value = max;
    ui->horizontalSlider->setValue(value);
}

void DinnerSelection::decreasePrice()
{
    const int step = 100;
    int value = ui->horizontalSlider->value();
    int min = ui->horizontalSlider->minimum();
    value -= step;
    if (value < min) value = min;
    ui->horizontalSlider->setValue(value);
}

void DinnerSelection::onDistanceChanged(int value)
{
    maxDistanceKm = value;
    ui->labelDistanceValue->setText(QString("%1 公里內").arg(value));
}

void DinnerSelection::fetchPlaces(double lat, double lon, QString pageToken)
{
    QUrl url("https://maps.googleapis.com/maps/api/place/nearbysearch/json");
    QUrlQuery query;
    QString apiKey = "AIzaSyBs73o60jvr_scDSieQsGLJCIUhKmoBoOw";
    query.addQueryItem("key", apiKey);

    if (!pageToken.isEmpty()) {
        query.addQueryItem("pagetoken", pageToken);
    } else {
        query.addQueryItem("location", QString("%1,%2").arg(lat).arg(lon));
        query.addQueryItem("radius", "2000");
        query.addQueryItem("type", "restaurant");
        query.addQueryItem("language", "zh-TW");
    }

    url.setQuery(query);
    network->get(QNetworkRequest(url));
}

void DinnerSelection::onPlacesReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonObject root = QJsonDocument::fromJson(data).object();
    QJsonArray results = root["results"].toArray();
    for (const QJsonValue &v : results) {
        allRestaurants.append(v.toObject());
    }

    m_nextPageToken = root["next_page_token"].toString();

    if (!m_nextPageToken.isEmpty()) {
        QTimer::singleShot(500, this, [=]() {
            fetchPlaces(23.7019, 120.4307, m_nextPageToken);
        });
    } else {
        applyFiltersAndShow();
    }
    reply->deleteLater();
}

void DinnerSelection::applyFiltersAndShow()
{
    int sliderValue = ui->horizontalSlider->value();
    int maxPriceLevel = -1;
    if (sliderValue == 100) maxPriceLevel = 0;
    else if (sliderValue == 200) maxPriceLevel = 1;
    else if (sliderValue == 300) maxPriceLevel = 2;
    else if (sliderValue == 400) maxPriceLevel = 3;
    else if (sliderValue >= 500) maxPriceLevel = 4;

    ui->listRestaurant->clear();
    currentFilteredRestaurants.clear();

    double minRatingThreshold = 0.0;
    bool isRatingSelected = ui->checkBox->isChecked() || ui->checkBox_2->isChecked() || ui->checkBox_3->isChecked();
    if (ui->checkBox_3->isChecked()) minRatingThreshold = 4.5;
    else if (ui->checkBox_2->isChecked()) minRatingThreshold = 4.0;
    else if (ui->checkBox->isChecked()) minRatingThreshold = 3.5;

    for (const QJsonObject &obj : allRestaurants) {
        QString name = obj["name"].toString();
        double rating = obj["rating"].toDouble(-1);
        int priceLevel = obj["price_level"].toInt(-1);

        if (isRatingSelected && rating >= 0 && rating < minRatingThreshold)
            continue;

        if (sliderValue != 0) {
            if (priceLevel != -1 && priceLevel > maxPriceLevel)
                continue;
        }

        if (!obj.contains("geometry")) continue;
        QJsonObject loc = obj["geometry"].toObject()["location"].toObject();
        double dLat = (loc["lat"].toDouble() - 23.7019) * 111.0;
        double dLon = (loc["lng"].toDouble() - 120.4307) * 111.0 * cos(23.7019 * M_PI / 180.0);
        double distanceKm = sqrt(dLat * dLat + dLon * dLon);

        if (distanceKm > maxDistanceKm)
            continue;

        currentFilteredRestaurants.append(obj);

        QString priceRange;
        if (obj.contains("custom_price_text")) {
            priceRange = obj["custom_price_text"].toString();
        } else {
            switch (priceLevel) {
            case 0:  priceRange = "100內"; break;
            case 1:  priceRange = "100~200"; break;
            case 2:  priceRange = "200~300"; break;
            case 3:  priceRange = "300~500"; break;
            case 4:  priceRange = "500以上"; break;
            default: priceRange = "未知"; break;
            }
        }

        ui->listRestaurant->addItem(
            QString("🍽 %1\n 💰 %3\n⭐ %2\n📍 %4 km")
                .arg(name)
                .arg(rating < 0 ? "無" : QString::number(rating))
                .arg(priceRange)
                .arg(QString::number(distanceKm, 'f', 2))
            );
    }

    if (currentFilteredRestaurants.isEmpty()) {
        ui->listRestaurant->addItem("⚠️ 沒有符合篩選條件的餐廳");
    }
}

void DinnerSelection::showAddConfirmation()
{
    int currentRow = ui->listRestaurant->currentRow();
    if (currentRow < 0 || currentRow >= currentFilteredRestaurants.size()) {
        QMessageBox::warning(this, "提示", "請先從清單中選擇一家餐廳！");
        return;
    }

    QJsonObject picked = currentFilteredRestaurants[currentRow];
    QString name = picked["name"].toString();
    double rating = picked["rating"].toDouble(-1);

    QString info = QString("您是否要將以下店家加入收藏？\n\n店名：%1\n評分：⭐ %2")
                       .arg(name)
                       .arg(rating < 0 ? "無" : QString::number(rating));

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "確認新增", info, QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QMessageBox::information(this, "成功", name + " 已加入您的清單！");
    }
}

void DinnerSelection::prepareManualAdd(double lat, double lon) {
    QDialog dialog(this);
    dialog.setWindowTitle("新增中心點店家");
    QFormLayout form(&dialog);

    QLineEdit *nameEdit = new QLineEdit(&dialog);
    QLineEdit *priceRangeEdit = new QLineEdit(&dialog);
    priceRangeEdit->setPlaceholderText("輸入價格範圍 (如: 200~500)");

    form.addRow("店家名稱:", nameEdit);
    form.addRow("價格範圍:", priceRangeEdit);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);

    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted && !nameEdit->text().isEmpty()) {
        QJsonObject newStore;
        newStore["name"] = nameEdit->text();
        newStore["custom_price_text"] = priceRangeEdit->text();
        newStore["rating"] = 5.0;

        QJsonObject loc;
        loc["lat"] = lat;
        loc["lng"] = lon;
        QJsonObject geometry;
        geometry["location"] = loc;
        newStore["geometry"] = geometry;

        allRestaurants.append(newStore);
        applyFiltersAndShow();

        QObject *rootObj = mapWidget->rootObject();
        if (rootObj) {
            QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                      Q_ARG(QVariant, lat),
                                      Q_ARG(QVariant, lon),
                                      Q_ARG(QVariant, nameEdit->text()));
        }
    }
}
