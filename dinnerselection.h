#ifndef DINNERSELECTION_H
#define DINNERSELECTION_H

#include <QWidget>
#include <QQuickWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QQuickItem>
#include <QPushButton>
#include <QMessageBox>
#include <QSpinBox>

QT_BEGIN_NAMESPACE
namespace Ui { class DinnerSelection; }
QT_END_NAMESPACE

class DinnerSelection : public QWidget
{
    Q_OBJECT

public:
    explicit DinnerSelection(QWidget *parent = nullptr);
    ~DinnerSelection();

private slots:
    void snapSliderToStep(int value);
    void onDistanceChanged(int value);
    void increasePrice();
    void decreasePrice();
    void onPlacesReply(QNetworkReply *reply);
    void applyFiltersAndShow();
    void addRestaurantToUI(const QJsonObject &obj);
private:
    Ui::DinnerSelection *ui;
    QQuickWidget *mapWidget;
    QNetworkAccessManager *network;
    // 資料儲存
    QList<QJsonObject> allRestaurants;            // 原始資料池
    QList<QJsonObject> currentFilteredRestaurants; // 篩選後的抽籤池
    QString m_nextPageToken;                       // 分頁標記
    QJsonObject recommendedRestaurant;
    bool hasRecommended = false;
    // 篩選參數預設值
    double minRating = 0.0;
    int maxDistanceKm = 5;

    void fetchPlaces(double lat, double lon, QString pageToken = "");
    void showRestaurants(const QJsonArray &results);

};

#endif // DINNERSELECTION_H
