import QtQuick 2.15
import QtLocation 6.5
import QtPositioning 6.5
import QtQuick.Controls 2.15

Item {
    id: root
    anchors.fill: parent
    property double centerLat: map.center.latitude
        property double centerLng: map.center.longitude
    // 提供給 C++ 呼叫的介面
    function updateMapMarker(lat, lon, name) {
        map.center = QtPositioning.coordinate(lat, lon)
        marker.coordinate = QtPositioning.coordinate(lat, lon)
        marker.visible = true
        markerLabel.text = name
        map.zoomLevel = 16 // 抽中時放大顯示
    }

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        center: QtPositioning.coordinate(23.7019, 120.4307)
        zoomLevel: 14
        property var lastClickedCoord: null

        TapHandler {
                onTapped: (eventPoint) => {
                    // 取得點擊處的經緯度
                    lastClickedCoord = Map.toCoordinate(eventPoint.position)
                    // 移動一個臨時標記到該位置（可選）
                    tempMarker.coordinate = lastClickedCoord
                }
            }

        MapQuickItem {
            id: marker
            visible: true
            anchorPoint.x: markerContent.width / 2
            anchorPoint.y: markerContent.height
            coordinate: QtPositioning.coordinate(0, 0)

            sourceItem: Column {
                id: markerContent
                spacing: 2
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: markerLabel.contentWidth + 10
                    height: markerLabel.contentHeight + 4
                    color: "white"
                    radius: 4
                    border.color: "red"
                    border.width: 1
                    Text {
                        id: markerLabel
                        anchors.centerIn: parent
                        font.bold: true
                        color: "black"
                    }
                }
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 15; height: 15
                    radius: 7.5
                    color: "red"
                    border.color: "white"
                }
            }
        }

        property bool dragging: false
        property real startX: 0
        property real startY: 0
        property real startLat: 0
        property real startLon: 0

        MouseArea {
            anchors.fill: parent
            onPressed: {
                map.dragging = true
                map.startX = mouse.x
                map.startY = mouse.y
                map.startLat = map.center.latitude
                map.startLon = map.center.longitude
            }
            onPositionChanged: {
                if (map.dragging) {
                    var baseFactor = 0.00005
                    var referenceZoom = 14
                    var dx = mouse.x - map.startX
                    var dy = mouse.y - map.startY
                    var zoomScale = Math.pow(2, referenceZoom - map.zoomLevel)
                    var factor = baseFactor * zoomScale
                    map.center = QtPositioning.coordinate(map.startLat + dy * factor, map.startLon - dx * factor)
                }
            }
            onReleased: map.dragging = false

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse
                onWheel: (event) => {
                    map.zoomLevel += event.angleDelta.y > 0 ? 0.2 : -0.2
                }
            }
        }
        Item {
                id: crosshair
                anchors.fill: parent
                z: 100 // 強制確保在最上層
                enabled: false // 讓滑鼠點擊可以穿透準星操作地圖

                // 垂直線
                Rectangle {
                    width: 2
                    height: 40
                    color: "red"
                    anchors.centerIn: parent
                }

                // 水平線
                Rectangle {
                    width: 40
                    height: 2
                    color: "red"
                    anchors.centerIn: parent
                }

                // 中心空心圓
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    color: "transparent"
                    border.color: "white"
                    border.width: 2
                    anchors.centerIn: parent
                }
            }
    }
}
