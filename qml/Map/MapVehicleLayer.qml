import QtQuick
import QtLocation
import QtPositioning

// 注：导航起终点名称相对位置按经度与对端比较（国内图幅下大致对应左右），名称用描边提高可读性

// 车辆/轨迹/搜索图钉 统一管理层，需挂载在 MapDisplay 内部
Item {
    id: vehicleLayer

    property var mapTarget          // 地图对象 (mapView.map)
    property var layoutMapView      // MapView，用于视口自适应布局
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
    property bool   suppressInteractionTracking: false
    property int    fitViewportMargin:  80
    property var    _pendingFitShape:   null
    property int    _pendingFitAttempts: 0
    property var    searchResultPin:    null
    property var    navigationPolyline:  null
    property var    navigationStartPin:   null
    property var    navigationEndPin:     null

    signal vehicleClicked(string plateNumber, double speed, int direction)

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

    Timer {
        id: pendingFitTimer
        interval: 50
        repeat: true
        onTriggered: vehicleLayer._retryPendingFit()
    }

    function _createPlacemark(initialProps) {
        var props = initialProps || {}
        props.layoutMapView = vehicleLayer.layoutMapView
        var comp = Qt.createComponent("qrc:/MapPlacemark.qml")
        if (comp.status !== Component.Ready) {
            if (comp.status === Component.Error)
                console.warn("MapPlacemark:", comp.errorString())
            return null
        }
        return comp.createObject(null, props)
    }

    function addVehicle(plateNumber, coordinate, direction, speed, color) {
        if (!vehicleItems[plateNumber]) {
            var item = _createPlacemark({
                placemarkKind: "vehicle",
                plateNumber: plateNumber,
                vehicleColor: color || ((typeof controller !== 'undefined' && controller)
                    ? controller.colorHexForPlate(plateNumber) : "#3498db")
            })
            if (item) {
                if (typeof controller !== 'undefined' && controller)
                    item.visitDays = controller.calculateTargetAreaVisitCount(plateNumber, targetLat, targetLon, 1000)
                item.vehicleClicked.connect(function(pn, spd, dir) { vehicleLayer.vehicleClicked(pn, spd, dir) })
                vehicleItems[plateNumber] = item
                mapTarget.addMapItem(item)
            }
        }
        var v = vehicleItems[plateNumber]
        if (v) { v.coordinate = coordinate; v.direction = direction; v.speed = speed }
    }

    function _mountTrajectoryPolylines(sourcePaths) {
        if (!sourcePaths || !mapTarget)
            return
        for (var i = 0; i < sourcePaths.length; ++i) {
            var pathCoords = sourcePaths[i]
            if (!pathCoords || pathCoords.length < 2)
                continue
            var line = _createMapLineFromPath(pathCoords, currentVehicleColor, 3)
            if (line) {
                mapTarget.addMapItem(line)
                trajectoryItems.push(line)
            }
        }
    }

    /// fitMode: "auto" 交互模式自适应；"now" 批量截图立即 fit；"none" 不 fit
    function showVehicleTrajectory(plateNumber, vehicleColor, fitMode) {
        clearTrajectory()
        currentVehicle = plateNumber
        if (vehicleColor)
            currentVehicleColor = vehicleColor
        if (typeof controller === 'undefined' || !controller || !mapTarget)
            return

        _mountTrajectoryPolylines(controller.trajectoryDisplayPolylinePaths())

        var startMarker = controller.trajectoryDisplayStartMarker()
        if (startMarker && startMarker.coordinate && startMarker.coordinate.isValid) {
            addVehicle(plateNumber,
                       startMarker.coordinate,
                       startMarker.direction || 0,
                       startMarker.speed || 0,
                       currentVehicleColor)
            if (fitMode === "auto" && autoFitEnabled)
                scheduleTrajectoryDisplayViewportFit()
            else if (fitMode === "now")
                fitTrajectoryDisplayViewportNow()
        }
    }

    function refreshVehicleTrajectory() {
        if (!currentVehicle)
            return
        resetInteraction()
        showVehicleTrajectory(currentVehicle, currentVehicleColor, autoFitEnabled ? "auto" : "none")
    }

    function clearTrajectory() {
        for (var i = 0; i < trajectoryItems.length; i++)
            mapTarget.removeMapItem(trajectoryItems[i])
        trajectoryItems = []
        for (var p in vehicleItems) {
            var item = vehicleItems[p]
            if (item) {
                item.layoutMapView = null
                mapTarget.removeMapItem(item)
            }
        }
        vehicleItems = {}
        resetInteraction()
    }

    function recalculateTargetAreaVisitCounts(lat, lon) {
        if (typeof controller === 'undefined' || !controller)
            return
        var plates = []
        for (var p in vehicleItems)
            plates.push(p)
        var counts = controller.batchTargetAreaVisitCounts(plates, lat, lon, 1000)
        for (var q in vehicleItems) {
            var v = vehicleItems[q]
            if (v)
                v.visitDays = counts[q]
        }
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

    function showSearchResult(lat, lon) {
        clearSearchResult()
        var coord = QtPositioning.coordinate(lat, lon)
        var pin = _createPlacemark({ placemarkKind: "searchPin" })
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
        var line = _createMapLineOnMap(points, "#e67e22", 5, 0.88)
        if (!line)
            return
        mapTarget.addMapItem(line)
        navigationPolyline = line
        if (typeof controller !== 'undefined' && controller) {
            var navShape = controller.geoPathForViewport(points)
            if (navShape)
                _doFitViewportShape(navShape)
        }
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
        var pin = _createPlacemark({ placemarkKind: "navEndpoint", isStartPin: true })
        if (!pin)
            return
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
        var pin = _createPlacemark({ placemarkKind: "navEndpoint", isStartPin: false })
        if (!pin)
            return
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

    function _createMapLineFromPath(pathCoords, lineColor, lineWidth, opacity) {
        var comp = Qt.createComponent("qrc:/MapLine.qml")
        if (comp.status !== Component.Ready) {
            if (comp.status === Component.Error)
                console.warn("MapLine:", comp.errorString())
            return null
        }
        var props = {
            lineColor: lineColor,
            lineWidth: lineWidth,
            pathCoordinates: pathCoords
        }
        if (opacity !== undefined)
            props.opacity = opacity
        return comp.createObject(null, props)
    }

    function _createMapLineOnMap(points, lineColor, lineWidth, opacity) {
        if (typeof controller !== 'undefined' && controller) {
            var pathCoords = controller.trajectoryPolylinePath(points)
            if (!pathCoords || pathCoords.length < 2)
                return null
            return _createMapLineFromPath(pathCoords, lineColor, lineWidth, opacity)
        }
        return null
    }

    function _updatePositionNow(plateNumber, coordinate, direction, speed) {
        if (!vehicleItems[plateNumber]) return
        var v = vehicleItems[plateNumber]
        if (v.coordinate && coordinate && typeof controller !== 'undefined' && controller
            && controller.isVehicleMoveBelowDistanceThreshold(v.coordinate, coordinate, 1.0))
            return
        v.coordinate = coordinate
        v.direction = direction
        v.speed = speed
    }

    function scheduleTrajectoryDisplayViewportFit() {
        if (typeof controller === 'undefined' || !controller || !mapTarget)
            return
        _pendingFitShape = controller.trajectoryDisplayViewportShape()
        _pendingFitAttempts = 0
        _retryPendingFit()
    }

    function _retryPendingFit() {
        if (!_pendingFitShape)
            return
        if (layoutMapView && layoutMapView.width > 10 && layoutMapView.height > 10) {
            _doFitViewportShape(_pendingFitShape)
            _pendingFitShape = null
            _pendingFitAttempts = 0
            pendingFitTimer.stop()
            return
        }
        _pendingFitAttempts++
        if (_pendingFitAttempts === 1)
            pendingFitTimer.start()
        else if (_pendingFitAttempts > 40) {
            _doFitViewportShape(_pendingFitShape)
            _pendingFitShape = null
            _pendingFitAttempts = 0
            pendingFitTimer.stop()
        }
    }

    function _doFitViewportShape(shape) {
        if (!shape || !mapTarget)
            return
        suppressInteractionTracking = true
        mapTarget.fitViewportToGeoShape(shape, Qt.size(fitViewportMargin, fitViewportMargin))
        Qt.callLater(function() { suppressInteractionTracking = false })
    }

    function fitTrajectoryDisplayViewportNow() {
        if (typeof controller === 'undefined' || !controller || !mapTarget)
            return
        pendingFitTimer.stop()
        _pendingFitShape = null
        _pendingFitAttempts = 0
        var shape = controller.trajectoryDisplayViewportShape()
        if (shape)
            _doFitViewportShape(shape)
    }

    Connections {
        target: typeof controller !== 'undefined' ? controller : null
        function onTargetAreaChanged() {
            if (controller)
                vehicleLayer.recalculateTargetAreaVisitCounts(controller.targetAreaLatitude, controller.targetAreaLongitude)
        }
    }
}
