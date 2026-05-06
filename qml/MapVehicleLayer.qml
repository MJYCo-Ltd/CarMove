import QtQuick
import QtQuick.Controls
import QtLocation
import QtPositioning

// 注：导航起终点名称相对位置按经度与对端比较（国内图幅下大致对应左右），名称用描边提高可读性

// 车辆/轨迹/搜索图钉 统一管理层，需挂载在 MapDisplay 内部
Item {
    id: vehicleLayer

    property var mapTarget          // 地图对象 (mapView.map)
    property var animationsRef      // MapAnimations 实例
    property bool animationsEnabled: true
    property int  updateThrottleMs: 16
    property int  maxVehicleMarkers: 100
    property double targetLat: 0
    property double targetLon: 0

    // 内部状态
    property var    vehicleItems:    ({})
    property var    trajectoryItems: []
    property string currentVehicle:  ""
    property string currentVehicleColor: "#0061F6"
    property bool   autoFitEnabled:     true
    property bool   userHasInteracted:  false
    property var    searchResultPin:    null
    property var    navigationPolyline:  null
    property var    navigationStartPin:   null
    property var    navigationEndPin:     null

    signal vehicleClicked(string plateNumber, double speed, int direction)

    // ── 轨迹线模板 ───────────────────────────────────────────────
    Component {
        id: trajectoryPolylineComp
        // z 勿用负值：部分地图插件会把折线画在底图之下导致「线消失」
        MapPolyline {
            line.color: "red"
            line.width: 5
            opacity: 0.8
        }
    }

    // ── 搜索图钉模板 ─────────────────────────────────────────────
    Component {
        id: searchPinComp
        MapQuickItem {
            anchorPoint.x: pinCol.width / 2
            anchorPoint.y: pinCol.height
            sourceItem: Column {
                id: pinCol; spacing: 0
                Rectangle {
                    width: 28; height: 28; radius: 14
                    color: "#8e44ad"; border.color: "white"; border.width: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    Text { anchors.centerIn: parent; text: "📍"; font.pixelSize: 14 }
                }
                Rectangle { width: 3; height: 10; color: "#8e44ad"; anchors.horizontalCenter: parent.horizontalCenter }
            }
        }
    }

    // 导航起点/终点（绿/红）：名称相对图标左右由与对端经度关系决定，大字 + 描边
    Component {
        id: navEndpointPinComp
        MapQuickItem {
            id: navEpRoot
            z: 50
            property bool isStartPin: true
            property string pinName: ""
            /// 仅起点：导航车牌（样式与 VehicleMarker 车牌一致，叠在路线之上）
            property string pinPlateNumber: ""
            property bool peerValid: false
            property double peerLon: 0

            readonly property bool nameLeftOfIcon: peerValid && (coordinate.longitude > peerLon)

            anchorPoint.x: pinRoot.pinAnchorX
            anchorPoint.y: pinRoot.height

            sourceItem: Item {
                id: pinRoot
                width: innerRow.width
                height: innerRow.height

                readonly property real pinAnchorX: navEpRoot.nameLeftOfIcon
                    ? (nameLCol.width + innerRow.spacing + pinCol.width / 2)
                    : (pinCol.width / 2)

                Row {
                    id: innerRow
                    spacing: 6

                    Column {
                        id: nameLCol
                        visible: navEpRoot.nameLeftOfIcon
                        width: visible ? 200 : 0
                        spacing: 4

                        Item {
                            width: parent.width
                            height: visible ? navPlateBadgeL.height : 0
                            visible: navEpRoot.isStartPin && navEpRoot.pinPlateNumber.length > 0

                            Rectangle {
                                id: navPlateBadgeL
                                anchors.right: parent.right
                                width: navPlateTextL.width + 8
                                height: navPlateTextL.height + 4
                                color: "yellow"
                                border.color: "white"
                                border.width: 1
                                radius: 3

                                Text {
                                    id: navPlateTextL
                                    anchors.centerIn: parent
                                    text: navEpRoot.pinPlateNumber
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: "black"
                                    style: Text.Outline
                                    styleColor: "white"
                                }
                            }
                        }

                        OutlinedGeoNameText {
                            id: nameL
                            width: 200
                            alignEnd: true
                            isStartStyle: navEpRoot.isStartPin
                            fontPixelSize: 12
                            text: navEpRoot.pinName.length > 0 ? navEpRoot.pinName : (navEpRoot.isStartPin ? "起点" : "终点")
                        }
                    }

                    Column {
                        id: pinCol
                        width: 32
                        spacing: 0
                        Rectangle {
                            width: 32
                            height: 32
                            radius: 16
                            color: navEpRoot.isStartPin ? "#27ae60" : "#e74c3c"
                            border.color: "white"
                            border.width: 2
                            anchors.horizontalCenter: parent.horizontalCenter
                            Text {
                                anchors.centerIn: parent
                                text: navEpRoot.isStartPin ? "起" : "终"
                                color: "white"
                                font.bold: true
                                font.pixelSize: 15
                            }
                        }
                        Rectangle {
                            width: 3
                            height: 8
                            color: navEpRoot.isStartPin ? "#1e8449" : "#922b21"
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }

                    Column {
                        id: nameRCol
                        visible: !navEpRoot.nameLeftOfIcon
                        width: visible ? 200 : 0
                        spacing: 4

                        Item {
                            width: parent.width
                            height: visible ? navPlateBadgeR.height : 0
                            visible: navEpRoot.isStartPin && navEpRoot.pinPlateNumber.length > 0

                            Rectangle {
                                id: navPlateBadgeR
                                anchors.left: parent.left
                                width: navPlateTextR.width + 8
                                height: navPlateTextR.height + 4
                                color: "yellow"
                                border.color: "white"
                                border.width: 1
                                radius: 3

                                Text {
                                    id: navPlateTextR
                                    anchors.centerIn: parent
                                    text: navEpRoot.pinPlateNumber
                                    font.pixelSize: 15
                                    font.bold: true
                                    color: "black"
                                    style: Text.Outline
                                    styleColor: "white"
                                }
                            }
                        }

                        OutlinedGeoNameText {
                            id: nameR
                            width: 200
                            alignEnd: false
                            isStartStyle: navEpRoot.isStartPin
                            fontPixelSize: 12
                            text: navEpRoot.pinName.length > 0 ? navEpRoot.pinName : (navEpRoot.isStartPin ? "起点" : "终点")
                        }
                    }
                }
            }
        }
    }

    // ── 节流更新定时器 ────────────────────────────────────────────
    Timer {
        id: throttleTimer
        interval: vehicleLayer.updateThrottleMs; repeat: false
        property var pending: ({})
        onTriggered: {
            for (var p in pending) {
                var u = pending[p]
                vehicleLayer._updatePositionNow(p, u.coordinate, u.direction, u.speed)
            }
            pending = {}
        }
    }

    // ── 公共接口 ─────────────────────────────────────────────────

    function addVehicle(plateNumber, coordinate, direction, speed, color) {
        if (!vehicleItems[plateNumber]) {
            var item = Qt.createComponent("VehicleMarker.qml").createObject(mapTarget)
            if (item) {
                item.plateNumber = plateNumber
                item.vehicleColor = color || _colorForPlate(plateNumber)
                if (typeof controller !== 'undefined' && controller)
                    item.visitDays = controller.calculateVisitDays(plateNumber, targetLat, targetLon, 1000)
                item.vehicleClicked.connect(function(pn, spd, dir) { vehicleLayer.vehicleClicked(pn, spd, dir) })
                vehicleItems[plateNumber] = item
                mapTarget.addMapItem(item)
            }
        }
        var v = vehicleItems[plateNumber]
        if (v) { v.coordinate = coordinate; v.direction = direction; v.speed = speed }
    }

    function addVehicleTrajectory(plateNumber, trajectoryPoints, vehicleColor) {
        clearTrajectory()
        currentVehicle = plateNumber
        if (trajectoryPoints && trajectoryPoints.length > 1) {
            var line = trajectoryPolylineComp.createObject(mapTarget)
            if (line) {
                line.line.color = currentVehicleColor; line.line.width = 3
                for (var i = 0; i < trajectoryPoints.length; i++) {
                    var c = _extractCoord(trajectoryPoints[i])
                    if (c) line.addCoordinate(c)
                }
                mapTarget.addMapItem(line)
                trajectoryItems.push(line)
            }
        }
        if (trajectoryPoints && trajectoryPoints.length > 0) {
            var first = trajectoryPoints[0]
            var fc = _extractCoord(first)
            if (fc) {
                addVehicle(plateNumber, fc, first.direction || 0, first.speed || 0, currentVehicleColor)
                if (autoFitEnabled && !userHasInteracted) _fitViewport(trajectoryPoints)
            }
        }
    }

    function updateTrajectoryCoordinates(newPoints) {
        if (currentVehicle && newPoints && newPoints.length > 0) {
            resetInteraction()
            addVehicleTrajectory(currentVehicle, newPoints, currentVehicleColor)
        }
    }

    function clearTrajectory() {
        for (var i = 0; i < trajectoryItems.length; i++) mapTarget.removeMapItem(trajectoryItems[i])
        trajectoryItems = []
        for (var p in vehicleItems) mapTarget.removeMapItem(vehicleItems[p])
        vehicleItems = {}
        resetInteraction()
    }

    function updateVehiclePosition(plateNumber, coordinate, direction, speed) {
        if (throttleTimer.running) {
            throttleTimer.pending[plateNumber] = { coordinate: coordinate, direction: direction, speed: speed }
        } else {
            _updatePositionNow(plateNumber, coordinate, direction, speed)
            throttleTimer.pending = {}
            throttleTimer.start()
        }
    }

    function showSearchResult(lat, lon, name) {
        clearSearchResult()
        var coord = QtPositioning.coordinate(lat, lon)
        var pin = searchPinComp.createObject(mapTarget)
        if (pin) { pin.coordinate = coord; mapTarget.addMapItem(pin); searchResultPin = pin }
        if (animationsRef) { animationsRef.animateToCenter(coord); animationsRef.animateToZoom(15) }
    }

    function clearSearchResult() {
        if (searchResultPin) { mapTarget.removeMapItem(searchResultPin); searchResultPin.destroy(); searchResultPin = null }
    }

    function _syncNavigationPeerLayout() {
        if (navigationStartPin) {
            navigationStartPin.peerValid = navigationEndPin !== null
            navigationStartPin.peerLon = navigationEndPin ? navigationEndPin.coordinate.longitude : 0
        }
        if (navigationEndPin) {
            navigationEndPin.peerValid = navigationStartPin !== null
            navigationEndPin.peerLon = navigationStartPin ? navigationStartPin.coordinate.longitude : 0
        }
    }

    function setNavigationPath(points) {
        clearNavigationRoute()
        if (!points || points.length < 2 || !mapTarget) return
        var line = trajectoryPolylineComp.createObject(mapTarget)
        if (!line) return
        line.line.color = "#e67e22"
        line.line.width = 5
        line.opacity = 0.88
        for (var i = 0; i < points.length; i++) {
            var c = _extractCoord(points[i])
            if (c) line.addCoordinate(c)
        }
        mapTarget.addMapItem(line)
        navigationPolyline = line
        _fitViewport(points)
    }

    function clearNavigationRoute() {
        if (navigationPolyline && mapTarget) {
            mapTarget.removeMapItem(navigationPolyline)
            navigationPolyline.destroy()
            navigationPolyline = null
        }
    }

    function setNavigationStartMarker(lat, lon, name, plateNumber) {
        if (!mapTarget)
            return
        clearNavigationStartMarker()
        var pin = navEndpointPinComp.createObject(mapTarget)
        if (!pin)
            return
        pin.isStartPin = true
        pin.pinName = name || "起点"
        pin.pinPlateNumber = plateNumber ? plateNumber : ""
        pin.coordinate = QtPositioning.coordinate(lat, lon)
        mapTarget.addMapItem(pin)
        navigationStartPin = pin
        _syncNavigationPeerLayout()
    }

    function updateNavigationStartPlate(plateNumber) {
        if (!navigationStartPin)
            return
        navigationStartPin.pinPlateNumber = plateNumber ? plateNumber : ""
    }

    function setNavigationEndMarker(lat, lon, name) {
        if (!mapTarget)
            return
        clearNavigationEndMarker()
        var pin = navEndpointPinComp.createObject(mapTarget)
        if (!pin)
            return
        pin.isStartPin = false
        pin.pinName = name || "终点"
        pin.coordinate = QtPositioning.coordinate(lat, lon)
        mapTarget.addMapItem(pin)
        navigationEndPin = pin
        _syncNavigationPeerLayout()
    }

    function clearNavigationStartMarker() {
        if (navigationStartPin && mapTarget) {
            mapTarget.removeMapItem(navigationStartPin)
            navigationStartPin.destroy()
            navigationStartPin = null
        }
        _syncNavigationPeerLayout()
    }

    function clearNavigationEndMarker() {
        if (navigationEndPin && mapTarget) {
            mapTarget.removeMapItem(navigationEndPin)
            navigationEndPin.destroy()
            navigationEndPin = null
        }
        _syncNavigationPeerLayout()
    }

    function clearNavigationEndpointMarkers() {
        clearNavigationStartMarker()
        clearNavigationEndMarker()
    }

    function resetInteraction() { userHasInteracted = false; autoFitEnabled = true }

    // ── 私有辅助 ─────────────────────────────────────────────────

    function _updatePositionNow(plateNumber, coordinate, direction, speed) {
        if (!vehicleItems[plateNumber]) return
        var v = vehicleItems[plateNumber]
        if (v.coordinate && coordinate && v.coordinate.distanceTo(coordinate) < 1.0) return
        var realtime = (typeof controller !== 'undefined') && controller && controller.isPlaying === false
        if (realtime || !animationsEnabled) {
            v.coordinate = coordinate; v.direction = direction; v.speed = speed
        } else if (animationsRef) {
            animationsRef.animateVehiclePosition(v, coordinate)
            animationsRef.animateVehicleRotation(v, direction)
            v.speed = speed
        }
    }

    function _fitViewport(trajectoryPoints) {
        var geoShape = QtPositioning.path()
        for (var i = 0; i < trajectoryPoints.length; i++) {
            var c = _extractCoord(trajectoryPoints[i])
            if (c) geoShape.addCoordinate(c)
        }
        mapTarget.fitViewportToGeoShape(geoShape, Qt.size(1, 1))
    }

    function _extractCoord(point) {
        if (!point) return null
        if (point.coordinate) return point.coordinate
        if (point.latitude !== undefined && point.longitude !== undefined)
            return QtPositioning.coordinate(point.latitude, point.longitude)
        return null
    }

    function _colorForPlate(plateNumber) {
        var colors = ["#e74c3c", "#3498db", "#2ecc71", "#f39c12", "#9b59b6", "#1abc9c", "#e67e22", "#34495e"]
        var hash = 0
        for (var i = 0; i < plateNumber.length; i++)
            hash = plateNumber.charCodeAt(i) + ((hash << 5) - hash)
        return colors[Math.abs(hash) % colors.length]
    }
}
