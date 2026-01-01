import QtQuick 2.15
import QtLocation 6.5
import QtPositioning 6.5
import QtQuick.Controls 2.15

Item {
    anchors.fill: parent

    Plugin {
        id: mapPlugin
        name: "osm"
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin

        center: QtPositioning.coordinate(23.7092, 120.5420)
        zoomLevel: 14

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

                    map.center.latitude  = map.startLat + dy * factor
                    map.center.longitude = map.startLon - dx * factor
                }
            }

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse
                onWheel: (event) => {
                    map.zoomLevel += event.angleDelta.y > 0 ? 0.2 : -0.2
                }
            }

            onReleased: {
                map.dragging = false
            }
        }
    }

        PositionSource {
            id: positionSource
            updateInterval: 1000
            active: false

            onPositionChanged: {
                console.log("--- 定位信號觸發 ---")
                var coord = position.coordinate
                if (coord.isValid) {
                    console.log("抓到有效座標:", coord.latitude, coord.longitude)

                    map.center = QtPositioning.coordinate(coord.latitude, coord.longitude)
                    map.zoomLevel = 15

                    stop()
                } else {
                    console.log("抓到的座標無效 (isValid = false)")
                }
            }


            onSourceErrorChanged: {
                console.log("定位錯誤碼:", sourceError)
                if (sourceError === PositionSource.AccessError) {
                    console.log("錯誤：作業系統拒絕存取位置（請檢查 Windows 隱私設定）")
                } else if (sourceError === PositionSource.ClosedError) {
                    console.log("錯誤：定位服務已關閉")
                }
            }
        }

        Dialog {
            id: permissionDialog
            title: "位置存取權限"
            anchors.centerIn: parent
            modal: true
            standardButtons: Dialog.Ok | Dialog.Cancel

            Text {
                text: "是否允許程式定位並跳轉地圖？"
                padding: 20
            }

            onAccepted: {
                console.log("使用者按下 OK，啟動 PositionSource...")
                positionSource.start()
            }
        }

        Button {
            text: "📍定位"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 30
            onClicked: permissionDialog.open()
        }
}
