import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Controls
import CarMove 1.0

Item {
    id: mapDisplay

    property alias map: mapView.map
    property int buttonSize: 50
    property int currentMapTypeIndex: 0
    property var availableMapTypes: []
    property double targetLat: 38.97887422901859
    property double targetLon: 117.73397758792544

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
            function onCenterChanged()    { updateMapCenter(); vehicleLayer.userHasInteracted = true; vehicleLayer.autoFitEnabled = false }
            function onZoomLevelChanged() { updateZoomLevel(); vehicleLayer.userHasInteracted = true; vehicleLayer.autoFitEnabled = false }
        }

        Component.onCompleted: {
            availableMapTypes = []
            for (var i = 0; i < map.supportedMapTypes.length; i++)
                availableMapTypes.push(map.supportedMapTypes[i])
            mapTypeSelector.updateMapTypes(map.supportedMapTypes)
            loadMapConfiguration()
        }
    }

    // 定位按钮
    StatusButton {
        id: locationButton
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 20; anchors.topMargin: 20
        buttonSize: mapDisplay.buttonSize
        iconText: "📍"; buttonColor: "#3498db"; hoverColor: "#2980b9"
        tooltipText: "定位到目标区域"
        onClicked: centerToLocation()
    }

    // 截屏按钮
    StatusButton {
        id: screenshotButton
        anchors.right: parent.right; anchors.top: locationButton.bottom
        anchors.rightMargin: 20; anchors.topMargin: 10
        buttonSize: mapDisplay.buttonSize
        iconText: "📷"; buttonColor: "#27ae60"; hoverColor: "#229954"
        tooltipText: "截取地图画面"
        onClicked: takeScreenshot()
    }

    // 地图类型选择器
    MapTypeSelector {
        id: mapTypeSelector
        anchors.right: parent.right; anchors.top: screenshotButton.bottom
        anchors.rightMargin: 20; anchors.topMargin: 10
        buttonSize: mapDisplay.buttonSize; expandedWidth: 180
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

    // 辅助叠加组件
    MapNotifications  { id: mapNotifications; anchors.fill: parent }
    MapAnimations     { id: mapAnimations; mapTarget: mapView.map }
    FuelUnloadingDisplay {
        id: fuelUnloadingDisplay; anchors.fill: parent
        Component.onCompleted: fuelUnloadingDisplay.setTargetMap(mapView.map)
    }

    // 车辆层（管理所有车辆/轨迹/搜索图钉）
    MapVehicleLayer {
        id: vehicleLayer
        mapTarget:     mapView.map
        animationsRef: mapAnimations
        targetLat:     mapDisplay.targetLat
        targetLon:     mapDisplay.targetLon
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
    function addVehicleTrajectory(pn, pts, color)        { vehicleLayer.addVehicleTrajectory(pn, pts, color) }
    function updateTrajectoryCoordinates(pts)            { vehicleLayer.updateTrajectoryCoordinates(pts) }
    function clearTrajectory()                           { vehicleLayer.clearTrajectory() }
    function updateVehiclePosition(pn, coord, dir, spd) { vehicleLayer.updateVehiclePosition(pn, coord, dir, spd) }
    function showSearchResult(lat, lon, name)            { vehicleLayer.showSearchResult(lat, lon, name) }
    function clearSearchResult()                         { vehicleLayer.clearSearchResult() }
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

    // 卸油记录 代理函数
    function clearFuelMarkers()         { fuelUnloadingDisplay.clearMarkers() }
    function showVehicleFuelRecords(pn) { fuelUnloadingDisplay.showVehicleRecords(pn) }
    function showAllFuelRecords()       { fuelUnloadingDisplay.showAllRecords() }

    // 定位 & 截图
    function centerToLocation() {
        var coord = QtPositioning.coordinate(targetLat, targetLon)
        mapAnimations.animateToCenter(coord)
        mapAnimations.animateToZoom(18)
    }

    function takeScreenshot() {
        var name = vehicleLayer.currentVehicle
                   ? vehicleLayer.currentVehicle + "_map_screenshot.png"
                   : "map_screenshot_" + Qt.formatDateTime(new Date(), "yyyyMMdd_HHmmss") + ".png"
        mapView.grabToImage(function(result) {
            var dir = (controller ? controller.getDocumentsPath() : "") + "/CarMove_Screenshots"
            if (result.saveToFile(dir + "/" + name)) {
                console.log("截图保存成功:", dir + "/" + name)
                mapNotifications.showScreenshotNotification(name)
            } else {
                console.error("截图保存失败:", dir + "/" + name)
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
        if (cfg.zoomLevel > 0)                    mapView.map.zoomLevel = cfg.zoomLevel
        if (cfg.mapCenter && cfg.mapCenter.isValid) mapView.map.center = cfg.mapCenter
        if (controller.setCoordinateConversionEnabled)
            controller.setCoordinateConversionEnabled(cfg.coordinateConversionEnabled)
    }

    function updateMapTypeIndex(index) {
        if (controller && controller.configManager) controller.configManager.mapTypeIndex = index
    }
    function updateZoomLevel() {
        if (controller && controller.configManager) controller.configManager.zoomLevel = mapView.map.zoomLevel
    }
    function updateMapCenter() {
        if (controller && controller.configManager) controller.configManager.mapCenter = mapView.map.center
    }

    Connections {
        target: typeof controller !== 'undefined' ? controller : null
        function onCoordinateConversionChanged() {
            if (controller && controller.configManager)
                controller.configManager.coordinateConversionEnabled = controller.coordinateConversionEnabled
        }
    }
}
