import QtQuick 2.15
import QtLocation 6.5
import QtPositioning 6.5

Item {
    anchors.fill: parent

    Plugin {
        id: mapPlugin
        name: "osm"   // OpenStreetMap
    }

    Map {
        anchors.fill: parent
        plugin: mapPlugin

        center: QtPositioning.coordinate(25.0330, 121.5654) // 台北
        zoomLevel: 14
    }
}
