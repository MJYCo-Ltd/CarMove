import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 驾车导航：起点 / 终点 / 途经点 通过「搜索」弹出 GeoPickSearch 选用
Rectangle {
    id: navigationPanel
    color: "#f0f0f0"
    border.color: "#ccc"

    property var mapRef: null

    property bool originSet: false
    property double originLon: 0
    property double originLat: 0
    property string originName: "未设置"

    property bool destSet: false
    property double destLon: 0
    property double destLat: 0
    property string destName: "未设置"

    ListModel {
        id: waypointModel
    }

    function clearWaypointModel() {
        while (waypointModel.count > 0)
            waypointModel.remove(0)
    }

    function clearAllNavPoints() {
        originSet = false
        originLon = 0
        originLat = 0
        originName = "未设置"
        destSet = false
        destLon = 0
        destLat = 0
        destName = "未设置"
        navigationPanel.clearWaypointModel()
        if (mapRef)
            mapRef.clearNavigationEndpointMarkers()
    }

    function buildMidString() {
        var parts = []
        for (var i = 0; i < waypointModel.count; i++) {
            var o = waypointModel.get(i)
            parts.push(o.longitude + "," + o.latitude)
        }
        return parts.join(";")
    }

    function applyPickFromPopup(lat, lon, name, role) {
        navErrorStrip.text = ""
        if (role === 0) {
            navigationPanel.originSet = true
            navigationPanel.originLon = lon
            navigationPanel.originLat = lat
            navigationPanel.originName = name
            if (mapRef)
                mapRef.setNavigationStartMarker(lat, lon, name, navPlateField.text.trim())
        } else if (role === 1) {
            navigationPanel.destSet = true
            navigationPanel.destLon = lon
            navigationPanel.destLat = lat
            navigationPanel.destName = name
            if (mapRef)
                mapRef.setNavigationEndMarker(lat, lon, name)
        } else {
            waypointModel.append({
                                     "name": name,
                                     "latitude": lat,
                                     "longitude": lon
                                 })
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        PanelHeader {
            title: "驾车导航"
            subtitle: "点击「搜索」打开地名检索窗口，在结果中点「选用」设置起点、终点或途经点。与「搜索」页共用天地图服务与密钥。"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label {
                text: "车牌号"
                font.pixelSize: 12
                color: "#2c3e50"
            }
            TextField {
                id: navPlateField
                Layout.fillWidth: true
                placeholderText: "例如 冀A·12345（显示在起点旁）"
                font.pixelSize: 12
                maximumLength: 16
                onTextChanged: {
                    if (navigationPanel.originSet && mapRef)
                        mapRef.updateNavigationStartPlate(text.trim())
                }
            }
        }

        GroupBox {
            title: "已选路线点"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: "起点名称：" + navigationPanel.originName
                            font.pixelSize: 12
                            color: "#2c3e50"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            visible: navigationPanel.originSet
                            text: navigationPanel.originSet ? (navigationPanel.originLon.toFixed(6) + ", " + navigationPanel.originLat.toFixed(6)) : ""
                            font.pixelSize: 10
                            color: "#7f8c8d"
                            Layout.fillWidth: true
                        }
                    }
                    Button {
                        text: "搜索"
                        font.pixelSize: 11
                        onClicked: {
                            navGeoPickPopup.pickTargetRole = 0
                            navGeoPickPopup.open()
                        }
                    }
                    Button {
                        text: "清除"
                        font.pixelSize: 11
                        onClicked: {
                            navigationPanel.originSet = false
                            navigationPanel.originName = "未设置"
                            if (mapRef)
                                mapRef.clearNavigationStartMarker()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Label {
                            text: "终点名称：" + navigationPanel.destName
                            font.pixelSize: 12
                            color: "#2c3e50"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            visible: navigationPanel.destSet
                            text: navigationPanel.destSet ? (navigationPanel.destLon.toFixed(6) + ", " + navigationPanel.destLat.toFixed(6)) : ""
                            font.pixelSize: 10
                            color: "#7f8c8d"
                            Layout.fillWidth: true
                        }
                    }
                    Button {
                        text: "搜索"
                        font.pixelSize: 11
                        onClicked: {
                            navGeoPickPopup.pickTargetRole = 1
                            navGeoPickPopup.open()
                        }
                    }
                    Button {
                        text: "清除"
                        font.pixelSize: 11
                        onClicked: {
                            navigationPanel.destSet = false
                            navigationPanel.destName = "未设置"
                            if (mapRef)
                                mapRef.clearNavigationEndMarker()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        text: "途经点（按顺序）"
                        font.pixelSize: 12
                        color: "#2c3e50"
                        Layout.fillWidth: true
                    }
                    Button {
                        text: "搜索添加"
                        font.pixelSize: 11
                        onClicked: {
                            navGeoPickPopup.pickTargetRole = 2
                            navGeoPickPopup.open()
                        }
                    }
                }

                ListView {
                    id: wpList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(200, Math.max(44, waypointModel.count * 52 + 8))
                    model: waypointModel
                    clip: true
                    spacing: 4
                    visible: waypointModel.count > 0

                    delegate: Rectangle {
                        width: wpList.width
                        height: 48
                        color: "white"
                        border.color: "#dce0e8"
                        border.width: 1
                        radius: 4

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 6
                            Label {
                                text: (index + 1) + ". " + model.name
                                font.pixelSize: 11
                                color: "#2c3e50"
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Button {
                                text: "移除"
                                font.pixelSize: 11
                                onClicked: waypointModel.remove(index)
                            }
                        }
                    }
                }

                Text {
                    text: waypointModel.count === 0 ? "（无途经点）" : ""
                    font.pixelSize: 11
                    color: "#95a5a6"
                    visible: waypointModel.count === 0
                }

                Button {
                    text: "清空全部途经点"
                    font.pixelSize: 11
                    enabled: waypointModel.count > 0
                    onClicked: navigationPanel.clearWaypointModel()
                }
            }
        }

        GroupBox {
            title: "策略"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                ComboBox {
                    id: styleCombo
                    Layout.fillWidth: true
                    model: ["最快 (0)", "最短 (1)", "避开高速 (2)", "步行 (3)"]
                    font.pixelSize: 12
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: (typeof routePlanner !== "undefined" && routePlanner && routePlanner.busy) ? "规划中…" : "规划路线"
                Layout.fillWidth: true
                Layout.minimumWidth: 96
                enabled: typeof routePlanner !== "undefined" && routePlanner && !routePlanner.busy
                onClicked: {
                    navErrorStrip.text = ""
                    if (!navigationPanel.originSet || !navigationPanel.destSet) {
                        navErrorStrip.text = "请先通过「搜索」选用起点与终点"
                        return
                    }
                    var mid = navigationPanel.buildMidString()
                    routePlanner.requestRoute(navigationPanel.originLon, navigationPanel.originLat,
                                                navigationPanel.destLon, navigationPanel.destLat,
                                                styleCombo.currentIndex, mid)
                }
            }

            Button {
                text: "清除路线"
                onClicked: {
                    navErrorStrip.text = ""
                    if (mapRef)
                        mapRef.clearNavigationRoute()
                }
            }

            Button {
                text: "重置点"
                onClicked: {
                    navErrorStrip.text = ""
                    navigationPanel.clearAllNavPoints()
                }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: typeof routePlanner !== "undefined" && routePlanner && routePlanner.busy
            running: visible
            Layout.preferredHeight: 28
        }

        FormErrorStrip {
            id: navErrorStrip
        }

        Item {
            Layout.fillHeight: true
        }
    }

    Popup {
        id: navGeoPickPopup
        modal: true
        focus: true
        padding: 10
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnReleaseOutside

        property int pickTargetRole: 0

        readonly property string pickBanner: pickTargetRole === 0 ? "搜索并选用：起点" : (pickTargetRole === 1 ? "搜索并选用：终点" : "搜索并选用：途经点")

        parent: navigationPanel.Window ? navigationPanel.Window.contentItem : navigationPanel
        anchors.centerIn: parent
        width: Math.min(520, parent.width - 40)
        height: Math.min(640, parent.height - 40)

        background: Rectangle {
            color: "#f0f0f0"
            border.color: "#bdc3c7"
            border.width: 1
            radius: 6
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            Label {
                text: navGeoPickPopup.pickBanner
                font.pixelSize: 14
                font.bold: true
                color: "#2c3e50"
                Layout.fillWidth: true
            }

            GeoPickSearch {
                id: navGeoPickContent
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 420
                mapLocateMode: false
                autoLocateFirstResult: false
                acceptGeocoderResults: navGeoPickPopup.visible
                pickHintText: navGeoPickPopup.pickTargetRole === 0 ? "起点" : (navGeoPickPopup.pickTargetRole === 1 ? "终点" : "途经点")

                onPlacePicked: function (lat, lon, name) {
                    navigationPanel.applyPickFromPopup(lat, lon, name, navGeoPickPopup.pickTargetRole)
                    navGeoPickPopup.close()
                }
            }
        }
    }

    Connections {
        target: (typeof routePlanner !== "undefined") ? routePlanner : null
        enabled: navigationPanel.visible

        function onRouteReady(pathPoints) {
            navErrorStrip.text = ""
            if (mapRef && pathPoints && pathPoints.length > 0)
                mapRef.showNavigationRoute(pathPoints)
        }

        function onRouteFailed(msg) {
            navErrorStrip.text = msg
        }
    }
}
