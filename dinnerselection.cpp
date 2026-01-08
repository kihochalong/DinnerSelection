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
#include <QDateTime>

DinnerSelection::DinnerSelection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DinnerSelection)
{
    setFixedSize(1200, 800);
    setMinimumSize(600, 500);

    ui->setupUi(this);
    connect(ui->horizontalSlider,
            &QSlider::valueChanged,
            this,
            &DinnerSelection::snapSliderToStep);
    connect(ui->btnPlus,  &QPushButton::clicked,
            this, &DinnerSelection::increasePrice);
    connect(ui->btnMinus, &QPushButton::clicked,
            this, &DinnerSelection::decreasePrice);
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
    connect(ui->sliderDistance, &QSlider::valueChanged,
            this, &DinnerSelection::onDistanceChanged);
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
    connect(ui->pushButton, &QPushButton::clicked,
            this, &DinnerSelection::applyFiltersAndShow);
    if (ui->btngood) {
        connect(ui->btngood, &QPushButton::clicked, this, [=](){
            int currentRow = ui->listRestaurant->currentRow();
            if (currentRow < 0 || currentRow >= currentFilteredRestaurants.size()) {
                QMessageBox::warning(this, "提示", "請先選擇餐廳");
                return;
            }

            QJsonObject picked = currentFilteredRestaurants[currentRow];

            // 避免重複加入喜好
            for (const auto &f : favoriteRestaurants) {
                if (f["name"].toString() == picked["name"].toString()) {
                    QMessageBox::information(this, "提示", "此餐廳已在喜好中");
                    return;
                }
            }

            favoriteRestaurants.append(picked);
            QMessageBox::information(this, "成功", picked["name"].toString() + " 已加入喜好！");
        });
    }

    connect(ui->btnPick, &QPushButton::clicked, this, [=]() {

        if (currentFilteredRestaurants.isEmpty()) {
            ui->labelRandomResult->setText("🎲 隨機選取\n請先進行篩選");
            return;
        }

        QVector<QJsonObject> pool;

        for (const auto &obj : currentFilteredRestaurants) {

            int weight = 1;
            QString name = obj["name"].toString();

            for (const auto &f : favoriteRestaurants) {
                if (f["name"].toString() == name) {
                    weight += 3;
                    break;
                }
            }

            int historyCount = 0;
            for (const auto &h : historyData) {
                if (h["name"].toString() == name)
                    historyCount++;
            }

            if (historyCount >= 2)
                weight = 0;
            else
                weight -= historyCount;

            weight = qMax(weight, 0);

            for (int i = 0; i < weight; ++i)
                pool.append(obj);
        }

        if (pool.isEmpty()) {
            ui->labelRandomResult->setText("🎲 沒有可抽選的餐廳");
            return;
        }

        QJsonObject picked =
            pool[QRandomGenerator::global()->bounded(pool.size())];

        QString name = picked["name"].toString();
        double rating = picked["rating"].toDouble(-1);

        // 價位
        QString priceRange;
        if (picked.contains("custom_price_text")) {
            priceRange = picked["custom_price_text"].toString();
        } else {
            int pl = picked["price_level"].toInt(-1);
            switch (pl) {
            case 0: priceRange = "100內"; break;
            case 1: priceRange = "100~200"; break;
            case 2: priceRange = "200~300"; break;
            case 3: priceRange = "300~500"; break;
            case 4: priceRange = "500以上"; break;
            default: priceRange = "未知"; break;
            }
        }

        // 距離
        QJsonObject loc = picked["geometry"].toObject()["location"].toObject();
        double dLat = (loc["lat"].toDouble() - 23.7019) * 111.0;
        double dLon = (loc["lng"].toDouble() - 120.4307) * 111.0
                      * cos(23.7019 * M_PI / 180.0);
        double dist = sqrt(dLat * dLat + dLon * dLon);

        // 地圖跳轉
        QObject *rootObj = mapWidget->rootObject();
        if (rootObj) {
            QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                      Q_ARG(QVariant, loc["lat"].toDouble()),
                                      Q_ARG(QVariant, loc["lng"].toDouble()),
                                      Q_ARG(QVariant, name));
        }

        ui->labelRandomResult->setText(
            QString("🎲 隨機結果：\n店名：%1\n⭐ 評分：%2\n💰 價位：%3\n📍 距離：%4 km")
                .arg(name)
                .arg(rating < 0 ? "無" : QString::number(rating))
                .arg(priceRange)
                .arg(QString::number(dist, 'f', 2))
            );
    });


    ui->listHistory->setStyleSheet(
        "QListWidget::item { border-bottom: 1px solid #E0E0E0; padding: 5px; font-size: 12px; }"
        "QListWidget::item:selected { background-color: #FFF9C4; color: black; }"
        );

    connect(ui->btngo, &QPushButton::clicked, this, [=]() {
        int currentRow = ui->listRestaurant->currentRow();

        if (currentRow < 0 || currentRow >= currentFilteredRestaurants.size()) {
            QMessageBox::warning(this, "提示", "請先選擇一家餐廳！");
            return;
        }

        QJsonObject picked = currentFilteredRestaurants[currentRow];
        QString name = picked["name"].toString();
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "出發確認",
            QString("確定要前往「%1」嗎？").arg(name),
            QMessageBox::Yes | QMessageBox::No
            );

        if (reply == QMessageBox::Yes) {
            QString timeStr = QDateTime::currentDateTime().toString("MM/dd HH:mm");
            ui->listHistory->addItem(QString("[%1] %2").arg(timeStr).arg(name));
            ui->listHistory->scrollToBottom();

            historyData.append(picked);
        }
    });

    connect(ui->listHistory, &QListWidget::itemClicked, this, [=]() {
        int row = ui->listHistory->currentRow();
        if (row >= 0 && row < historyData.size()) {
            QJsonObject picked = historyData[row];
            QJsonObject loc = picked["geometry"].toObject()["location"].toObject();

            QObject *rootObj = mapWidget->rootObject();
            if (rootObj) {
                QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                          Q_ARG(QVariant, loc["lat"].toDouble()),
                                          Q_ARG(QVariant, loc["lng"].toDouble()),
                                          Q_ARG(QVariant, picked["name"].toString()));
            }
        }
    });
    connect(ui->btnAdd, &QPushButton::clicked, this, [=]() {
        QObject *rootObj = mapWidget->rootObject();
        if (!rootObj) return;

        // 從 QML 地圖物件獲取目前中心點座標
        double currentLat = rootObj->property("centerLat").toDouble();
        double currentLon = rootObj->property("centerLng").toDouble();

        // 呼叫新增功能
        prepareManualAdd(currentLat, currentLon);
    });

    mapWidget = new QQuickWidget(this);
    mapWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);

    mapWidget->setSource(QUrl("qrc:/map.qml"));

    ui->mapLayout->addWidget(mapWidget);
    ui->labelMap->hide();
    ui->listRestaurant->setStyleSheet(
        "QListWidget::item { "
        "   border-bottom: 1px solid #C0C0C0; "
        "   padding: 8px; "
        "}"
        "QListWidget::item:selected { "
        "   background-color: #e0f0ff; " // 增加選中時的背景色，讓使用者知道點了哪項
        "   color: black; "
        "}"
        );
    connect(ui->listRestaurant, &QListWidget::itemClicked, this, [=]() {
        int currentRow = ui->listRestaurant->currentRow();

        // 檢查索引是否合法 (對應目前篩選後的餐廳清單)
        if (currentRow >= 0 && currentRow < currentFilteredRestaurants.size()) {
            QJsonObject picked = currentFilteredRestaurants[currentRow];

            // 取得經緯度與名稱
            QJsonObject loc = picked["geometry"].toObject()["location"].toObject();
            double lat = loc["lat"].toDouble();
            double lng = loc["lng"].toDouble();
            QString name = picked["name"].toString();

            // 呼叫 QML 函式讓地圖跳轉並標記位置
            QObject *rootObj = mapWidget->rootObject();
            if (rootObj) {
                // 使用您現有的 QML 介面函式
                QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                          Q_ARG(QVariant, lat),
                                          Q_ARG(QVariant, lng),
                                          Q_ARG(QVariant, name));

                // 可選：如果你希望地圖中心直接對準，也可以在 QML 裡把 map.center 設為該座標
                qDebug() << "地圖已跳轉至：" << name << "(" << lat << "," << lng << ")";
            }
        }
    });
    network = new QNetworkAccessManager(this);
    connect(network, &QNetworkAccessManager::finished,
            this, &DinnerSelection::onPlacesReply);
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
    int max   = ui->horizontalSlider->maximum();

    value += step;
    if (value > max) value = max;

    ui->horizontalSlider->setValue(value);
}

