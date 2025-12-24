import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Controls

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
    
    MapView {
        id: mapView
        anchors.fill: parent
        
        map.plugin: Plugin {
            name: "QGroundControl"   // 使用 OpenStreetMap 插件
            PluginParameter {
                name: "TiandiTuKey"
                value: "cbc71550f33685acbd0bff46a661e63d"
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
                // 检查是否是用户手动操作（而不是程序设置）
                if (mapView.map.gesture && mapView.map.gesture.enabled) {
                    if (!userHasInteracted) {
                        console.log("MapDisplay: 检测到用户手动移动地图，禁用自动调整")
                        userHasInteracted = true
                        autoFitEnabled = false
                    }
                }
            }
            function onZoomLevelChanged() {
                // 检查是否是用户手动操作（而不是程序设置）
                if (mapView.map.gesture && mapView.map.gesture.enabled) {
                    if (!userHasInteracted) {
                        console.log("MapDisplay: 检测到用户手动缩放地图，禁用自动调整")
                        userHasInteracted = true
                        autoFitEnabled = false
                    }
                }
            }
        }
        
        Component.onCompleted:{
            console.log(map.supportedMapTypes)
        }
        
        // 车辆图标组件
        Component {
            id: vehicleMarker
            MapQuickItem {
                id: marker
                property string plateNumber: ""
                property int direction: 0
                property double speed: 0
                property string vehicleColor: "yellow"
                property int visitDays: 0  // 新增：到达目标区域的天数
                
                coordinate: QtPositioning.coordinate(0, 0)
                anchorPoint.x: vehicleIcon.width / 2
                anchorPoint.y: vehicleIcon.height / 2
                
                sourceItem: Item {
                    width: 40
                    height: 50
                    
                    Rectangle {
                        id: vehicleIcon
                        width: 24
                        height: 24
                        color: marker.vehicleColor
                        radius: 12
                        anchors.centerIn: parent
                        anchors.verticalCenterOffset: -8
                        rotation: marker.direction
                        border.color: "white"
                        border.width: 2
                        
                        // 车头指示箭头
                        Rectangle {
                            width: 8
                            height: 2
                            color: "white"
                            anchors.centerIn: parent
                            anchors.verticalCenterOffset: -6
                            radius: 1
                        }
                        
                        // 速度指示器
                        Rectangle {
                            width: 4
                            height: 4
                            color: marker.speed > 0 ? "#27ae60" : "#e74c3c"
                            radius: 2
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.rightMargin: -2
                            anchors.topMargin: -2
                        }
                    }
                    
                    // 到达天数标识（右上角）
                    Rectangle {
                        id: visitDaysIndicator
                        width: visitDaysText.width + 6
                        height: visitDaysText.height + 4
                        color: "#e74c3c"
                        border.color: "white"
                        border.width: 1
                        radius: 8
                        visible: marker.visitDays > 0
                        anchors.right: vehicleIcon.right
                        anchors.top: vehicleIcon.top
                        anchors.rightMargin: -8
                        anchors.topMargin: -8
                        z: 10
                        
                        Text {
                            id: visitDaysText
                            text: marker.visitDays.toString()
                            font.pixelSize: 10
                            font.bold: true
                            color: "white"
                            anchors.centerIn: parent
                        }
                    }
                    
                    // 车牌号标签
                    Rectangle {
                        width: plateText.width + 8
                        height: plateText.height + 4
                        color: "yellow"  // 使用车辆颜色作为背景色
                        border.color: "white"       // 白色边框以确保可见性
                        border.width: 1
                        radius: 3
                        anchors.top: vehicleIcon.bottom
                        anchors.horizontalCenter: vehicleIcon.horizontalCenter
                        anchors.topMargin: 2
                        
                        Text {
                            id: plateText
                            text: marker.plateNumber
                            font.pixelSize: 11
                            color: "black"  // 白色文字以确保在彩色背景上的可读性
                            style: Text.Outline
                            styleColor: "white"

                            anchors.centerIn: parent
                        }
                    }
                }
                
                // 点击事件
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        mapDisplay.showVehicleInfo(marker.plateNumber, marker.speed, marker.direction)
                    }
                    
                    // 悬停效果
                    hoverEnabled: true
                    onEntered: {
                        vehicleIcon.scale = 1.2
                    }
                    onExited: {
                        vehicleIcon.scale = 1.0
                    }
                }
            }
        }
    }
    
    // 地图定位按钮
    Rectangle {
        id: locationButton
        width: 50
        height: 50
        color: "#3498db"
        radius: 25
        border.color: "white"
        border.width: 2
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 20
        anchors.topMargin: 20
        z: 1000
        
        // 定位图标
        Text {
            text: "📍"
            font.pixelSize: 24
            color: "white"
            anchors.centerIn: parent
        }
        
        // 鼠标交互
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            
            onClicked: {
                mapDisplay.centerToLocation()
            }
            
            onEntered: {
                parent.color = "#2980b9"
                parent.scale = 1.1
            }
            
            onExited: {
                parent.color = "#3498db"
                parent.scale = 1.0
            }
        }
        
        // 悬停动画
        Behavior on color {
            ColorAnimation { duration: 200 }
        }
        
        Behavior on scale {
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
        }
    }
    
    // 截屏按钮
    Rectangle {
        id: screenshotButton
        width: 50
        height: 50
        color: "#27ae60"
        radius: 25
        border.color: "white"
        border.width: 2
        anchors.right: parent.right
        anchors.top: locationButton.bottom
        anchors.rightMargin: 20
        anchors.topMargin: 10
        z: 1000
        
        // 截屏图标
        Text {
            text: "📷"
            font.pixelSize: 24
            color: "white"
            anchors.centerIn: parent
        }
        
        // 鼠标交互
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            
            onClicked: {
                mapDisplay.takeScreenshot()
            }
            
            onEntered: {
                parent.color = "#229954"
                parent.scale = 1.1
            }
            
            onExited: {
                parent.color = "#27ae60"
                parent.scale = 1.0
            }
        }
        
        // 悬停动画
        Behavior on color {
            ColorAnimation { duration: 200 }
        }
        
        Behavior on scale {
            NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
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
    Rectangle {
        id: vehicleInfoPopup
        width: 200
        height: 120
        color: "white"
        border.color: "#3498db"
        border.width: 2
        radius: 8
        visible: false
        z: 1000
        
        property string plateNumber: ""
        property double speed: 0
        property int direction: 0
        
        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            
            Text {
                text: "车辆信息"
                font.bold: true
                font.pixelSize: 14
                color: "#2c3e50"
            }
            
            Text {
                text: "车牌号: " + vehicleInfoPopup.plateNumber
                font.pixelSize: 12
                color: "#34495e"
            }
            
            Text {
                text: "速度: " + vehicleInfoPopup.speed.toFixed(1) + " km/h"
                font.pixelSize: 12
                color: "#34495e"
            }
            
            Text {
                text: "方向: " + vehicleInfoPopup.direction + "°"
                font.pixelSize: 12
                color: "#34495e"
            }
            
            Button {
                text: "关闭"
                width: 60
                height: 25
                onClicked: vehicleInfoPopup.visible = false
            }
        }
        
        // 自动隐藏定时器
        Timer {
            id: hideTimer
            interval: 5000
            onTriggered: vehicleInfoPopup.visible = false
        }
    }
    
    // 截屏成功通知
    Rectangle {
        id: screenshotNotification
        width: 300
        height: 80
        color: "#2ecc71"
        border.color: "white"
        border.width: 2
        radius: 8
        visible: false
        z: 1001
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 20
        
        property string fileName: ""
        
        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            
            Text {
                text: "截屏成功！"
                font.bold: true
                font.pixelSize: 14
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            Text {
                text: "已保存为: " + screenshotNotification.fileName
                font.pixelSize: 12
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
                wrapMode: Text.WordWrap
                width: parent.width - 20
            }
        }
        
        // 自动隐藏定时器
        Timer {
            id: screenshotNotificationTimer
            interval: 3000
            onTriggered: screenshotNotification.visible = false
        }
        
        // 点击关闭
        MouseArea {
            anchors.fill: parent
            onClicked: screenshotNotification.visible = false
        }
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
            var item = vehicleMarker.createObject(mapView.map)
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
        // currentVehicleColor = vehicleColor || generateVehicleColor(plateNumber)
        
        // 添加轨迹线
        if (trajectoryPoints && trajectoryPoints.length > 1) {
            var trajectoryLine = trajectoryPolyline.createObject(mapView.map)
            if (trajectoryLine) {
                trajectoryLine.line.color = currentVehicleColor
                trajectoryLine.line.width = 3
                
                // 添加所有轨迹点
                for (var i = 0; i < trajectoryPoints.length; i++) {
                    var point = trajectoryPoints[i]
                    if (point.coordinate) {
                        trajectoryLine.addCoordinate(point.coordinate)
                    } else if (point.latitude !== undefined && point.longitude !== undefined) {
                        trajectoryLine.addCoordinate(QtPositioning.coordinate(point.latitude, point.longitude))
                    }
                }
                
                mapView.map.addMapItem(trajectoryLine)
                trajectoryItems.push(trajectoryLine)
            }
        }
        
        // 添加车辆标记（初始位置）
        if (trajectoryPoints && trajectoryPoints.length > 0) {
            var firstPoint = trajectoryPoints[0]
            var coord = firstPoint.coordinate || QtPositioning.coordinate(firstPoint.latitude, firstPoint.longitude)
            addVehicle(plateNumber, coord, firstPoint.direction || 0, firstPoint.speed || 0, currentVehicleColor)
            
            // 使用智能地图视图调整功能
            if (autoFitEnabled && !userHasInteracted) {
                fitViewportToTrajectoryBounds(trajectoryPoints)
            }
        }
    }
    
    function updateTrajectoryCoordinates(newTrajectoryPoints) {
        // 更新轨迹线坐标（用于坐标系转换后的更新）
        if (currentVehicle && newTrajectoryPoints && newTrajectoryPoints.length > 0) {
            console.log("MapDisplay: 更新轨迹坐标，重新启用自动调整")
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
        console.log("MapDisplay: 清除轨迹，重置自动调整状态")
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
                if (positionAnimation.target !== vehicle) {
                    positionAnimation.target = vehicle
                }
                positionAnimation.to = coordinate
                positionAnimation.start()
                
                if (rotationAnimation.target !== vehicle) {
                    rotationAnimation.target = vehicle
                }
                rotationAnimation.to = direction
                rotationAnimation.start()
                
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
    
    // Position and rotation animations (moved here for better organization)
    PropertyAnimation {
        id: positionAnimation
        property: "coordinate"
        duration: mapDisplay.animationsEnabled ? 1000 : 0
        easing.type: Easing.InOutQuad
    }
    
    RotationAnimation {
        id: rotationAnimation
        property: "direction"
        duration: mapDisplay.animationsEnabled ? 500 : 0
        direction: RotationAnimation.Shortest
    }
    
    function fitViewportToTrajectoryBounds(trajectoryPoints) {
        if (!trajectoryPoints) {
            console.log("MapDisplay: 没有轨迹点，跳过视图调整")
            return
        }
        
        // 检查是否已经是 QtPositioning.path
        var isPath = typeof trajectoryPoints === 'object' && trajectoryPoints.addCoordinate !== undefined;
        var pointsArray;
        var geoShape;
        
        if (isPath) {
            geoShape = trajectoryPoints;
            // 从 path 提取坐标数组用于计算边界
            pointsArray = [];
            for (var i = 0; i < geoShape.path.length; i++) {
                pointsArray.push({coordinate: geoShape.path[i]});
            }
        } else {
            // 假设是坐标数组
            if (trajectoryPoints.length === 0) {
                console.log("MapDisplay: 没有轨迹点，跳过视图调整")
                return
            }
            pointsArray = trajectoryPoints;
            // 创建 path
            geoShape = QtPositioning.path();
            for (var i = 0; i < trajectoryPoints.length; i++) {
                var point = trajectoryPoints[i];
                var coord = point.coordinate || QtPositioning.coordinate(point.latitude, point.longitude);
                geoShape.addCoordinate(coord);
            }
        }
        
        // 计算最小包围矩形
        var boundingRect = calculateTrajectoryBounds(pointsArray)
        
        if (boundingRect.isValid) {

            // 使用Qt Location的标准方法调整视图
            try {
                mapView.map.fitViewportToGeoShape(geoShape,
                                                  Qt.size(1, 1))
                console.log("MapDisplay: 成功调整地图视图到轨迹范围")
            } catch (error) {
                console.error("MapDisplay: 调整地图视图失败:", error)
                // 回退到手动计算的方法
                fallbackFitViewport(boundingRect.bounds)
            }
        } else {
            console.warn("MapDisplay: 无法计算有效的包围矩形")
        }
    }
    
    function calculateTrajectoryBounds(trajectoryPoints) {
        if (!trajectoryPoints || trajectoryPoints.length === 0) {
            return { isValid: false }
        }
        
        var minLat = 90, maxLat = -90, minLon = 180, maxLon = -180
        var validPointCount = 0
        
        // 遍历所有轨迹点找到边界
        for (var i = 0; i < trajectoryPoints.length; i++) {
            var point = trajectoryPoints[i]
            var lat = point.coordinate ? point.coordinate.latitude : point.latitude
            var lon = point.coordinate ? point.coordinate.longitude : point.longitude
            
            // 验证坐标有效性
            if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
                if (lat < minLat) minLat = lat
                if (lat > maxLat) maxLat = lat
                if (lon < minLon) minLon = lon
                if (lon > maxLon) maxLon = lon
                validPointCount++
            }
        }
        
        if (validPointCount === 0) {
            console.warn("MapDisplay: 没有找到有效的坐标点")
            return { isValid: false }
        }
        
        // 如果只有一个点，创建一个小的区域
        if (validPointCount === 1) {
            var margin = 0.01  // 约1公里的边距
            minLat -= margin
            maxLat += margin
            minLon -= margin
            maxLon += margin
        } else {
            // 添加适当的边距（10%，但至少0.001度）
            var latRange = maxLat - minLat
            var lonRange = maxLon - minLon
            var latMargin = Math.max(latRange * 0.1, 0.001)
            var lonMargin = Math.max(lonRange * 0.1, 0.001)
            
            minLat -= latMargin
            maxLat += latMargin
            minLon -= lonMargin
            maxLon += lonMargin
        }
        
        // 确保坐标在有效范围内
        minLat = Math.max(minLat, -90)
        maxLat = Math.min(maxLat, 90)
        minLon = Math.max(minLon, -180)
        maxLon = Math.min(maxLon, 180)
        
        // 创建地理矩形
        
        return {
            isValid: true,
            bounds: {
                minLat: minLat,
                maxLat: maxLat,
                minLon: minLon,
                maxLon: maxLon
            }
        }
    }
    
    function fallbackFitViewport(bounds) {
        // 回退方法：手动设置中心点和缩放级别
        console.log("MapDisplay: 使用回退方法调整视图")
        
        var center = QtPositioning.coordinate(
                    (bounds.minLat + bounds.maxLat) / 2,
                    (bounds.minLon + bounds.maxLon) / 2
                    )
        mapView.map.center = center
        
        // 计算合适的缩放级别
        var latDiff = bounds.maxLat - bounds.minLat
        var lonDiff = bounds.maxLon - bounds.minLon
        var maxDiff = Math.max(latDiff, lonDiff)
        
        var zoomLevel = 15
        if (maxDiff > 0.1) zoomLevel = 10
        else if (maxDiff > 0.05) zoomLevel = 12
        else if (maxDiff > 0.01) zoomLevel = 14
        else if (maxDiff > 0.005) zoomLevel = 15
        else zoomLevel = 16
        
        mapView.map.zoomLevel = zoomLevel
        console.log("MapDisplay: 设置中心点为", center.latitude.toFixed(6), center.longitude.toFixed(6), "缩放级别", zoomLevel)
    }
    
    function enableAutoFit(enabled) {
        autoFitEnabled = enabled
        console.log("MapDisplay: 自动调整视图功能", enabled ? "启用" : "禁用")
    }
    
    function resetUserInteraction() {
        userHasInteracted = false
        autoFitEnabled = true
        console.log("MapDisplay: 重置用户交互状态，重新启用自动调整")
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
        centerAnimation.to = targetCoordinate
        zoomAnimation.to = targetZoomLevel
        
        centerAnimation.start()
        zoomAnimation.start()
        
        // 如果当前有车辆且到达天数不为0，将车辆移动到目标位置
        if (currentVehicle && currentVehicle !== "" && vehicleItems[currentVehicle]) {
            var vehicle = vehicleItems[currentVehicle]
            if (vehicle && vehicle.visitDays > 0) {
                // 使用动画将车辆移动到目标坐标
                vehicleLocationAnimation.target = vehicle
                vehicleLocationAnimation.to = targetCoordinate
                vehicleLocationAnimation.start()
                
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
                showScreenshotNotification(fileName)
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
    
    function showScreenshotNotification(fileName) {
        screenshotNotification.fileName = fileName
        screenshotNotification.visible = true
        screenshotNotificationTimer.restart()
    }
    
    // 地图中心点动画
    PropertyAnimation {
        id: centerAnimation
        target: mapView.map
        property: "center"
        duration: 1500
        easing.type: Easing.InOutQuad
    }
    
    // 地图缩放动画
    PropertyAnimation {
        id: zoomAnimation
        target: mapView.map
        property: "zoomLevel"
        duration: 1500
        easing.type: Easing.InOutQuad
    }
    
    // 车辆位置移动动画
    PropertyAnimation {
        id: vehicleLocationAnimation
        property: "coordinate"
        duration: 2000
        easing.type: Easing.InOutQuad
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
}
