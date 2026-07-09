import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 地点搜索：复用 GeoPickSearch（地图定位模式 + 首条自动定位）
SidePanelContainer {
    id: geoSearchPanel
    columnSpacing: 10

    signal locateRequested(double lat, double lon, string name)
    signal targetAreaRequested(double lat, double lon, string name)

    PanelHeader {
        title: "地点搜索"
    }

    GeoPickSearch {
        id: geoPickForSearch
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 220
        acceptGeocoderResults: geoSearchPanel.visible
        mapLocateMode: true
        autoLocateFirstResult: true
        pickHintText: ""

        onLocateRequested: function (lat, lon, name) {
            geoSearchPanel.locateRequested(lat, lon, name)
        }
        onTargetAreaRequested: function (lat, lon, name) {
            geoSearchPanel.targetAreaRequested(lat, lon, name)
        }
    }
}
