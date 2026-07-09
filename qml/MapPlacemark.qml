import QtQuick
import QtLocation
import QtPositioning
import "MapMarkerLayout.js" as MapMarkerLayout

/// 统一地图锚点标记：图标 + 视口自适应文字（vehicle / target / navEndpoint / searchPin / geoName）
MapQuickItem {
    id: root

    /// "vehicle" | "target" | "geoName" | "searchPin" | "targetPin"(兼容) | "navEndpoint"
    property string placemarkKind: "geoName"
    property var layoutMapView: null
    /// viewport：按屏幕边缘自适应；peer：导航起终点经度相对；fixed：固定锚点
    property string layoutMode: {
        if (placemarkKind === "navEndpoint")
            return "peer"
        if (placemarkKind === "searchPin" || placemarkKind === "targetPin")
            return "fixed"
        return "viewport"
    }
    property int edgeMargin: 28

    property string labelHorizontal: "right"
    property string labelVertical: "bottom"
    property real _layoutContentWidth: 80
    property real _layoutContentHeight: 40
    property real _layoutAnchorX: 40
    property real _layoutAnchorY: 20

    // ── 文字 / 目标 ────────────────────────────────────────────
    property string text: ""
    property alias nameFontPixelSize: primaryGeoLabel.nameFontPixelSize
    property alias nameTextColor: primaryGeoLabel.nameTextColor
    property alias nameStrokeColor: primaryGeoLabel.nameStrokeColor
    property alias nameStrokeEnabled: primaryGeoLabel.nameStrokeEnabled

    signal nameDoubleClicked()
    signal vehicleClicked(string plateNumber, double speed, int direction)

    // ── 车辆 ───────────────────────────────────────────────────
    property string plateNumber: ""
    property int direction: 0
    property double speed: 0
    property string vehicleColor: "#3498db"
    property int visitDays: 0

    // ── 导航起终点 ─────────────────────────────────────────────
    property bool isStartPin: true
    property string pinName: ""
    property string pinPlateNumber: ""
    property bool peerValid: false
    property double peerLon: 0

    readonly property string resolvedPinLabel: pinName.length > 0 ? pinName
        : (isStartPin ? qsTr("起点") : qsTr("终点"))
    readonly property color navGeoNameColor: isStartPin ? "#d5f5e3" : "#fadbd8"
    readonly property bool nameLeftOfIcon: peerValid && (coordinate.longitude > peerLon)

    readonly property bool _isVehicle: placemarkKind === "vehicle"
    readonly property bool _isTarget: placemarkKind === "target" || placemarkKind === "targetPin"
    readonly property bool _isTargetMerged: placemarkKind === "target"
    readonly property bool _isGeo: placemarkKind === "geoName"
    readonly property bool _isSearch: placemarkKind === "searchPin"
    readonly property bool _isNav: placemarkKind === "navEndpoint"
    readonly property bool _usesViewportLayout: layoutMode === "viewport"

    z: _isNav ? 50
       : _isVehicle ? 38
       : _isTargetMerged || _isTarget ? 37
       : _isGeo ? 37
       : 35

    anchorPoint.x: _usesViewportLayout ? _layoutAnchorX
        : _isGeo ? (legacyGeoLabel.alignEnd ? legacyGeoHitArea.width : 0)
        : _isSearch ? (searchCol.width / 2)
        : _isTarget && !_isTargetMerged ? (targetPinCol.width / 2)
        : navPinRoot.pinAnchorX
    anchorPoint.y: _usesViewportLayout ? _layoutAnchorY
        : _isGeo ? legacyGeoHitArea.height
        : _isSearch ? searchCol.height
        : _isTarget && !_isTargetMerged ? targetPinCol.height
        : navPinRoot.height

    function scheduleViewportLayout() {
        if (!_usesViewportLayout || !layoutMapView || !coordinate.isValid)
            return
        layoutTimer.restart()
    }

    onLayoutMapViewChanged: {
        if (!layoutMapView)
            layoutTimer.stop()
    }

    function updateViewportLayout() {
        if (!_usesViewportLayout || !layoutMapView || !coordinate.isValid)
            return

        var measured = {
            labelWidth: root._isVehicle ? vehiclePlateBadge.width
                        : (root._isGeo ? geoOnlyLabel.width : primaryGeoLabel.width),
            labelHeight: root._isVehicle ? vehiclePlateBadge.height
                        : (root._isGeo ? geoOnlyLabel.height : primaryGeoLabel.height)
        }
        var kind = _isTarget ? "target" : placemarkKind
        var metrics = MapMarkerLayout.metricsForKind(kind, measured)
        metrics.edgeMargin = edgeMargin
        var placement = MapMarkerLayout.computePlacement(layoutMapView, coordinate, metrics)
        labelHorizontal = placement.horizontal
        labelVertical = placement.vertical
        var anchor = MapMarkerLayout.computeAnchor(placement, metrics)
        _layoutAnchorX = anchor.anchorX
        _layoutAnchorY = anchor.anchorY
        _layoutContentWidth = anchor.contentWidth
        _layoutContentHeight = anchor.contentHeight
    }

    Timer {
        id: layoutTimer
        interval: 0
        repeat: false
        onTriggered: root.updateViewportLayout()
    }

    onCoordinateChanged: scheduleViewportLayout()
    onTextChanged: scheduleViewportLayout()
    onPlateNumberChanged: scheduleViewportLayout()
    Component.onCompleted: scheduleViewportLayout()

    Connections {
        target: layoutMapView
        enabled: layoutMapView !== null
        function onWidthChanged() { layoutTimer.restart() }
        function onHeightChanged() { layoutTimer.restart() }
    }

    Connections {
        target: layoutMapView && layoutMapView.map ? layoutMapView.map : null
        enabled: layoutMapView !== null && layoutMapView.map !== null
        function onCenterChanged() { layoutTimer.restart() }
        function onZoomLevelChanged() { layoutTimer.restart() }
        function onBearingChanged() { layoutTimer.restart() }
    }

    sourceItem: Item {
        id: srcRoot
        width: root._usesViewportLayout ? root._layoutContentWidth
             : root._isGeo ? legacyGeoHitArea.width
             : root._isSearch ? searchCol.width
             : root._isTarget && !root._isTargetMerged ? targetPinCol.width
             : navPinRoot.width
        height: root._usesViewportLayout ? root._layoutContentHeight
              : root._isGeo ? legacyGeoHitArea.height
              : root._isSearch ? searchCol.height
              : root._isTarget && !root._isTargetMerged ? targetPinCol.height
              : navPinRoot.height

        // ── 视口自适应：车辆 ─────────────────────────────────────
        Item {
            id: vehicleLayout
            visible: root._isVehicle && root._usesViewportLayout
            anchors.fill: parent

            Item {
                id: vehicleIconHost
                width: 24
                height: 24
                x: (parent.width - width) / 2
                y: root.labelVertical === "top"
                   ? (vehiclePlateBadge.height + 2)
                   : 0

                Rectangle {
                    id: vehicleIcon
                    width: 24
                    height: 24
                    color: root.vehicleColor
                    radius: 12
                    anchors.centerIn: parent
                    rotation: root.direction
                    border.color: "white"
                    border.width: 2

                    Rectangle {
                        width: 8
                        height: 2
                        color: "white"
                        radius: 1
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -6
                    }

                    Rectangle {
                        width: 4
                        height: 4
                        radius: 2
                        color: root.speed > 0 ? "#27ae60" : "#e74c3c"
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.rightMargin: -2
                        anchors.topMargin: -2
                    }
                }

                Rectangle {
                    id: visitDaysIndicator
                    width: visitDaysText.width + 6
                    height: visitDaysText.height + 4
                    color: "#e74c3c"
                    border.color: "white"
                    border.width: 1
                    radius: 8
                    visible: root.visitDays > 0
                    anchors.right: vehicleIcon.right
                    anchors.top: vehicleIcon.top
                    anchors.rightMargin: -8
                    anchors.topMargin: -8
                    z: 10

                    Text {
                        id: visitDaysText
                        text: root.visitDays.toString()
                        font.pixelSize: 10
                        font.bold: true
                        color: "white"
                        anchors.centerIn: parent
                    }
                }
            }

            MapPlateBadge {
                id: vehiclePlateBadge
                x: (parent.width - width) / 2
                y: root.labelVertical === "top" ? 0 : (vehicleIconHost.height + 2)
                plateText: root.plateNumber
                fontPixelSize: 11
                fontBold: false
                onWidthChanged: if (root.layoutMapView) root.scheduleViewportLayout()
                onHeightChanged: if (root.layoutMapView) root.scheduleViewportLayout()
            }
        }

        // ── 视口自适应：目标（图钉 + 名称）──────────────────────
        Item {
            id: targetLayout
            visible: root._isTargetMerged && root._usesViewportLayout
            anchors.fill: parent

            MapGeoNameLabel {
                id: primaryGeoLabel
                maxLabelWidth: 200
                text: root.text
                alignEnd: root.labelHorizontal === "left"
                nameFontPixelSize: 13
                nameTextColor: "#fdebd0"
                nameStrokeColor: "#1a1a1a"
                x: root.labelHorizontal === "left" ? 0 : (targetPinColumn.x + targetPinColumn.width + 6)
                y: Math.max(0, targetPinColumn.y + (targetPinColumn.height - height) / 2)
                onWidthChanged: if (root.layoutMapView) root.scheduleViewportLayout()
                onHeightChanged: if (root.layoutMapView) root.scheduleViewportLayout()
            }

            Column {
                id: targetPinColumn
                spacing: 0
                x: root.labelHorizontal === "left"
                   ? (primaryGeoLabel.width + 6)
                   : 0
                y: 0

                Rectangle {
                    width: 30
                    height: 30
                    radius: 15
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
                    width: 3
                    height: 8
                    color: "#d35400"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }

        // ── 视口自适应：纯文字 geoName ───────────────────────────
        Item {
            id: geoViewportLayout
            visible: root._isGeo && root._usesViewportLayout
            anchors.fill: parent

            MapGeoNameLabel {
                id: geoOnlyLabel
                maxLabelWidth: 200
                text: root.text
                alignEnd: root.labelHorizontal === "left"
                x: root.labelHorizontal === "left" ? (parent.width - width) : 0
                y: root.labelVertical === "top" ? 0 : (parent.height - height)
                onWidthChanged: if (root.layoutMapView) root.scheduleViewportLayout()
                onHeightChanged: if (root.layoutMapView) root.scheduleViewportLayout()
            }
        }

        // ── 兼容：旧 geoName 固定锚点 ───────────────────────────
        Item {
            id: legacyGeoHitArea
            visible: root._isGeo && !root._usesViewportLayout
            width: visible ? legacyGeoLabel.width : 0
            height: visible ? legacyGeoLabel.height : 0

            MapGeoNameLabel {
                id: legacyGeoLabel
                maxLabelWidth: 200
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
            id: targetPinCol
            visible: root._isTarget && !root._isTargetMerged
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

        MouseArea {
            anchors.fill: parent
            visible: root._isVehicle || root._isTargetMerged || (root._isGeo && root._usesViewportLayout)
            hoverEnabled: root._isVehicle
            onClicked: {
                if (root._isVehicle)
                    root.vehicleClicked(root.plateNumber, root.speed, root.direction)
            }
            onDoubleClicked: {
                if (root._isTargetMerged || (root._isGeo && root._usesViewportLayout))
                    root.nameDoubleClicked()
            }
            onEntered: {
                if (root._isVehicle)
                    vehicleIcon.scale = 1.2
            }
            onExited: {
                if (root._isVehicle)
                    vehicleIcon.scale = 1.0
            }
        }
    }
}
