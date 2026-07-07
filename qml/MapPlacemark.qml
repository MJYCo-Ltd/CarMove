import QtQuick
import QtLocation
import QtPositioning

/// 地图上单一坐标点的展示：地名标签 / 搜索图钉 / 目标区图钉 / 导航起终点（原 MapGeoNameText + MapTargetAreaPin + MapVehicleLayer 内联 Component）
MapQuickItem {
    id: root

    /// "geoName" | "searchPin" | "targetPin" | "navEndpoint"
    property string placemarkKind: "geoName"

    // ── geoName（原 MapGeoNameText）────────────────────────────
    property string text: ""
    property alias alignEnd: geoLabel.alignEnd
    property alias nameFontPixelSize: geoLabel.nameFontPixelSize
    property alias nameTextColor: geoLabel.nameTextColor
    property alias nameStrokeColor: geoLabel.nameStrokeColor
    property alias nameStrokeEnabled: geoLabel.nameStrokeEnabled

    signal nameDoubleClicked()

    // ── navEndpoint（原 navEndpointPinComp）────────────────────
    property bool isStartPin: true
    property string pinName: ""
    property string pinPlateNumber: ""
    property bool peerValid: false
    property double peerLon: 0

    readonly property string resolvedPinLabel: pinName.length > 0 ? pinName
        : (isStartPin ? qsTr("起点") : qsTr("终点"))
    readonly property color navGeoNameColor: isStartPin ? "#d5f5e3" : "#fadbd8"
    readonly property bool nameLeftOfIcon: peerValid && (coordinate.longitude > peerLon)

    z: placemarkKind === "navEndpoint" ? 50
       : placemarkKind === "geoName" ? 37
       : placemarkKind === "targetPin" ? 36
       : 35

    readonly property bool _isGeo: placemarkKind === "geoName"
    readonly property bool _isSearch: placemarkKind === "searchPin"
    readonly property bool _isTarget: placemarkKind === "targetPin"
    readonly property bool _isNav: placemarkKind === "navEndpoint"

    anchorPoint.x: _isGeo ? (geoNameHitArea.width / 2)
        : _isSearch ? (searchCol.width / 2)
        : _isTarget ? (targetCol.width / 2)
        : navPinRoot.pinAnchorX
    anchorPoint.y: _isGeo ? geoNameHitArea.height
        : _isSearch ? searchCol.height
        : _isTarget ? targetCol.height
        : navPinRoot.height

    sourceItem: Item {
        id: srcRoot
        width: _isGeo ? geoNameHitArea.width
             : _isSearch ? searchCol.width
             : _isTarget ? targetCol.width
             : navPinRoot.width
        height: _isGeo ? geoNameHitArea.height
              : _isSearch ? searchCol.height
              : _isTarget ? targetCol.height
              : navPinRoot.height

        Item {
            id: geoNameHitArea
            visible: root._isGeo
            width: visible ? geoLabel.width : 0
            height: visible ? geoLabel.height : 0

            MapGeoNameLabel {
                id: geoLabel
                width: 200
                text: root.text
            }

            MouseArea {
                anchors.fill: parent
                onDoubleClicked: root.nameDoubleClicked()
            }
        }

        Column {
            id: searchCol
            visible: root._isSearch
            spacing: 0
            width: visible ? 28 : 0
            Rectangle {
                width: 28; height: 28; radius: 14
                color: "#8e44ad"; border.color: "white"; border.width: 2
                anchors.horizontalCenter: parent.horizontalCenter
                Text { anchors.centerIn: parent; text: "📍"; font.pixelSize: 14 }
            }
            Rectangle {
                width: 3; height: 10; color: "#8e44ad"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        Column {
            id: targetCol
            visible: root._isTarget
            spacing: 0
            width: visible ? 30 : 0
            Rectangle {
                width: 30; height: 30; radius: 15
                color: "#d35400"
                border.color: "white"
                border.width: 2
                anchors.horizontalCenter: parent.horizontalCenter
                Text {
                    anchors.centerIn: parent
                    text: "◎"
                    color: "white"
                    font.pixelSize: 16
                }
            }
            Rectangle {
                width: 3; height: 8; color: "#d35400"
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        Item {
            id: navPinRoot
            visible: root._isNav
            width: visible ? innerRow.width : 0
            height: visible ? innerRow.height : 0

            readonly property real pinAnchorX: root.nameLeftOfIcon
                ? (nameLCol.width + innerRow.spacing + pinCol.width / 2)
                : (pinCol.width / 2)

            Row {
                id: innerRow
                spacing: 6

                Column {
                    id: nameLCol
                    visible: root.nameLeftOfIcon
                    width: visible ? 200 : 0
                    NavEndpointNameSideColumn {
                        width: 200
                        alignNameEnd: true
                        plateAlignRight: true
                        displayName: root.resolvedPinLabel
                        nameTextColor: root.navGeoNameColor
                        plateNumber: root.pinPlateNumber
                        showPlate: root.isStartPin
                    }
                }

                Column {
                    id: pinCol
                    width: 32
                    spacing: 0
                    Rectangle {
                        width: 32; height: 32; radius: 16
                        color: root.isStartPin ? "#27ae60" : "#e74c3c"
                        border.color: "white"
                        border.width: 2
                        anchors.horizontalCenter: parent.horizontalCenter
                        Text {
                            anchors.centerIn: parent
                            text: root.isStartPin ? "起" : "终"
                            color: "white"
                            font.bold: true
                            font.pixelSize: 15
                        }
                    }
                    Rectangle {
                        width: 3; height: 8
                        color: root.isStartPin ? "#1e8449" : "#922b21"
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                Column {
                    id: nameRCol
                    visible: !root.nameLeftOfIcon
                    width: visible ? 200 : 0
                    NavEndpointNameSideColumn {
                        width: 200
                        alignNameEnd: false
                        plateAlignRight: false
                        displayName: root.resolvedPinLabel
                        nameTextColor: root.navGeoNameColor
                        plateNumber: root.pinPlateNumber
                        showPlate: root.isStartPin
                    }
                }
            }
        }
    }
}