void DinnerSelection::decreasePrice()
{
    const int step = 100;

    int value = ui->horizontalSlider->value();
    int min   = ui->horizontalSlider->minimum();

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
        QJsonObject obj = v.toObject();

        bool exists = false;
        for (const auto &e : allRestaurants) {
            if (e["place_id"].toString() == obj["place_id"].toString()) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        allRestaurants.append(obj);
        currentFilteredRestaurants.append(obj);
        addRestaurantToUI(obj);
    }

    m_nextPageToken = root["next_page_token"].toString();

    if (!m_nextPageToken.isEmpty()) {
        QTimer::singleShot(500, this, [=]() {
            fetchPlaces(23.70310806, 120.43015111, m_nextPageToken);
        });
    }

    reply->deleteLater();
}

int DinnerSelection::calculateWeight(const QJsonObject &obj)
{
    int weight = 1; // 基礎權重

    QString name = obj["name"].toString();

    // 喜好加權
    for (const auto &f : favoriteRestaurants) {
        if (f["name"].toString() == name) {
            weight += 3;
            break;
        }
    }

    // 歷史降權
    int historyCount = 0;
    for (const auto &h : historyData) {
        if (h["name"].toString() == name) {
            historyCount++;
        }
    }

    if (historyCount >= 2)
        weight = 0;        // 避免一直抽到
    else
        weight -= historyCount;

    return qMax(weight, 0);
}

