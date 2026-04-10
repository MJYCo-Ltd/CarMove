import QtQuick
import QtLocation
import QtPositioning

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

    signal vehicleClicked(string plateNumber, double speed, int direction)

    // ── 轨迹线模板 ───────────────────────────────────────────────
    Component {
        id: trajectoryPolylineComp
        MapPolyline { line.color: "red"; line.width: 5; opacity: 0.8 }
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
