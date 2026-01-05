#include "dinnerselection.h"
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
#include <QLineEdit>
#include <cmath>

DinnerSelection::DinnerSelection(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DinnerSelection)
{
    setFixedSize(1200, 800);
    setMinimumSize(600, 500);
    ui->setupUi(this);

    // 1. 基本 UI 連線
    connect(ui->horizontalSlider, &QSlider::valueChanged, this, &DinnerSelection::snapSliderToStep);
    connect(ui->btnPlus, &QPushButton::clicked, this, &DinnerSelection::increasePrice);
    connect(ui->btnMinus, &QPushButton::clicked, this, &DinnerSelection::decreasePrice);

    // 2. 評分複選框互斥邏輯
    connect(ui->checkBox, &QCheckBox::clicked, [=] { ui->checkBox_2->setChecked(false); ui->checkBox_3->setChecked(false); });
    connect(ui->checkBox_2, &QCheckBox::clicked, [=] { ui->checkBox->setChecked(false); ui->checkBox_3->setChecked(false); });
    connect(ui->checkBox_3, &QCheckBox::clicked, [=] { ui->checkBox->setChecked(false); ui->checkBox_2->setChecked(false); });

    // 3. 距離連線
    connect(ui->sliderDistance, &QSlider::valueChanged, this, &DinnerSelection::onDistanceChanged);
    connect(ui->pushButton_4, &QPushButton::clicked, this, [=] { ui->sliderDistance->setValue(ui->sliderDistance->value() + 1); });
    connect(ui->pushButton_5, &QPushButton::clicked, this, [=] { ui->sliderDistance->setValue(ui->sliderDistance->value() - 1); });

    // 4. 按鈕功能連線
    connect(ui->pushButton, &QPushButton::clicked, this, &DinnerSelection::applyFiltersAndShow);

    // 隨機選取按鈕
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
                                      Q_ARG(QVariant, lat), Q_ARG(QVariant, lon), Q_ARG(QVariant, name));
        }

        double rating = picked["rating"].toDouble(-1);
        int level = picked["price_level"].toInt(-1);

        // 價格顯示邏輯
        QString priceRange = picked.contains("custom_price_text") ? picked["custom_price_text"].toString() :
                                 (level == -1 ? "一般價位" : (level == 0 ? "100內" : (level == 1 ? "100~200" : (level == 2 ? "200~300" : (level == 3 ? "300~500" : "500以上")))));

        double dLat = (lat - 23.7019) * 111.0;
        double dLon = (lon - 120.4307) * 111.0 * cos(23.7019 * M_PI / 180.0);
        double dist = sqrt(dLat * dLat + dLon * dLon);

        ui->labelRandomResult->setText(
            QString("🎲 隨機結果：\n店名：%1\n⭐ 評分：%2\n💰 價位：%3\n📍 距離：%4 km")
                .arg(name).arg(rating < 0 ? "無" : QString::number(rating))
                .arg(priceRange).arg(QString::number(dist, 'f', 2))
            );
    });

    // 新增店家按鈕
    connect(ui->btnAdd, &QPushButton::clicked, this, [=]() {
        QObject *rootObj = mapWidget->rootObject();
        if (!rootObj) return;

        QDialog dialog(this);
        dialog.setWindowTitle("新增中心點店家");
        QFormLayout form(&dialog);
        QLineEdit *nameEdit = new QLineEdit(&dialog);
        QLineEdit *priceEdit = new QLineEdit(&dialog);
        QDialogButtonBox bb(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form.addRow("店名:", nameEdit); form.addRow("價位:", priceEdit); form.addRow(&bb);
        connect(&bb, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&bb, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

        if (dialog.exec() == QDialog::Accepted && !nameEdit->text().isEmpty()) {
            QJsonObject ns;
            ns["name"] = nameEdit->text();
            ns["custom_price_text"] = priceEdit->text();
            ns["rating"] = 5.0;
            QJsonObject loc; loc["lat"] = rootObj->property("centerLat").toDouble(); loc["lng"] = rootObj->property("centerLng").toDouble();
            QJsonObject geo; geo["location"] = loc; ns["geometry"] = geo;
            allRestaurants.append(ns);
            applyFiltersAndShow();
        }
    });

    // 5. 初始化地圖組件
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
            QObject *rootObj = mapWidget->rootObject();
            if (rootObj) {
                QMetaObject::invokeMethod(rootObj, "updateMapMarker",
                                          Q_ARG(QVariant, loc["lat"].toDouble()),
                                          Q_ARG(QVariant, loc["lng"].toDouble()),
                                          Q_ARG(QVariant, picked["name"].toString()));
            }
        }
    });

    network = new QNetworkAccessManager(this);
    connect(network, &QNetworkAccessManager::finished, this, &DinnerSelection::onPlacesReply);

    fetchPlaces(23.7019, 120.4307);
}

