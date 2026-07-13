import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Controls
import QGroundControl 1.0

Item {
    id: mapDisplay

    /// 已选车辆且车牌非空：与右侧「坐标切换」等地图工具条显隐一致（供时间轴栏等绑定）
    readonly property bool mapVehicleContextActive: controller && controller.selectedVehicle
                                                     && controller.selectedVehicle.length > 0

    property alias map: mapView.map

    /// 地图右键菜单：更新目标区域所用坐标（WGS/GCJ 与当前地图一致）
    property real contextMenuLat: 0
    property real contextMenuLon: 0
    property int buttonSize: 50
    property int currentMapTypeIndex: 0
    property var availableMapTypes: []

    /// geocoder 调用意图：rightClick=右键通知 POI；fillTargetName=名称空时补全目标区域名
    property string geocoderIntent: ""
    property string lastReversePoi: ""
    property string lastReverseFormatted: ""

    /// 轨迹模式下允许双击地图目标区域编辑名称
    property bool trajectoryModeActive: false
    /// 批量截图进行中：隐藏地图工具按钮，避免进入截图
    property bool batchScreenshotActive: false
    property int captureFitMargin: 120
    property bool _suppressMapInteractionFlag: false
    property var _targetPlacemarkComponent: null
    property bool _targetPlacemarkComponentHooked: false

    property var pendingCapture: null
    /// fit / 定位后由 onTilesReady 置 true，用于批量截图等待瓦片
    property bool tilesReady: false

    signal batchCaptureFinished(bool success, string captureLabel)

    function markTilesPending() {
        tilesReady = false
        _batchPerfLog("tiles.pending", "")
    }

    function cancelTileWait() {
        tileWaitTimer.stop()
        pendingCapture = null
    }

    function _beginBatchCaptureViewport(kind, filePath, captureLabel, plateNumber, vehicleColor) {
        cancelTileWait()
        const label = captureLabel || kind || ""
        pendingCapture = {
            filePath: filePath,
            label: label,
            stage: "waitTiles"
        }
        _batchPerfLog("capture.begin", label)
        if (kind === "trajectory")
            prepareCaptureTrajectory(plateNumber, vehicleColor || "#3498db")
        else if (kind === "targetArea")
            centerToLocation(true, 18, true)
        _batchPerfLog("capture.viewport.ready", label)
        tileWaitTimer.interval = captureTileTimeoutMs
        tileWaitTimer.restart()
        _batchPerfLog("tiles.wait", label + " await tilesReady=" + tilesReady)
        tryFinishPendingCapture(false)
    }

    function beginBatchCaptureTrajectory(plateNumber, vehicleColor, filePath, captureLabel) {
        _beginBatchCaptureViewport("trajectory", filePath, captureLabel || "trajectory", plateNumber, vehicleColor)
    }

    function beginBatchCaptureTargetArea(filePath, captureLabel) {
        _beginBatchCaptureViewport("targetArea", filePath, captureLabel || "targetArea", "", "")
    }

    function tryFinishPendingCapture(viaTimeout) {
        if (!pendingCapture || pendingCapture.stage !== "waitTiles")
            return
        if (!viaTimeout && !tilesReady)
            return

        tileWaitTimer.stop()
        _batchPerfLog("tiles.ready", pendingCapture.label + (viaTimeout ? " timeout" : " ready"))
        _batchPerfLog("capture.tiles.ready", pendingCapture.label)
        pendingCapture.stage = "grab"
        _executePendingGrab()
    }

    function _executePendingGrab() {
        if (!pendingCapture || pendingCapture.stage !== "grab")
            return

        const pending = pendingCapture
        _batchPerfLog("capture.grab.begin", pending.label)
        vehicleLayer.finalizeVehicleLabelsForCapture()

        mapView.grabToImage(function(result) {
            var success = result && result.saveToFile(pending.filePath)
            mapDisplay._finishPendingGrab(success)
        })
    }

    function _finishPendingGrab(success) {
        if (!pendingCapture)
            return
        const captureLabel = pendingCapture.label
        _batchPerfLog("capture.grab.done", captureLabel + " ok=" + success)
        pendingCapture = null
        batchCaptureFinished(success, captureLabel)
    }

    /// 批量截图：轨迹绘制与视口计算均在 C++ 完成
    function prepareCaptureTrajectory(plateNumber, vehicleColor) {
        vehicleLayer.userHasInteracted = false
        vehicleLayer.autoFitEnabled = true
        vehicleLayer.fitViewportMargin = captureFitMargin
        clearTrajectory()
        vehicleLayer.showVehicleTrajectory(plateNumber, vehicleColor || "#3498db", "now")
        syncTargetAreaMapMarkers()
        refreshTargetAreaMarkerLayout()
    }

    function showSelectedVehicleTrajectory(plateNumber, vehicleColor) {
        syncTargetAreaMapMarkers()
        vehicleLayer.showVehicleTrajectory(plateNumber, vehicleColor || "#3498db", "auto")
        Qt.callLater(refreshTargetAreaMarkerLayout)
        targetAreaRelayoutTimer.restart()
    }

    function refreshSelectedVehicleTrajectory() {
        vehicleLayer.refreshVehicleTrajectory()
        Qt.callLater(refreshTargetAreaMarkerLayout)
        targetAreaRelayoutTimer.restart()
    }

    function resetCaptureViewportMargin() {
        vehicleLayer.fitViewportMargin = 80
    }

    Timer {
        id: tileWaitTimer
        repeat: false
        onTriggered: {
            const label = pendingCapture ? pendingCapture.label : ""
            _batchPerfLog("tiles.timeout", label + " tilesReady=" + tilesReady)
            tryFinishPendingCapture(true)
        }
    }

    readonly property int captureTileTimeoutMs: 5000

    property real _batchPerfLastMs: 0

    function _batchPerfLog(step, detail) {
        if (!batchScreenshotActive)
            return
        const now = Date.now()
        const stepMs = _batchPerfLastMs > 0 ? (now - _batchPerfLastMs) : 0
        const detailText = (detail !== undefined && detail !== null && detail !== "") ? detail : ""
        console.log("[BatchShot][Map]",
                    Qt.formatDateTime(new Date(now), "hh:mm:ss.zzz"),
                    step,
                    detailText,
                    "| step +" + stepMs + "ms")
        _batchPerfLastMs = now
    }

    function scheduleMaybeFillTargetAreaName() {
        if (!controller)
            return
        var n = controller.targetAreaName
        if (n && n.length > 0)
            return
        var cfg = controller.configManager
        if (!cfg || !cfg.tiandituKey || cfg.tiandituKey.length === 0)
            return
        if (typeof geocoder !== "undefined" && geocoder && geocoder.busy)
            return
        targetNameFillDebounce.restart()
    }

    function openTargetNameEditor() {
        if (!trajectoryModeActive || !controller)
            return
        targetNameEditDialog.initialName = controller.targetAreaName ? controller.targetAreaName : ""
        targetNameEditDialog.open()
    }

    function isNearTargetAreaScreenPoint(x, y) {
        if (!controller || !mapView.map)
            return false
        var targetCoord = controller.targetAreaMapCoordinate()
        if (!targetCoord.isValid)
            return false
        var targetPoint = mapView.map.fromCoordinate(targetCoord)
        if (!targetPoint)
            return false
        var dx = x - targetPoint.x
        var dy = y - targetPoint.y
        return (dx * dx + dy * dy) <= (64 * 64)
    }

    function tryFillTargetAreaNameFromReverseGeocode() {
        if (!controller)
            return
        if (controller.targetAreaName && controller.targetAreaName.length > 0)
            return
        var cfg = controller.configManager
        if (!cfg || !cfg.tiandituKey || cfg.tiandituKey.length === 0)
            return
        if (typeof geocoder === "undefined" || !geocoder || geocoder.busy)
            return
        mapDisplay.geocoderIntent = "fillTargetName"
        geocoder.reverseGeocode(controller.targetAreaLongitude, controller.targetAreaLatitude)
    }

    Timer {
        id: targetNameFillDebounce
        interval: 80
        repeat: false
        onTriggered: mapDisplay.tryFillTargetAreaNameFromReverseGeocode()
    }

    /// 目标区域：统一 MapPlacemark（图钉 + 名称，视口自适应）
    property var targetAreaMapItem: null

    Timer {
        id: targetAreaRelayoutTimer
        interval: 300
        repeat: false
        onTriggered: mapDisplay.refreshTargetAreaMarkerLayout()
    }

    Timer {
        id: suppressInteractionResetTimer
        interval: 1600
        repeat: false
        onTriggered: {
            mapDisplay._suppressMapInteractionFlag = false
            vehicleLayer.suppressInteractionTracking = false
        }
    }

    function refreshTargetAreaMarkerLayout() {
        syncTargetAreaMapMarkers()
        if (targetAreaMapItem)
            targetAreaMapItem.scheduleViewportLayout()
    }

    function _createTargetAreaMapItem() {
        if (targetAreaMapItem || !mapView || !mapView.map)
            return
        var comp = _targetPlacemarkComponent
        if (!comp || comp.status !== Component.Ready)
            return
        targetAreaMapItem = comp.createObject(null, {
            placemarkKind: "target",
            layoutMapView: mapView
        })
        if (targetAreaMapItem) {
            mapView.map.addMapItem(targetAreaMapItem)
            targetAreaMapItem.nameDoubleClicked.connect(mapDisplay.openTargetNameEditor)
            targetAreaMapItem.text = Qt.binding(function() {
                if (!controller)
                    return qsTr("目标区域")
                var name = controller.targetAreaName
                return (name && name.length > 0) ? name : qsTr("目标区域")
            })
        }
    }

    function ensureTargetAreaMapMarkers() {
        if (!mapView || !mapView.map)
            return
        if (targetAreaMapItem)
            return
        if (!_targetPlacemarkComponent)
            _targetPlacemarkComponent = Qt.createComponent("qrc:/MapPlacemark.qml")
        var comp = _targetPlacemarkComponent
        if (comp.status === Component.Loading) {
            if (!_targetPlacemarkComponentHooked) {
                _targetPlacemarkComponentHooked = true
                comp.statusChanged.connect(function() {
                    if (comp.status === Component.Ready) {
                        mapDisplay._createTargetAreaMapItem()
                        mapDisplay.syncTargetAreaMapMarkers()
                    } else if (comp.status === Component.Error) {
                        console.warn("MapPlacemark:", comp.errorString())
                    }
                })
            }
            return
        }
        if (comp.status !== Component.Ready) {
            if (comp.status === Component.Error)
                console.warn("MapPlacemark:", comp.errorString())
            return
        }
        _createTargetAreaMapItem()
    }

    function syncTargetAreaMapMarkers() {
        ensureTargetAreaMapMarkers()
        if (!controller)
            return
        var c = controller.targetAreaMapCoordinate()
        if (targetAreaMapItem && c && c.isValid)
            targetAreaMapItem.coordinate = c
    }

    // 地图视图
    MapView {
        id: mapView
        anchors.fill: parent

        map.plugin: Plugin {
            name: "QGroundControl"
            PluginParameter { name: "TiandiTuKey"; value: (controller && controller.configManager) ? controller.configManager.tiandituKey : "" }
            PluginParameter { name: "multiLayer"; value: "true" }
            PluginParameter { name: "layers"; value: "天地图街道,天地图街道注记" }
        }
        map.activeMapType: map.supportedMapTypes[0]
        map.center: QtPositioning.coordinate(39.9, 116.4)
        map.zoomLevel: 12
        map.minimumZoomLevel: 3
        map.maximumZoomLevel: 18

        Connections {
            target: mapView.map
            function onCenterChanged() {
                if (!_suppressMapInteractionFlag && !vehicleLayer.suppressInteractionTracking) {
                    vehicleLayer.userHasInteracted = true
                    vehicleLayer.autoFitEnabled = false
                }
            }
            function onZoomLevelChanged() {
                updateZoomLevel()
                if (!_suppressMapInteractionFlag && !vehicleLayer.suppressInteractionTracking) {
                    vehicleLayer.userHasInteracted = true
                    vehicleLayer.autoFitEnabled = false
                }
            }
        }

        Component.onCompleted: {
            availableMapTypes = []
            for (var i = 0; i < map.supportedMapTypes.length; i++)
                availableMapTypes.push(map.supportedMapTypes[i])
            mapTypeSelector.updateMapTypes(map.supportedMapTypes)
            loadMapConfiguration()
            mapDisplay.syncTargetAreaMapMarkers()
        }

        // 右键：上下文菜单「更新目标区域」+ 天地图逆地理（通知仅 POI）
        // 左键双击：轨迹模式下在目标区域附近编辑目标名称
        MouseArea {
            id: mapRightClickArea
            anchors.fill: parent
            acceptedButtons: Qt.RightButton | Qt.LeftButton
            onClicked: function (mouse) {
                if (mouse.button !== Qt.RightButton)
                    return
                if (!mapView.map)
                    return
                var coord = mapView.map.toCoordinate(Qt.point(mouse.x, mouse.y))
                if (!coord || !coord.isValid)
                    return
                mapDisplay.contextMenuLat = coord.latitude
                mapDisplay.contextMenuLon = coord.longitude
                // 子对象 id 不能写成 mapDisplay.mapContextMenu（在 QML 里会为 undefined）
                mapContextMenu.popup(mapRightClickArea, mouse.x, mouse.y)

                var cfg = controller && controller.configManager ? controller.configManager : null
                if (!cfg || !cfg.tiandituKey || cfg.tiandituKey.length === 0)
                    return
                if (typeof geocoder === "undefined" || !geocoder || geocoder.busy)
                    return
                mapDisplay.geocoderIntent = "rightClick"
                geocoder.reverseGeocode(coord.longitude, coord.latitude)
            }
            onDoubleClicked: function (mouse) {
                if (!mapDisplay.trajectoryModeActive)
                    return
                if (!mapDisplay.isNearTargetAreaScreenPoint(mouse.x, mouse.y))
                    return
                mapDisplay.openTargetNameEditor()
            }
        }
    }

    MapTileMonitor {
        id: tileMonitor
        map: mapView.map
        onTilesReady: {
            mapDisplay.tilesReady = true
            _batchPerfLog("tiles.signal", mapDisplay.pendingCapture ? mapDisplay.pendingCapture.label : "")
            mapDisplay.tryFinishPendingCapture(false)
        }
    }

    Menu {
        id: mapContextMenu
        parent: mapView
        MenuItem {
            text: qsTr("更新目标区域")
            onTriggered: {
                if (!controller)
                    return
                var label = ""
                if (mapDisplay.lastReversePoi && mapDisplay.lastReversePoi.length > 0)
                    label = mapDisplay.lastReversePoi
                else if (mapDisplay.lastReverseFormatted && mapDisplay.lastReverseFormatted.length > 0)
                    label = mapDisplay.lastReverseFormatted
                controller.setTargetAreaCenter(mapDisplay.contextMenuLat, mapDisplay.contextMenuLon, label)
            }
        }
    }

    // 定位按钮（目标区域中心由 controller.targetAreaLatitude/Longitude 提供）
    StatusButton {
        id: locationButton
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 20; anchors.topMargin: 20
        buttonSize: mapDisplay.buttonSize
        iconText: "📍"; buttonColor: "#3498db"; hoverColor: "#2980b9"
        tooltipText: "定位到目标区域"
        visible: !mapDisplay.batchScreenshotActive
        onClicked: centerToLocation()
    }

    // 坐标系切换（逻辑在 C++，完成后经 trajectoryConverted / coordinateConversionChanged 刷新界面）
    StatusButton {
        id: coordinateMapButton
        anchors.right: parent.right; anchors.top: locationButton.bottom
        anchors.rightMargin: 20; anchors.topMargin: 10
        buttonSize: mapDisplay.buttonSize
        iconText: "⇄"
        buttonColor: (controller && controller.coordinateConversionEnabled) ? "#e67e22" : "#16a085"
        hoverColor: (controller && controller.coordinateConversionEnabled) ? "#d35400" : "#138d75"
        tooltipText: controller && controller.coordinateConversionEnabled
                     ? "当前：火星坐标(GCJ02)\n点击切换为 GPS(WGS84)"
                     : "当前：GPS(WGS84)\n点击切换为火星坐标(GCJ02)"
        visible: mapDisplay.mapVehicleContextActive && !mapDisplay.batchScreenshotActive
        onClicked: {
            if (controller)
                controller.toggleCoordinateConversion()
        }
    }

    // 截屏按钮
    StatusButton {
        id: screenshotButton
        anchors.right: parent.right; anchors.top: coordinateMapButton.bottom
        anchors.rightMargin: 20; anchors.topMargin: 10
        buttonSize: mapDisplay.buttonSize
        iconText: "📷"; buttonColor: "#27ae60"; hoverColor: "#229954"
        tooltipText: "截取地图画面"
        visible: !mapDisplay.batchScreenshotActive
        onClicked: takeScreenshot()
    }

    // 地图类型选择器
    MapTypeSelector {
        id: mapTypeSelector
        anchors.right: parent.right; anchors.top: screenshotButton.bottom
        anchors.rightMargin: 20; anchors.topMargin: 10
        buttonSize: mapDisplay.buttonSize; expandedWidth: 180
        visible: !mapDisplay.batchScreenshotActive
        onMapTypeSelected: function(index) { selectMapType(index) }
    }

    // 车辆信息弹窗
    VehicleInfoCard {
        id: vehicleInfoPopup
        width: 200; height: 120
        visible: false; z: 1000
        layoutMode: "vertical"; showBorder: true; borderColor: "#3498db"; isClickable: false
        Button {
            text: "关闭"; width: 60; height: 25
            anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 10
            onClicked: vehicleInfoPopup.visible = false
        }
        Timer { id: hideTimer; interval: 5000; onTriggered: vehicleInfoPopup.visible = false }
    }

    MapNotifications  { id: mapNotifications; anchors.fill: parent }

    TargetNameEditDialog {
        id: targetNameEditDialog
        parent: Overlay.overlay
        onNameConfirmed: function(name) {
            if (!controller)
                return
            controller.targetAreaName = name
            mapDisplay.syncTargetAreaMapMarkers()
        }
    }
    MapAnimations     { id: mapAnimations; mapTarget: mapView.map }

    // 车辆层（管理所有车辆/轨迹/搜索图钉）
    MapVehicleLayer {
        id: vehicleLayer
        mapTarget:     mapView.map
        layoutMapView: mapView
        animationsRef: mapAnimations
        targetLat:     controller ? controller.targetAreaLatitude : 0
        targetLon:     controller ? controller.targetAreaLongitude : 0
        onVehicleClicked: function(pn, spd, dir) {
            vehicleInfoPopup.plateNumber = pn
            vehicleInfoPopup.visible = true
            vehicleInfoPopup.x = mapDisplay.width / 2 - vehicleInfoPopup.width / 2
            vehicleInfoPopup.y = 20
            hideTimer.restart()
        }
    }

    // 车辆/轨迹 代理函数
    function addVehicle(pn, coord, dir, spd, color)     { vehicleLayer.addVehicle(pn, coord, dir, spd, color) }
    function clearTrajectory()                           { vehicleLayer.clearTrajectory() }
    function updateVehiclePosition(pn, coord, dir, spd, ts) {
        vehicleLayer.updateVehiclePosition(pn, coord, dir, spd, ts)
    }
    function showSearchResult(lat, lon)                 { vehicleLayer.showSearchResult(lat, lon) }
    function clearSearchResult()                         { vehicleLayer.clearSearchResult() }
    function snapVehicleToNearestTrajectoryPoint(lat, lon) {
        if (!mapVehicleContextActive || !controller)
            return false
        return controller.seekVehicleToNearestTrajectoryPoint(lat, lon)
    }
    function locateToPlace(lat, lon) {
        showSearchResult(lat, lon)
        snapVehicleToNearestTrajectoryPoint(lat, lon)
    }
    /// 搜索面板「设为目标区域」：写入 controller 属性；地图图钉/地名由 MapDisplay.syncTargetAreaMapMarkers 同步
    function setTargetAreaFromSearch(lat, lon, name) {
        if (controller)
            controller.setTargetAreaCenter(lat, lon, name ? name : "")
    }
    function showNavigationRoute(points)                 { vehicleLayer.setNavigationPath(points) }
    function clearNavigationRoute()                      { vehicleLayer.clearNavigationRoute() }
    function setNavigationStartMarker(lat, lon, name, plateNumber) {
        vehicleLayer.setNavigationStartMarker(lat, lon, name, plateNumber)
    }
    function updateNavigationStartPlate(plateNumber)   { vehicleLayer.updateNavigationStartPlate(plateNumber) }
    function setNavigationEndMarker(lat, lon, name)     { vehicleLayer.setNavigationEndMarker(lat, lon, name) }
    function clearNavigationStartMarker()               { vehicleLayer.clearNavigationStartMarker() }
    function clearNavigationEndMarker()                 { vehicleLayer.clearNavigationEndMarker() }
    function clearNavigationEndpointMarkers()           { vehicleLayer.clearNavigationEndpointMarkers() }

    // 定位 & 截图
    function focusTargetArea(zoomLevel, instant) {
        if (!controller)
            return
        markTilesPending()
        syncTargetAreaMapMarkers()
        var coord = controller.targetAreaMapCoordinate()
        if (!coord || !coord.isValid)
            return
        var zoom = (zoomLevel !== undefined) ? zoomLevel : 14
        _suppressMapInteractionFlag = true
        vehicleLayer.suppressInteractionTracking = true
        suppressInteractionResetTimer.restart()
        if (instant) {
            mapAnimations.jumpToView(coord, zoom)
            refreshTargetAreaMarkerLayout()
        } else {
            mapAnimations.animateToCenter(coord)
            mapAnimations.animateToZoom(zoom)
            Qt.callLater(refreshTargetAreaMarkerLayout)
            targetAreaRelayoutTimer.restart()
        }
        vehicleLayer.resetInteraction()
    }

    function centerToLocation(instant, zoomLevel, forCapture) {
        if (forCapture)
            vehicleLayer.autoFitEnabled = false
        var zoom = (zoomLevel !== undefined) ? zoomLevel : 18
        focusTargetArea(zoom, instant === true)
        if (!controller)
            return
        var coord = controller.targetAreaMapCoordinate()
        if (coord && coord.isValid) {
            if (forCapture && vehicleLayer.currentVehicle) {
                var marker = controller.trajectoryDisplayNearestMarker(coord.latitude, coord.longitude)
                if (marker && marker.coordinate && marker.coordinate.isValid)
                    vehicleLayer.applyVehicleMarker(vehicleLayer.currentVehicle, marker)
                else
                    snapVehicleToNearestTrajectoryPoint(coord.latitude, coord.longitude)
            } else {
                snapVehicleToNearestTrajectoryPoint(coord.latitude, coord.longitude)
            }
        }
        if (forCapture)
            vehicleLayer.autoFitEnabled = false
    }

    function takeScreenshot() {
        captureScreenshotTo("")
    }

    /// 手动截图（非批量）；grabToImage 为 Qt 异步 API，结果在函数内处理
    function captureScreenshotTo(filePath) {
        vehicleLayer.finalizeVehicleLabelsForCapture()
        var targetPath = filePath
        if (!targetPath || targetPath.length === 0) {
            var name = vehicleLayer.currentVehicle
                       ? vehicleLayer.currentVehicle + "_map_screenshot.png"
                       : "map_screenshot_" + Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss") + ".png"
            targetPath = (controller ? controller.getDocumentsPath() : "") + "/CarMove_Screenshots/" + name
        }

        mapView.grabToImage(function(result) {
            var ok = result && result.saveToFile(targetPath)
            if (ok && (!filePath || filePath.length === 0)) {
                var fileName = targetPath.substring(targetPath.lastIndexOf("/") + 1)
                mapNotifications.showScreenshotNotification(fileName)
            }
        })
    }

    // 地图类型选择
    function selectMapType(index) {
        if (index < 0 || index >= availableMapTypes.length) return
        currentMapTypeIndex = index
        mapView.map.activeMapType = availableMapTypes[index]
        mapTypeSelector.setCurrentIndex(index)
        var t = availableMapTypes[index]
        mapNotifications.showMapTypeNotification(t.name || "", t.description || "")
        updateMapTypeIndex(index)
    }

    // 地图配置
    function loadMapConfiguration() {
        var cfg = (controller && controller.configManager) ? controller.configManager : null
        if (!cfg) {
            currentMapTypeIndex = 0
            if (availableMapTypes.length > 0) mapView.map.activeMapType = availableMapTypes[0]
            return
        }
        var idx = cfg.mapTypeIndex
        currentMapTypeIndex = (idx >= 0 && idx < availableMapTypes.length) ? idx : 0
        mapView.map.activeMapType = availableMapTypes[currentMapTypeIndex]
        mapTypeSelector.setCurrentIndex(currentMapTypeIndex)
        _suppressMapInteractionFlag = true
        if (cfg.zoomLevel > 0)
            mapView.map.zoomLevel = cfg.zoomLevel
        var targetCoord = controller ? controller.targetAreaMapCoordinate() : null
        if (targetCoord && targetCoord.isValid)
            mapView.map.center = targetCoord
        _suppressMapInteractionFlag = false
        if (controller.setCoordinateConversionEnabled)
            controller.setCoordinateConversionEnabled(cfg.coordinateConversionEnabled)
    }

    function updateMapTypeIndex(index) {
        if (controller && controller.configManager) controller.configManager.mapTypeIndex = index
    }
    function updateZoomLevel() {
        if (controller && controller.configManager) controller.configManager.zoomLevel = mapView.map.zoomLevel
    }

    Connections {
        target: typeof controller !== 'undefined' ? controller : null
        function onCoordinateConversionChanged() {
            if (controller && controller.configManager)
                controller.configManager.coordinateConversionEnabled = controller.coordinateConversionEnabled
            mapDisplay.syncTargetAreaMapMarkers()
            mapDisplay.refreshTargetAreaMarkerLayout()
        }
        function onTargetAreaChanged() {
            mapDisplay.syncTargetAreaMapMarkers()
            mapDisplay.scheduleMaybeFillTargetAreaName()
        }
        function onSelectedVehicleChanged() {
            mapDisplay.syncTargetAreaMapMarkers()
        }
    }

    Connections {
        target: (typeof geocoder !== "undefined") ? geocoder : null
        function onReverseGeocodeSucceeded(poi, formattedAddress, latitude, longitude) {
            var poiS = poi ? poi : ""
            var fmt = formattedAddress ? formattedAddress : ""
            mapDisplay.lastReversePoi = poiS
            mapDisplay.lastReverseFormatted = fmt
            var intent = mapDisplay.geocoderIntent
            mapDisplay.geocoderIntent = ""
            if (intent === "fillTargetName") {
                var label = poiS.length > 0 ? poiS : fmt
                if (controller) {
                    if (label.length > 0)
                        controller.targetAreaName = label
                    else
                        controller.targetAreaName = qsTr("（地点名未获取）")
                }
                return
            }
            if (intent === "rightClick")
                mapNotifications.showReverseGeocodePoiNotification(poiS)
        }
        function onReverseGeocodeFailed(message) {
            var intent = mapDisplay.geocoderIntent
            mapDisplay.geocoderIntent = ""
            if (intent === "fillTargetName") {
                if (controller)
                    controller.targetAreaName = qsTr("（地点名未获取）")
                return
            }
            mapNotifications.showErrorNotification(message)
        }
    }
}