void DinnerSelection::applyFiltersAndShow()
{
    ui->listRestaurant->clear();
    currentFilteredRestaurants.clear();

    // ⭐ 評分門檻
    double minRatingThreshold = 0.0;
    if (ui->checkBox_3->isChecked())      minRatingThreshold = 4.5;
    else if (ui->checkBox_2->isChecked()) minRatingThreshold = 4.0;
    else if (ui->checkBox->isChecked())   minRatingThreshold = 3.5;

    // 💰 價格門檻
    int sliderValue = ui->horizontalSlider->value();
    int maxPriceLevel = -1;
    if (sliderValue == 100) maxPriceLevel = 0;
    else if (sliderValue == 200) maxPriceLevel = 1;
    else if (sliderValue == 300) maxPriceLevel = 2;
    else if (sliderValue == 400) maxPriceLevel = 3;
    else if (sliderValue >= 500) maxPriceLevel = 4;

    for (const QJsonObject &obj : allRestaurants) {

        double rating = obj["rating"].toDouble(-1);
        int priceLevel = obj["price_level"].toInt(-1);

        // 評分篩選
        if (minRatingThreshold > 0) {
            if (rating >= 0 && rating < minRatingThreshold) {
                continue; // 有評分但低於門檻才刷掉
            }
        }

        // 價格篩選
        if (sliderValue != 0) {
            if (priceLevel != -1 && priceLevel > maxPriceLevel) {
                continue; // 只有「明確太貴」才刷掉
            }
        }

        // 距離計算
        if (!obj.contains("geometry")) continue;
        QJsonObject loc = obj["geometry"].toObject()["location"].toObject();
        double dLat = (loc["lat"].toDouble() - 23.7019) * 111.0;
        double dLon = (loc["lng"].toDouble() - 120.4307) * 111.0
                      * cos(23.7019 * M_PI / 180.0);
        double dist = sqrt(dLat * dLat + dLon * dLon);

        if (dist > maxDistanceKm)
            continue;

        // 通過篩選
        currentFilteredRestaurants.append(obj);
        addRestaurantToUI(obj);
    }
    if (currentFilteredRestaurants.isEmpty()) {
        ui->labelRandomResult->setText("⚠️ 沒有符合條件的餐廳");
        return;
    }
}

void DinnerSelection::prepareManualAdd(double lat, double lon) {
    // 1. 建立對話盒 (包含名稱與價格範圍輸入)
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

    // 2. 執行並處理資料
    if (dialog.exec() == QDialog::Accepted && !nameEdit->text().isEmpty()) {

        // 封裝成與 Google API 一致的 JSON 格式，使其能被 applyFiltersAndShow 處理
        QJsonObject newStore;
        newStore["name"] = nameEdit->text();
        newStore["custom_price_text"] = priceRangeEdit->text(); // 自定義價格欄位
        newStore["rating"] = 5.0; // 手動新增預設滿分

        QJsonObject loc;
        loc["lat"] = lat;
        loc["lng"] = lon;
        QJsonObject geometry;
        geometry["location"] = loc;
        newStore["geometry"] = geometry;

        // 3. 加入清單並永久固定在目前的執行階段中
        allRestaurants.append(newStore);

        // 4. 立即刷新列表與顯示
        applyFiltersAndShow();

        // 5. 在地圖上目前的中心位置插上標記 (呼叫 QML 現有函式)
        QObject *rootObj = mapWidget->rootObject();
        if (rootObj) {
            QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                      Q_ARG(QVariant, lat),
                                      Q_ARG(QVariant, lon),
                                      Q_ARG(QVariant, nameEdit->text()));
        }
    }
}

void DinnerSelection::addRestaurantToUI(const QJsonObject &obj)
{
    QString name = obj["name"].toString();
    double rating = obj["rating"].toDouble(-1);

    // 價位
    QString priceRange;
    if (obj.contains("custom_price_text")) {
        priceRange = obj["custom_price_text"].toString();
    } else {
        int priceLevel = obj["price_level"].toInt(-1);
        switch (priceLevel) {
        case 0: priceRange = "100內"; break;
        case 1: priceRange = "100~200"; break;
        case 2: priceRange = "200~300"; break;
        case 3: priceRange = "300~500"; break;
        case 4: priceRange = "500以上"; break;
        default: priceRange = "未知"; break;
        }
    }

    // 距離
    QJsonObject loc = obj["geometry"].toObject()["location"].toObject();
    double dLat = (loc["lat"].toDouble() - 23.7019) * 111.0;
    double dLon = (loc["lng"].toDouble() - 120.4307) * 111.0 * cos(23.7019 * M_PI / 180.0);
    double distanceKm = sqrt(dLat * dLat + dLon * dLon);

    ui->listRestaurant->addItem(
        QString("🍽 %1\n 💰 %2\n⭐ %3\n📍 %4 km")
            .arg(name)
            .arg(priceRange)
            .arg(rating < 0 ? "無" : QString::number(rating))
            .arg(QString::number(distanceKm, 'f', 2))
        );
}