DinnerSelection::~DinnerSelection() { delete ui; }

void DinnerSelection::snapSliderToStep(int value) {
    int snapped = (value + 50) / 100 * 100;
    if (snapped != value) ui->horizontalSlider->setValue(snapped);
    ui->label_3->setText(QString("%1 元").arg(snapped));
}

void DinnerSelection::increasePrice() { ui->horizontalSlider->setValue(qMin(ui->horizontalSlider->value() + 100, ui->horizontalSlider->maximum())); }
void DinnerSelection::decreasePrice() { ui->horizontalSlider->setValue(qMax(ui->horizontalSlider->value() - 100, ui->horizontalSlider->minimum())); }
void DinnerSelection::onDistanceChanged(int value) { maxDistanceKm = value; ui->labelDistanceValue->setText(QString("%1 公里內").arg(value)); }

void DinnerSelection::fetchPlaces(double lat, double lon, QString pageToken)
{
    QUrl url("https://maps.googleapis.com/maps/api/place/nearbysearch/json");
    QUrlQuery query;
    query.addQueryItem("key", "AIzaSyBs73o60jvr_scDSieQsGLJCIUhKmoBoOw");

    // 只有在第一次請求時才初始化列表
    if (pageToken.isEmpty()) {
        allRestaurants.clear();
    }

    if (!pageToken.isEmpty()) {
        query.addQueryItem("pagetoken", pageToken);
    } else {
        query.addQueryItem("location", QString("%1,%2").arg(lat).arg(lon));
        query.addQueryItem("radius", "5000"); // 擴大搜尋到 5 公里，獲取更多樣本
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

    QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
    QJsonArray results = root["results"].toArray();

    // 1. 直接顯示抓到的所有店家
    for (const QJsonValue &v : results) {
        QJsonObject obj = v.toObject();

        // 檢查重複
        bool exists = false;
        for (const QJsonValue &existingVal : allRestaurants) {
            if (existingVal.toObject()["place_id"].toString() == obj["place_id"].toString()) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            allRestaurants.append(obj);
            currentFilteredRestaurants.append(obj); // 初始清單包含所有抓到的店
            addRestaurantToUI(obj);
        }
    }

    // 2. 處理下一頁 Token
    m_nextPageToken = root["next_page_token"].toString();

    if (!m_nextPageToken.isEmpty()) {
        // 延遲 2 秒再抓下一頁，避免 Google 拒絕請求
        QTimer::singleShot(2000, this, [=]() {
            fetchPlaces(23.7019, 120.4307, m_nextPageToken);
        });
    }

    reply->deleteLater();
}

void DinnerSelection::addRestaurantToUI(const QJsonObject &obj)
{
    QString name = obj["name"].toString();
    double rating = obj["rating"].toDouble(-1);
    int priceLevel = obj["price_level"].toInt(-1);

    // 計算距離
    QJsonObject loc = obj["geometry"].toObject()["location"].toObject();
    double lat = loc["lat"].toDouble();
    double lng = loc["lng"].toDouble();

    double dLat = (lat - 23.7019) * 111.0;
    double dLon = (lng - 120.4307) * 111.0 * cos(23.7019 * M_PI / 180.0);
    double dist = sqrt(dLat * dLat + dLon * dLon);

    // 價位文字轉換
    QString priceRange = obj.contains("custom_price_text") ? obj["custom_price_text"].toString() :
                             (priceLevel == -1 ? "一般價位" :
                                  (priceLevel == 0 ? "100內" :
                                       (priceLevel == 1 ? "100~200" :
                                            (priceLevel == 2 ? "200~300" :
                                                 (priceLevel == 3 ? "300~500" : "500以上")))));

    ui->listRestaurant->addItem(
        QString("🍽 %1\n 💰 %2\n⭐ %3\n📍 %4 km")
            .arg(name).arg(priceRange)
            .arg(rating < 0 ? "無" : QString::number(rating))
            .arg(QString::number(dist, 'f', 2))
        );
}

void DinnerSelection::applyFiltersAndShow() {
    ui->listRestaurant->clear();
    currentFilteredRestaurants.clear();

    // 取得篩選門檻
    double minRatingThreshold = 0.0;
    if (ui->checkBox_3->isChecked()) minRatingThreshold = 4.5;
    else if (ui->checkBox_2->isChecked()) minRatingThreshold = 4.0;
    else if (ui->checkBox->isChecked()) minRatingThreshold = 3.5;

    int sliderValue = ui->horizontalSlider->value();
    int maxPriceLevel = (sliderValue / 100) - 1;
    if (sliderValue >= 500) maxPriceLevel = 4;

    for (const QJsonValue &value : allRestaurants) {
        QJsonObject obj = value.toObject(); // 修正：先轉為 Object 再進行操作
        double rating = obj["rating"].toDouble(-1);
        int priceLevel = obj["price_level"].toInt(-1);

        // 計算距離
        QJsonObject loc = obj["geometry"].toObject()["location"].toObject();
        double dLat = (loc["lat"].toDouble() - 23.7019) * 111.0;
        double dLon = (loc["lng"].toDouble() - 120.4307) * 111.0 * cos(23.7019 * M_PI / 180.0);
        double dist = sqrt(dLat * dLat + dLon * dLon);

        // 執行篩選判斷
        if (minRatingThreshold > 0 && rating < minRatingThreshold) continue;
        if (sliderValue > 0 && priceLevel != -1 && priceLevel > maxPriceLevel) continue;
        if (dist > maxDistanceKm) continue;

        // 符合條件者
        currentFilteredRestaurants.append(obj);
        addRestaurantToUI(obj);
    }

    if (currentFilteredRestaurants.isEmpty()) {
        ui->label->setText("✨ 每日推薦：\n目前無符合條件的店家");
        return;
    }

    // 🔒 如果還沒推薦過，才隨機選一次
    if (!hasRecommended) {
        int idx = QRandomGenerator::global()->bounded(currentFilteredRestaurants.size());
        recommendedRestaurant = currentFilteredRestaurants[idx];
        hasRecommended = true;
    }

    // 👉 之後都用同一家
    QJsonObject dp = recommendedRestaurant;

    QString dpPrice = dp.contains("custom_price_text")
                          ? dp["custom_price_text"].toString()
                          : "一般價位";

    QJsonObject loc = dp["geometry"].toObject()["location"].toObject();

    double dist = sqrt(
        pow((loc["lat"].toDouble() - 23.7019) * 111.0, 2) +
        pow((loc["lng"].toDouble() - 120.4307) * 111.0 *
                cos(23.7019 * M_PI / 180.0), 2)
        );

    ui->label->setText(
        QString("✨ 每日推薦：\n店名：%1\n評分：⭐ %2\n價位：💰 %3\n距離：📍 %4 km")
            .arg(dp["name"].toString())
            .arg(dp["rating"].toDouble(-1) < 0
                     ? "無"
                     : QString::number(dp["rating"].toDouble()))
            .arg(dpPrice)
            .arg(QString::number(dist, 'f', 2))
        );
}
