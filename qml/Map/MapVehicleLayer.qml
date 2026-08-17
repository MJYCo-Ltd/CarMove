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
    property int  updateThrottleMs: 16
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
    /// 轨迹视口是否纳入目标位置（批量「只截大图」时为 false）
    property bool   viewportFitIncludeTarget: true
    property var    _pendingFitShape:   null
    property int    _pendingFitAttempts: 0
    property var    searchResultPin:    null
    property var    navigationPickPins: []
    property var    navigationPolyline:  null
    property var    navigationStartPin:   null
    property var    navigationEndPin:     null
    /// 轨迹补全路线（区别于导航面板路线）
    property bool   complementRouteActive: false
    property var    complementEndCoordinate: null
    property real   complementEndDirection: 0
    property string complementArrivalTimeText: ""
    /// 由「含自定义点」特殊导航创建的车辆，切回普通导航时需清除（不影响轨迹车标）
    property bool   customPointNavVehicleActive: false

    signal vehicleClicked(string plateNumber, double speed, int direction)

    Timer {
        id: throttleTimer
        interval: vehicleLayer.updateThrottleMs; repeat: false
        property var pending: ({})
        onTriggered: {
            for (var p in pending) {
                var u = pending[p]
                vehicleLayer._updatePositionNow(p, u.coordinate, u.direction, u.speed, u.positionTimeText)
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

    function _formatTimestamp(ts) {
        if (!ts)
            return ""
        if (typeof ts === "string")
            return ts
        if (ts.getTime && ts.getTime() > 0)
            return Qt.formatDateTime(ts, "yyyy-MM-dd HH:mm:ss")
        return ""
    }

    function _applyVehicleMarkerState(vehicle, marker) {
        if (!vehicle || !marker || !marker.coordinate || !marker.coordinate.isValid)
            return
        vehicle.coordinate = marker.coordinate
        vehicle.direction = marker.direction || 0
        vehicle.speed = marker.speed || 0
        if (marker.timestampText && marker.timestampText.length > 0)
            vehicle.positionTimeText = marker.timestampText
        else
            vehicle.positionTimeText = _formatTimestamp(marker.timestamp)
    }

    function applyVehicleMarker(plateNumber, marker) {
        if (!plateNumber || !marker)
            return
        if (!vehicleItems[plateNumber]) {
            addVehicle(plateNumber,
                       marker.coordinate,
                       marker.direction || 0,
                       marker.speed || 0,
                       currentVehicleColor,
                       marker.timestampText || _formatTimestamp(marker.timestamp))
            return
        }
        _applyVehicleMarkerState(vehicleItems[plateNumber], marker)
    }

    function finalizeVehicleLabelsForCapture() {
        for (var p in vehicleItems) {
            var v = vehicleItems[p]
            if (v && v.updateViewportLayout)
                v.updateViewportLayout()
        }
    }

    function addVehicle(plateNumber, coordinate, direction, speed, color, positionTimeText) {
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
        if (v) {
            v.coordinate = coordinate
            v.direction = direction
            v.speed = speed
            if (positionTimeText !== undefined)
                v.positionTimeText = positionTimeText
        }
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
                       currentVehicleColor,
                       startMarker.timestampText || _formatTimestamp(startMarker.timestamp))
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
        // 切换车辆/重载轨迹时一并清掉补全路线，避免残留
        clearNavigationRoute()
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
            if (!v)
                continue
            // 补全路线生效时，当前车到场次数固定为 1，不被轨迹统计覆盖
            if (complementRouteActive && q === currentVehicle)
                v.visitDays = 1
            else
                v.visitDays = counts[q]
        }
    }

    function updateVehiclePosition(plateNumber, coordinate, direction, speed, positionTimestamp) {
        var timeText = _formatTimestamp(positionTimestamp)
        if (throttleTimer.running) {
            throttleTimer.pending[plateNumber] = {
                coordinate: coordinate,
                direction: direction,
                speed: speed,
                positionTimeText: timeText
            }
        } else {
            _updatePositionNow(plateNumber, coordinate, direction, speed, timeText)
            throttleTimer.pending = {}
            throttleTimer.start()
        }
    }

    /// adjustView：默认 true（搜索定位会挪视口）；地图选点传 false
    function showSearchResult(lat, lon, adjustView) {
        clearSearchResult()
        var coord = QtPositioning.coordinate(lat, lon)
        var pin = _createPlacemark({ placemarkKind: "searchPin" })
        if (pin) { pin.coordinate = coord; mapTarget.addMapItem(pin); searchResultPin = pin }
        if (adjustView === false)
            return
        if (animationsRef) { animationsRef.animateToCenter(coord); animationsRef.animateToZoom(15) }
    }

    function clearSearchResult() {
        if (searchResultPin) { mapTarget.removeMapItem(searchResultPin); searchResultPin.destroy(); searchResultPin = null }
    }

    /// 导航地图选点：落 📍，不调整视口；可叠加多个
    function addNavigationPickMarker(lat, lon) {
        if (!mapTarget)
            return
        var coord = QtPositioning.coordinate(lat, lon)
        if (!coord || !coord.isValid)
            return
        var pin = _createPlacemark({ placemarkKind: "searchPin" })
        if (!pin)
            return
        pin.coordinate = coord
        mapTarget.addMapItem(pin)
        var pins = navigationPickPins.slice()
        pins.push(pin)
        navigationPickPins = pins
    }

    function clearNavigationPickMarkers() {
        for (var i = 0; i < navigationPickPins.length; i++) {
            var pin = navigationPickPins[i]
            if (pin) {
                if (mapTarget)
                    mapTarget.removeMapItem(pin)
                pin.destroy()
            }
        }
        navigationPickPins = []
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

    function setNavigationPath(points, lineColor, lineWidth, opacity, fitToRoute) {
        clearNavigationRoute()
        clearNavigationPickMarkers()
        if (!points || points.length < 2 || !mapTarget)
            return
        var color = (lineColor !== undefined && lineColor !== null && String(lineColor).length > 0)
                    ? lineColor : "#e67e22"
        var width = (lineWidth !== undefined && lineWidth !== null) ? lineWidth : 5
        var op = (opacity !== undefined && opacity !== null) ? opacity : 0.88
        var line = _createMapLineOnMap(points, color, width, op)
        if (!line)
            return
        mapTarget.addMapItem(line)
        navigationPolyline = line
        // fitToRoute 默认 true；补全路线会改为 false，改用整段车辆轨迹视口
        if (fitToRoute === false)
            return
        if (typeof controller !== 'undefined' && controller) {
            var navShape = controller.geoPathForViewport(points)
            if (navShape)
                _doFitViewportShape(navShape)
        }
    }

    function _coordinateFromPathPoint(point) {
        if (!point)
            return null
        var lat = point.latitude !== undefined ? point.latitude : point.lat
        var lon = point.longitude !== undefined ? point.longitude : point.lon
        if (lat === undefined || lon === undefined)
            return null
        var c = QtPositioning.coordinate(lat, lon)
        return (c && c.isValid) ? c : null
    }

    /// 补全到目标区域：绘制导航线并记住终点 / 到场时间，供「定位目标区域」使用
    function setComplementRoute(points, lineColor, lineWidth, opacity, arrivalTimeText) {
        setNavigationPath(points, lineColor, lineWidth, opacity, false)
        if (!navigationPolyline || !points || points.length < 2)
            return

        var endCoord = _coordinateFromPathPoint(points[points.length - 1])
        if (!endCoord)
            return

        complementRouteActive = true
        complementEndCoordinate = endCoord
        complementArrivalTimeText = arrivalTimeText ? String(arrivalTimeText) : ""
        complementEndDirection = 0
        if (points.length >= 2) {
            var from = _coordinateFromPathPoint(points[points.length - 2])
            if (from)
                complementEndDirection = from.azimuthTo(endCoord)
        }

        if (currentVehicle && vehicleItems[currentVehicle])
            vehicleItems[currentVehicle].visitDays = 1

        // 有车辆轨迹时按整段轨迹适配；否则按补全/导航折线+目标区域适配
        if (typeof controller !== 'undefined' && controller
                && controller.activeTrajectoryPointCount() > 0) {
            scheduleTrajectoryDisplayViewportFit()
        } else if (typeof controller !== 'undefined' && controller) {
            var shape = controller.geoPathForViewport(points)
            if (shape)
                _doFitViewportShape(shape)
        }
    }

    /// 含自定义点的特殊导航：蓝线+车辆；定位停靠在 vehicleParkIndex（导航终点，不含自定义点）
    function showCustomPointNavigationRoute(points, plateNumber, startTimeText, endTimeText, vehicleParkIndex) {
        clearNavigationEndpointMarkers()
        clearNavigationPickMarkers()
        customPointNavVehicleActive = false
        _clearVehicleMarkersOnly()

        var plate = plateNumber ? String(plateNumber).trim() : ""
        if (!plate)
            plate = "导航"
        var color = "#3498db"

        currentVehicle = plate
        currentVehicleColor = color
        setComplementRoute(points, color, 3, 0.9, endTimeText || "")

        // 定位停靠：导航选择的终点（天地图路线末点），不含后续自定义点
        var parkIdx = points.length - 1
        if (vehicleParkIndex !== undefined && vehicleParkIndex !== null
                && vehicleParkIndex >= 0 && vehicleParkIndex < points.length)
            parkIdx = vehicleParkIndex
        var parkCoord = _coordinateFromPathPoint(points[parkIdx])
        if (parkCoord && parkCoord.isValid) {
            complementEndCoordinate = parkCoord
            complementEndDirection = 0
            if (parkIdx >= 1) {
                var fromPark = _coordinateFromPathPoint(points[parkIdx - 1])
                if (fromPark)
                    complementEndDirection = fromPark.azimuthTo(parkCoord)
            }
        }

        var startCoord = _coordinateFromPathPoint(points[0])
        if (!startCoord || !startCoord.isValid)
            return

        var startDir = 0
        if (points.length >= 2) {
            var nextCoord = _coordinateFromPathPoint(points[1])
            if (nextCoord)
                startDir = startCoord.azimuthTo(nextCoord)
        }

        addVehicle(plate, startCoord, startDir, 0, color, startTimeText || "")
        if (vehicleItems[plate])
            vehicleItems[plate].visitDays = 1
        customPointNavVehicleActive = true
    }

    function _clearVehicleMarkersOnly() {
        for (var p in vehicleItems) {
            var item = vehicleItems[p]
            if (item) {
                item.layoutMapView = null
                if (mapTarget)
                    mapTarget.removeMapItem(item)
            }
        }
        vehicleItems = {}
        currentVehicle = ""
    }

    function updateComplementArrivalTime(arrivalTimeText, applyToVehicle) {
        if (!complementRouteActive)
            return
        complementArrivalTimeText = arrivalTimeText ? String(arrivalTimeText) : ""
        if (applyToVehicle === false)
            return
        if (currentVehicle && vehicleItems[currentVehicle] && complementArrivalTimeText.length > 0)
            vehicleItems[currentVehicle].positionTimeText = complementArrivalTimeText
    }

    /// 将当前车辆放到补全路线终点，时间用到场时间
    function placeVehicleAtComplementEnd() {
        if (!complementRouteActive || !complementEndCoordinate || !complementEndCoordinate.isValid)
            return false
        if (!currentVehicle || !vehicleItems[currentVehicle])
            return false

        var v = vehicleItems[currentVehicle]
        v.coordinate = complementEndCoordinate
        v.direction = complementEndDirection
        v.speed = 0
        if (complementArrivalTimeText.length > 0)
            v.positionTimeText = complementArrivalTimeText
        v.visitDays = 1
        return true
    }

    function clearNavigationRoute() {
        if (navigationPolyline && mapTarget) {
            mapTarget.removeMapItem(navigationPolyline)
            navigationPolyline.destroy()
            navigationPolyline = null
        }
        complementRouteActive = false
        complementEndCoordinate = null
        complementEndDirection = 0
        complementArrivalTimeText = ""
        // 仅清除「含自定义点」特殊导航创建的车辆，不影响轨迹模式下的车标
        if (customPointNavVehicleActive) {
            customPointNavVehicleActive = false
            _clearVehicleMarkersOnly()
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

    function _updatePositionNow(plateNumber, coordinate, direction, speed, positionTimeText) {
        if (!vehicleItems[plateNumber]) return
        var v = vehicleItems[plateNumber]
        if (v.coordinate && coordinate && typeof controller !== 'undefined' && controller
            && controller.isVehicleMoveBelowDistanceThreshold(v.coordinate, coordinate, 1.0))
            return
        v.coordinate = coordinate
        v.direction = direction
        v.speed = speed
        if (positionTimeText !== undefined)
            v.positionTimeText = positionTimeText
    }

    function scheduleTrajectoryDisplayViewportFit() {
        if (typeof controller === 'undefined' || !controller || !mapTarget)
            return
        _pendingFitShape = controller.trajectoryDisplayViewportShape(viewportFitIncludeTarget)
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

    function _markViewportPending() {
        if (parent && parent.markViewportPending)
            parent.markViewportPending()
    }

    function _doFitViewportShape(shape) {
        if (!shape || !mapTarget)
            return
        _markViewportPending()
        suppressInteractionTracking = true
        mapTarget.fitViewportToGeoShape(shape, Qt.size(fitViewportMargin, fitViewportMargin))
        Qt.callLater(function() {
            suppressInteractionTracking = false
            for (var p in vehicleItems) {
                var v = vehicleItems[p]
                if (v && v.scheduleViewportLayout)
                    v.scheduleViewportLayout()
            }
        })
    }

    function fitTrajectoryDisplayViewportNow() {
        if (typeof controller === 'undefined' || !controller || !mapTarget)
            return
        pendingFitTimer.stop()
        _pendingFitShape = null
        _pendingFitAttempts = 0
        var shape = controller.trajectoryDisplayViewportShape(viewportFitIncludeTarget)
        if (shape)
            _doFitViewportShape(shape)
    }

    function fitTargetAreaCaptureViewportNow() {
        if (typeof controller === 'undefined' || !controller || !mapTarget)
            return
        pendingFitTimer.stop()
        _pendingFitShape = null
        _pendingFitAttempts = 0

        var shape = controller.targetAreaCaptureViewportShape()
        if (shape)
            _doFitViewportShape(shape)

        var coord = controller.targetAreaMapCoordinate()
        if (currentVehicle && coord && coord.isValid) {
            var marker = controller.trajectoryDisplayNearestMarker(coord.latitude, coord.longitude)
            if (marker && marker.coordinate && marker.coordinate.isValid)
                applyVehicleMarker(currentVehicle, marker)
        }
    }

    Connections {
        target: typeof controller !== 'undefined' ? controller : null
        function onTargetAreaChanged() {
            if (controller)
                vehicleLayer.recalculateTargetAreaVisitCounts(controller.targetAreaLatitude, controller.targetAreaLongitude)
        }
    }
}
