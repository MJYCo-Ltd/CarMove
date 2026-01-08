import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Controls
import CarMove 1.0

Item {
    id: mapDisplay
    
    property alias map: mapView.map
    property var vehicleItems: ({})
    property var trajectoryItems: []
    property string currentVehicle: ""
    property string currentVehicleColor: "#0061F6"
    
    // Performance optimization properties
    property int maxVehicleMarkers: 100 // Limit number of visible markers
    property bool animationsEnabled: true
    property int updateThrottleMs: 16 // ~60fps throttling
    property bool autoFitEnabled: true  // 控制是否自动调整视图
    property bool userHasInteracted: false  // 跟踪用户是否手动操作过地图
    
    // 统一的按钮尺寸控制
    property int buttonSize: 50
    
    // 地图类型切换相关属性
    property int currentMapTypeIndex: 0
    property var availableMapTypes: []
    
    MapView {
        id: mapView
        anchors.fill: parent
        
        map.plugin: Plugin {
            name: "QGroundControl"   // 使用 OpenStreetMap 插件
            PluginParameter {
                name: "TiandiTuKey"
                value: ""
            }
            PluginParameter {
                name: "multiLayer"
                value: "true"
            }

            // 直接指定图层列表（按顺序从底到顶）
            PluginParameter {
                name: "layers"
                value: "天地图卫星,天地图卫星注记"
            }
        }
        map.activeMapType: map.supportedMapTypes[0]
        map.center: QtPositioning.coordinate(39.9, 116.4) // 北京坐标
        map.zoomLevel: 12
        map.minimumZoomLevel: 3
        map.maximumZoomLevel: 18
        
        // 监听用户手动操作地图
        Connections {
            target: mapView.map
            function onCenterChanged() {
                handleUserMapInteraction("移动地图")
                // 更新内存中的地图中心位置（不立即保存）
                updateMapCenter()
            }
            function onZoomLevelChanged() {
                handleUserMapInteraction("缩放地图")
                // 更新内存中的缩放级别（不立即保存）
                updateZoomLevel()
            }
        }
        
    Component.onCompleted:{
        console.log(map.supportedMapTypes)
        // 初始化可用地图类型列表
        availableMapTypes = []
        
        for (var i = 0; i < map.supportedMapTypes.length; i++) {
            var mapType = map.supportedMapTypes[i]
            availableMapTypes.push(mapType)
            console.log("地图类型 " + i + ":", mapType.name, mapType.description)
        }
        
        // 更新地图类型选择器
        mapTypeSelector.updateMapTypes(map.supportedMapTypes)
        
        // 加载保存的地图配置
        loadMapConfiguration()
        
        logMapDisplayMessage("info", "初始化完成，共找到 " + availableMapTypes.length + " 种地图类型")
    }
    }
    
    // 地图定位按钮
    StatusButton {
        id: locationButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 20
        anchors.topMargin: 20
        
        buttonSize: mapDisplay.buttonSize
        iconText: "📍"
        buttonColor: "#3498db"
        hoverColor: "#2980b9"
        tooltipText: "定位到目标区域"
        
        onClicked: {
            mapDisplay.centerToLocation()
        }
    }
    
    // 截屏按钮
    StatusButton {
        id: screenshotButton
        anchors.right: parent.right
        anchors.top: locationButton.bottom
        anchors.rightMargin: 20
        anchors.topMargin: 10
        
        buttonSize: mapDisplay.buttonSize
        iconText: "📷"
        buttonColor: "#27ae60"
        hoverColor: "#229954"
        tooltipText: "截取地图画面"
        
        onClicked: {
            mapDisplay.takeScreenshot()
        }
    }
    
    // 地图类型选择组件
    MapTypeSelector {
        id: mapTypeSelector
        anchors.right: parent.right
        anchors.top: screenshotButton.bottom
        anchors.rightMargin: 20
        anchors.topMargin: 10
        
        // 统一使用mapDisplay的buttonSize
        buttonSize: mapDisplay.buttonSize
        expandedWidth: 180
        
        onMapTypeSelected: function(index) {
            mapDisplay.selectMapType(index)
        }
    }
    
    // 轨迹线组件
    Component {
        id: trajectoryPolyline
        MapPolyline {
            line.color: "red"
            line.width: 5
            opacity: 0.8
        }
    }
    
    // 车辆信息弹窗
    VehicleInfoCard {
        id: vehicleInfoPopup
        width: 200
        height: 120
        visible: false
        z: 1000
        
        layoutMode: "vertical"
        showBorder: true
        borderColor: "#3498db"
        isClickable: false
        
        property double speed: 0
        property int direction: 0
        
        // Close button
        Button {
            text: "关闭"
            width: 60
            height: 25
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 10
            onClicked: vehicleInfoPopup.visible = false
        }
        
        // Auto-hide timer
        Timer {
            id: hideTimer
            interval: 5000
            onTriggered: vehicleInfoPopup.visible = false
        }
    }
    
    // 通知组件
    MapNotifications {
        id: mapNotifications
        anchors.fill: parent
    }
    
    // 动画组件
    MapAnimations {
        id: mapAnimations
        mapTarget: mapView.map
        animationsEnabled: mapDisplay.animationsEnabled
    }
    
    // 卸油记录显示组件
    FuelUnloadingDisplay {
        id: fuelUnloadingDisplay
        anchors.fill: parent
        
        Component.onCompleted: {
            // 设置目标地图
            fuelUnloadingDisplay.setTargetMap(mapView.map)
        }
    }
    
    // 卸油记录相关的代理函数
    function clearFuelMarkers() {
        fuelUnloadingDisplay.clearMarkers()
    }
    
    function showVehicleFuelRecords(plateNumber) {
        fuelUnloadingDisplay.showVehicleRecords(plateNumber)
    }
    
    function showAllFuelRecords() {
        fuelUnloadingDisplay.showAllRecords()
    }
    
    // Position update throttling timer
    Timer {
        id: updateThrottleTimer
        interval: mapDisplay.updateThrottleMs
        repeat: false
        property var pendingUpdates: ({})
        
        onTriggered: {
            // Process all pending updates at once
            for (var plateNumber in pendingUpdates) {
                var update = pendingUpdates[plateNumber]
                updateVehiclePositionImmediate(plateNumber, update.coordinate, update.direction, update.speed)
            }
            pendingUpdates = {}
        }
    }
    
    // Performance monitoring
    Timer {
        id: performanceMonitor
        interval: 5000 // Check every 5 seconds
        repeat: true
        running: true
        
        onTriggered: {
            var markerCount = Object.keys(vehicleItems).length
            var trajectoryCount = trajectoryItems.length
            
            if (markerCount > mapDisplay.maxVehicleMarkers) {
                console.warn("MapDisplay: Too many vehicle markers (" + markerCount + "), consider optimization")
            }
            
            // Clean up unused markers
            cleanupUnusedMarkers()
        }
    }
    
    // 公共函数实现
    function addVehicle(plateNumber, coordinate, direction, speed, color) {
        if (!vehicleItems[plateNumber]) {
            var item = Qt.createComponent("VehicleMarker.qml").createObject(mapView.map)
            if (item) {
                item.plateNumber = plateNumber
                item.vehicleColor = color || generateVehicleColor(plateNumber)
                
                // 计算到达目标区域的天数
                if (typeof controller !== 'undefined' && controller) {
                    var targetLat = 38.365533743246445
                    var targetLon = 117.41485834121706
                    var radiusMeters = 1000
                    item.visitDays = controller.calculateVisitDays(plateNumber, targetLat, targetLon, radiusMeters)
                } else {
                    item.visitDays = 0
                }
                
                // 连接车辆点击信号
                item.vehicleClicked.connect(function(plateNumber, speed, direction) {
                    mapDisplay.showVehicleInfo(plateNumber, speed, direction)
                })
                
                vehicleItems[plateNumber] = item
                mapView.map.addMapItem(item)
            }
        }
        
        var vehicle = vehicleItems[plateNumber]
        if (vehicle) {
            vehicle.coordinate = coordinate
            vehicle.direction = direction
            vehicle.speed = speed
        }
    }
    
    function addVehicleTrajectory(plateNumber, trajectoryPoints, vehicleColor) {
        // 清除之前的轨迹
        clearTrajectory()
        
        currentVehicle = plateNumber
        
        // 添加轨迹线
        if (trajectoryPoints && trajectoryPoints.length > 1) {
            var trajectoryLine = trajectoryPolyline.createObject(mapView.map)
            if (trajectoryLine) {
                trajectoryLine.line.color = currentVehicleColor
                trajectoryLine.line.width = 3
                
                // 添加所有轨迹点
                for (var i = 0; i < trajectoryPoints.length; i++) {
                    var point = trajectoryPoints[i]
                    var coord = extractCoordinate(point)
                    if (coord) {
                        trajectoryLine.addCoordinate(coord)
                    }
                }
                
                mapView.map.addMapItem(trajectoryLine)
                trajectoryItems.push(trajectoryLine)
            }
        }
        
        // 添加车辆标记（初始位置）
        if (trajectoryPoints && trajectoryPoints.length > 0) {
            var firstPoint = trajectoryPoints[0]
            var coord = extractCoordinate(firstPoint)
            if (coord) {
                addVehicle(plateNumber, coord, firstPoint.direction || 0, firstPoint.speed || 0, currentVehicleColor)
                
                // 使用智能地图视图调整功能
                if (autoFitEnabled && !userHasInteracted) {
                    fitViewportToTrajectoryBounds(trajectoryPoints)
                }
            }
        }
    }
    
    function updateTrajectoryCoordinates(newTrajectoryPoints) {
        // 更新轨迹线坐标（用于坐标系转换后的更新）
        if (currentVehicle && newTrajectoryPoints && newTrajectoryPoints.length > 0) {
            logMapDisplayMessage("info", "更新轨迹坐标，重新启用自动调整")
            // 重置用户交互状态，因为这是新的轨迹数据
            resetUserInteraction()
            addVehicleTrajectory(currentVehicle, newTrajectoryPoints, currentVehicleColor)
        }
    }
    
    function clearTrajectory() {
        // 清除所有轨迹线
        for (var i = 0; i < trajectoryItems.length; i++) {
            mapView.map.removeMapItem(trajectoryItems[i])
        }
        trajectoryItems = []
        
        // 清除所有车辆标记
        for (var plateNumber in vehicleItems) {
            mapView.map.removeMapItem(vehicleItems[plateNumber])
        }
        vehicleItems = {}
        
        // 重置自动调整状态
        resetUserInteraction()
        logMapDisplayMessage("info", "清除轨迹，重置自动调整状态")
    }
    
    function updateVehiclePosition(plateNumber, coordinate, direction, speed) {
        // Use throttling to improve performance during rapid updates
        if (updateThrottleTimer.running) {
            // Queue the update
            updateThrottleTimer.pendingUpdates[plateNumber] = {
                coordinate: coordinate,
                direction: direction,
                speed: speed
            }
        } else {
            // Process immediately and start throttle timer
            updateVehiclePositionImmediate(plateNumber, coordinate, direction, speed)
            updateThrottleTimer.pendingUpdates = {}
            updateThrottleTimer.start()
        }
    }
    
    function updateVehiclePositionImmediate(plateNumber, coordinate, direction, speed) {
        if (vehicleItems[plateNumber]) {
            var vehicle = vehicleItems[plateNumber]
            
            // Check if position changed significantly to avoid unnecessary updates
            var lastPos = vehicle.coordinate
            if (lastPos && coordinate) {
                var distance = lastPos.distanceTo(coordinate)
                if (distance < 1.0) { // Less than 1 meter change
                    return // Skip update
                }
            }
            
            // Use immediate update during dragging for better responsiveness
            var isRealTimeUpdate = controller && controller.isPlaying === false
            
            if (isRealTimeUpdate || !mapDisplay.animationsEnabled) {
                // Immediate update without animation for dragging or when animations disabled
                vehicle.coordinate = coordinate
                vehicle.direction = direction
                vehicle.speed = speed
            } else {
                // Smooth animation for normal playback
                mapAnimations.animateVehiclePosition(vehicle, coordinate)
                mapAnimations.animateVehicleRotation(vehicle, direction)
                vehicle.speed = speed
            }
        }
    }
    
    function cleanupUnusedMarkers() {
        // Remove markers that haven't been updated recently
        var currentTime = Date.now()
        var markerKeys = Object.keys(vehicleItems)
        
        for (var i = 0; i < markerKeys.length; i++) {
            var plateNumber = markerKeys[i]
            var vehicle = vehicleItems[plateNumber]
            
            if (vehicle && vehicle.lastUpdateTime &&
                    (currentTime - vehicle.lastUpdateTime) > 300000) { // 5 minutes
                mapView.map.removeMapItem(vehicle)
                delete vehicleItems[plateNumber]
            }
        }
    }
    
    function fitViewportToTrajectoryBounds(trajectoryPoints) {
        if (!trajectoryPoints || trajectoryPoints.length === 0) {
            logMapDisplayMessage("warn", "没有轨迹点数据，无法调整视图")
            return
        }
        
        // 创建 GeoPath
        var geoShape = QtPositioning.path()
        var validPointCount = 0
        
        for (var i = 0; i < trajectoryPoints.length; i++) {
            var point = trajectoryPoints[i]
            var coord = extractCoordinate(point)
            if (coord) {
                geoShape.addCoordinate(coord)
                validPointCount++
            }
        }
        
        if (validPointCount === 0) {
            logMapDisplayMessage("warn", "没有有效的坐标点，无法调整视图")
            return
        }
        
        // 直接使用 fitViewportToGeoShape 调整视图
        mapView.map.fitViewportToGeoShape(geoShape, Qt.size(1, 1))
        logMapDisplayMessage("info", "使用 fitViewportToGeoShape 调整视图，包含 " + validPointCount + " 个轨迹点")
    }
    
    function enableAutoFit(enabled) {
        autoFitEnabled = enabled
        logMapDisplayMessage("info", "自动调整视图功能" + (enabled ? "启用" : "禁用"))
    }
    
    function resetUserInteraction() {
        userHasInteracted = false
        autoFitEnabled = true
        logMapDisplayMessage("info", "重置用户交互状态，重新启用自动调整")
    }
    
    function handleUserMapInteraction(actionType) {
        // 检查是否是用户手动操作（而不是程序设置）
        if (mapView.map.gesture && mapView.map.gesture.enabled) {
            if (!userHasInteracted) {
                logMapDisplayMessage("info", "检测到用户手动" + actionType + "，禁用自动调整")
                userHasInteracted = true
                autoFitEnabled = false
            }
        }
    }
    
    function extractCoordinate(point) {
        // 统一的坐标提取函数，处理不同的坐标数据格式
        if (!point) {
            return null
        }
        
        if (point.coordinate) {
            return point.coordinate
        } else if (point.latitude !== undefined && point.longitude !== undefined) {
            return QtPositioning.coordinate(point.latitude, point.longitude)
        }
        
        return null
    }
    
    function logMapDisplayMessage(level, message) {
        // 统一的日志输出函数，确保一致的日志格式
        var prefix = "MapDisplay: "
        switch (level) {
            case "info":
                console.log(prefix + message)
                break
            case "warn":
                console.warn(prefix + message)
                break
            case "error":
                console.error(prefix + message)
                break
            default:
                console.log(prefix + message)
        }
    }
    
    function generateVehicleColor(plateNumber) {
        // 根据车牌号生成颜色
        var colors = ["#e74c3c", "#3498db", "#2ecc71", "#f39c12", "#9b59b6", "#1abc9c", "#e67e22", "#34495e"]
        var hash = 0
        for (var i = 0; i < plateNumber.length; i++) {
            hash = plateNumber.charCodeAt(i) + ((hash << 5) - hash)
        }
        return colors[Math.abs(hash) % colors.length]
    }
    
    function centerToLocation() {
        // 定位到指定坐标：117.41485834121706, 38.365533743246445，缩放级别19
        var targetCoordinate = QtPositioning.coordinate(38.365533743246445, 117.41485834121706)
        var targetZoomLevel = 16
        
        // 使用动画平滑移动到目标位置
        mapAnimations.animateToCenter(targetCoordinate)
        mapAnimations.animateToZoom(targetZoomLevel)
        
        // 如果当前有车辆且到达天数不为0，将车辆移动到目标位置
        if (currentVehicle && currentVehicle !== "" && vehicleItems[currentVehicle]) {
            var vehicle = vehicleItems[currentVehicle]
            if (vehicle && vehicle.visitDays > 0) {
                // 使用动画将车辆移动到目标坐标
                mapAnimations.animateVehicleToLocation(vehicle, targetCoordinate)
                console.log("车辆", currentVehicle, "移动到目标位置，到达天数:", vehicle.visitDays)
            }
        }
        
        console.log("地图定位到坐标:", targetCoordinate.latitude, targetCoordinate.longitude, "缩放级别:", targetZoomLevel)
    }
    
    function takeScreenshot() {
        // 生成文件名
        var fileName = generateScreenshotFileName()
        
        // 使用grabToImage直接截取MapView组件
        mapView.grabToImage(function(result) {
            // 生成完整的文件路径
            var documentsPath = ""
            if (typeof controller !== 'undefined' && controller) {
                documentsPath = controller.getDocumentsPath()
            }
            
            var screenshotDir = documentsPath + "/CarMove_Screenshots"
            var fullPath = screenshotDir + "/" + fileName
            
            // 保存截图
            if (result.saveToFile(fullPath)) {
                console.log("地图截图成功保存到:", fullPath)
                console.log("截图尺寸:", result.image.width + "x" + result.image.height)
                
                // 显示截屏成功提示
                mapNotifications.showScreenshotNotification(fileName)
            } else {
                console.error("保存地图截图失败:", fullPath)
            }
        })
    }
    
    function generateScreenshotFileName() {
        var fileName = ""
        
        // 如果有当前显示的车辆轨迹，使用车牌号
        if (currentVehicle && currentVehicle !== "") {
            fileName = currentVehicle + "_map_screenshot"
        } else {
            // 否则使用时间戳
            var now = new Date()
            var timestamp = now.getFullYear() +
                    String(now.getMonth() + 1).padStart(2, '0') +
                    String(now.getDate()).padStart(2, '0') + "_" +
                    String(now.getHours()).padStart(2, '0') +
                    String(now.getMinutes()).padStart(2, '0') +
                    String(now.getSeconds()).padStart(2, '0')
            fileName = "map_screenshot_" + timestamp
        }
        
        return fileName + ".png"
    }
    
    function selectMapType(index) {
        if (index < 0 || index >= availableMapTypes.length) {
            logMapDisplayMessage("error", "无效的地图类型索引: " + index)
            return
        }
        
        var selectedMapType = availableMapTypes[index]
        if (selectedMapType) {
            currentMapTypeIndex = index
            mapView.map.activeMapType = selectedMapType
            mapTypeSelector.setCurrentIndex(index)
            
            var typeName = selectedMapType.name || "未知类型"
            var typeDesc = selectedMapType.description || "无描述"
            
            logMapDisplayMessage("info", "选择地图类型: " + typeName + " (" + typeDesc + ")")
            
            // 显示切换通知
            mapNotifications.showMapTypeNotification(typeName, typeDesc)
            
            // 更新内存中的地图类型配置（不立即保存）
            updateMapTypeIndex(index)
        } else {
            logMapDisplayMessage("error", "无法获取地图类型信息")
        }
    }
    
    function switchMapType() {
        // 保留这个函数以兼容可能的其他调用
        if (availableMapTypes.length <= 1) {
            logMapDisplayMessage("warn", "只有一种地图类型可用，无法切换")
            return
        }
        
        var nextIndex = (currentMapTypeIndex + 1) % availableMapTypes.length
        selectMapType(nextIndex)
    }
    
    // 信号处理
    signal showVehicleInfo(string plateNumber, double speed, int direction)
    
    onShowVehicleInfo: {
        vehicleInfoPopup.plateNumber = plateNumber
        vehicleInfoPopup.speed = speed
        vehicleInfoPopup.direction = direction
        vehicleInfoPopup.visible = true
        vehicleInfoPopup.x = mapDisplay.width / 2 - vehicleInfoPopup.width / 2
        vehicleInfoPopup.y = 20
        hideTimer.restart()
    }
    
    // 地图配置管理函数
    function loadMapConfiguration() {
        if (typeof controller !== 'undefined' && controller && controller.configManager) {
            var configManager = controller.configManager
            
            // 加载地图类型
            var savedMapTypeIndex = configManager.mapTypeIndex
            if (savedMapTypeIndex >= 0 && savedMapTypeIndex < availableMapTypes.length) {
                currentMapTypeIndex = savedMapTypeIndex
                mapView.map.activeMapType = availableMapTypes[savedMapTypeIndex]
                mapTypeSelector.setCurrentIndex(savedMapTypeIndex)
                logMapDisplayMessage("info", "加载保存的地图类型索引: " + savedMapTypeIndex)
            } else {
                // 使用默认地图类型
                currentMapTypeIndex = 0
                if (availableMapTypes.length > 0) {
                    mapView.map.activeMapType = availableMapTypes[0]
                }
            }
            
            // 加载缩放级别
            var savedZoomLevel = configManager.zoomLevel
            if (savedZoomLevel > 0) {
                mapView.map.zoomLevel = savedZoomLevel
                logMapDisplayMessage("info", "加载保存的缩放级别: " + savedZoomLevel)
            }
            
            // 加载地图中心
            var savedCenter = configManager.mapCenter
            if (savedCenter && savedCenter.isValid) {
                mapView.map.center = savedCenter
                logMapDisplayMessage("info", "加载保存的地图中心: " + savedCenter.latitude + ", " + savedCenter.longitude)
            }
            
            // 加载坐标转换设置
            var savedCoordConversion = configManager.coordinateConversionEnabled
            if (typeof controller.setCoordinateConversionEnabled === 'function') {
                controller.setCoordinateConversionEnabled(savedCoordConversion)
                logMapDisplayMessage("info", "加载保存的坐标转换设置: " + (savedCoordConversion ? "启用" : "禁用"))
            }
            
            logMapDisplayMessage("info", "地图配置加载完成")
        } else {
            logMapDisplayMessage("warn", "ConfigManager 不可用，使用默认配置")
            // 使用默认配置
            currentMapTypeIndex = 0
            if (availableMapTypes.length > 0) {
                mapView.map.activeMapType = availableMapTypes[0]
            }
        }
    }
    
    function updateMapTypeIndex(index) {
        if (typeof controller !== 'undefined' && controller && controller.configManager) {
            controller.configManager.mapTypeIndex = index
            logMapDisplayMessage("info", "更新地图类型索引: " + index)
        }
    }
    
    function updateZoomLevel() {
        if (typeof controller !== 'undefined' && controller && controller.configManager) {
            var currentZoom = mapView.map.zoomLevel
            controller.configManager.zoomLevel = currentZoom
            logMapDisplayMessage("info", "更新缩放级别: " + currentZoom)
        }
    }
    
    function updateMapCenter() {
        if (typeof controller !== 'undefined' && controller && controller.configManager) {
            var currentCenter = mapView.map.center
            controller.configManager.mapCenter = currentCenter
            logMapDisplayMessage("info", "更新地图中心: " + currentCenter.latitude + ", " + currentCenter.longitude)
        }
    }
    
    function updateCoordinateConversionState(enabled) {
        if (typeof controller !== 'undefined' && controller && controller.configManager) {
            controller.configManager.coordinateConversionEnabled = enabled
            logMapDisplayMessage("info", "更新坐标转换状态: " + (enabled ? "启用" : "禁用"))
        }
    }
    
    // 监听坐标转换状态变化
    Connections {
        target: typeof controller !== 'undefined' ? controller : null
        function onCoordinateConversionChanged() {
            if (controller && controller.configManager) {
                updateCoordinateConversionState(controller.coordinateConversionEnabled)
            }
        }
    }
}
