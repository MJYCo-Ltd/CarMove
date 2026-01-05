import QtQuick
import QtLocation
import QtPositioning
import QtQuick.Controls
import CarMove 1.0

Item {
    id: fuelUnloadingDisplay
    
    property var markerItems: []
    property bool offsetCorrectionEnabled: false
    
    // 外部地图引用（需要从外部传入）
    property var targetMap: null
    
    // 数据加载器
    FuelUnloadingDataLoader {
        id: dataLoader
        
        onDataLoaded: function(success, message) {
            if (success) {
                console.log("FuelUnloadingDisplay:", message)
            } else {
                console.error("FuelUnloadingDisplay: 数据加载失败:", message)
            }
        }
    }
    
    // 监听坐标转换状态变化
    Connections {
        target: typeof controller !== 'undefined' ? controller : null
        function onCoordinateConversionChanged() {
            if (controller && typeof controller.coordinateConversionEnabled !== 'undefined') {
                var newState = controller.coordinateConversionEnabled
                if (offsetCorrectionEnabled !== newState) {
                    offsetCorrectionEnabled = newState
                    applyOffsetCorrection(offsetCorrectionEnabled)
                }
            }
        }
    }
    
    // 卸油标记组件
    Component {
        id: fuelMarkerComponent
        
        MapQuickItem {
            id: markerItem
            
            property string plateNumber: ""
            property string date: ""
            property string time: ""
            property string fuelType: ""
            property real amount: 0
            property color markerColor: "#e74c3c"
            
            anchorPoint.x: marker.width / 2
            anchorPoint.y: marker.height
            
            sourceItem: Item {
                id: marker
                width: 40
                height: 50
                
                // 标记图标
                Rectangle {
                    id: markerIcon
                    width: 30
                    height: 30
                    radius: 15
                    color: markerItem.markerColor
                    border.color: "#ffffff"
                    border.width: 2
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    
                    // 燃油类型图标
                    Text {
                        anchors.centerIn: parent
                        text: markerItem.fuelType === "汽油" ? "⛽" : "🛢️"
                        font.pixelSize: 16
                        color: "white"
                    }
                    
                    // 鼠标悬停效果
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        
                        onEntered: {
                            markerIcon.scale = 1.2
                            tooltip.visible = true
                        }
                        
                        onExited: {
                            markerIcon.scale = 1.0
                            tooltip.visible = false
                        }
                        
                        onClicked: {
                            showDetailDialog(markerItem)
                        }
                    }
                }
                
                // 指向地面的三角形
                Canvas {
                    id: pointer
                    width: 20
                    height: 20
                    anchors.horizontalCenter: markerIcon.horizontalCenter
                    anchors.top: markerIcon.bottom
                    anchors.topMargin: -2
                    
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)
                        ctx.fillStyle = markerItem.markerColor
                        ctx.strokeStyle = "#ffffff"
                        ctx.lineWidth = 2
                        
                        ctx.beginPath()
                        ctx.moveTo(width / 2, height - 5)
                        ctx.lineTo(5, 5)
                        ctx.lineTo(width - 5, 5)
                        ctx.closePath()
                        ctx.fill()
                        ctx.stroke()
                    }
                }
                
                // 悬浮提示
                Rectangle {
                    id: tooltip
                    visible: false
                    width: tooltipText.width + 20
                    height: tooltipText.height + 10
                    color: "#2c3e50"
                    radius: 5
                    border.color: "#34495e"
                    border.width: 1
                    
                    anchors.bottom: markerIcon.top
                    anchors.horizontalCenter: markerIcon.horizontalCenter
                    anchors.bottomMargin: 5
                    
                    Text {
                        id: tooltipText
                        anchors.centerIn: parent
                        text: markerItem.plateNumber + "\n" + 
                              markerItem.date + " " + markerItem.time + "\n" +
                              markerItem.fuelType + " " + markerItem.amount + "吨"
                        color: "white"
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }
    }
    
    // 详细信息对话框
    Rectangle {
        id: detailDialog
        visible: false
        width: 300
        height: 200
        color: "#ffffff"
        border.color: "#3498db"
        border.width: 2
        radius: 8
        z: 1000
        
        anchors.centerIn: parent
        
        property string plateNumber: ""
        property string date: ""
        property string time: ""
        property string fuelType: ""
        property real amount: 0
        property real longitude: 0
        property real latitude: 0
        
        Column {
            anchors.fill: parent
            anchors.margins: 15
            spacing: 10
            
            Text {
                text: "卸油记录详情"
                font.pixelSize: 18
                font.bold: true
                color: "#2c3e50"
            }
            
            Rectangle {
                width: parent.width
                height: 1
                color: "#bdc3c7"
            }
            
            Text {
                text: "车牌号: " + detailDialog.plateNumber
                font.pixelSize: 14
                color: "#34495e"
            }
            
            Text {
                text: "日期: " + detailDialog.date
                font.pixelSize: 14
                color: "#34495e"
            }
            
            Text {
                text: "时间: " + detailDialog.time
                font.pixelSize: 14
                color: "#34495e"
            }
            
            Text {
                text: "燃油类型: " + detailDialog.fuelType
                font.pixelSize: 14
                color: "#34495e"
            }
            
            Text {
                text: "卸油量: " + detailDialog.amount + " 吨"
                font.pixelSize: 14
                color: "#e74c3c"
                font.bold: true
            }
            
            Text {
                text: "坐标: " + detailDialog.longitude.toFixed(6) + ", " + detailDialog.latitude.toFixed(6)
                font.pixelSize: 12
                color: "#7f8c8d"
            }
        }
        
        // 关闭按钮
        Button {
            text: "关闭"
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            
            onClicked: {
                detailDialog.visible = false
            }
        }
    }
    
    // 通知组件
    Rectangle {
        id: notificationRect
        visible: false
        width: notificationText.width + 20
        height: 40
        color: "#2ecc71"
        radius: 5
        border.color: "#27ae60"
        border.width: 1
        
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 60
        z: 1001
        
        Text {
            id: notificationText
            anchors.centerIn: parent
            color: "white"
            font.pixelSize: 14
            font.bold: true
        }
        
        Timer {
            id: notificationTimer
            interval: 3000
            onTriggered: {
                notificationRect.visible = false
            }
        }
    }
    
    // 公共函数
    
    // 设置目标地图
    function setTargetMap(map) {
        targetMap = map
        if (dataLoader.isLoaded) {
            createUnloadingMarkers()
            fitViewToMarkers()
        }
    }
    
    // 显示所有卸油记录
    function showAllRecords() {
        if (dataLoader.isLoaded) {
            createUnloadingMarkers()
            fitViewToMarkers()
        }
    }
    
    // 显示特定车辆的卸油记录
    function showVehicleRecords(plateNumber) {
        if (!targetMap || !dataLoader.isLoaded) {
            console.warn("FuelUnloadingDisplay: 地图或数据未准备好")
            return
        }
        
        // 清除现有标记
        clearMarkers()
        
        var colors = ["#e74c3c", "#3498db", "#2ecc71", "#f39c12", "#9b59b6"]
        var colorIndex = 0
        var allRecords = dataLoader.getAllRecords()
        var vehicleRecords = []
        
        // 筛选特定车辆的记录
        for (var i = 0; i < allRecords.length; i++) {
            if (allRecords[i].plateNumber === plateNumber) {
                vehicleRecords.push(allRecords[i])
            }
        }
        
        // 创建标记
        for (var j = 0; j < vehicleRecords.length; j++) {
            var record = vehicleRecords[j]
            var coordinate = getCoordinate(record, offsetCorrectionEnabled)
            
            var marker = fuelMarkerComponent.createObject(targetMap, {
                coordinate: coordinate,
                plateNumber: record.plateNumber,
                date: record.date,
                time: record.time,
                fuelType: record.fuelType,
                amount: record.amount,
                markerColor: colors[colorIndex % colors.length]
            })
            
            if (marker) {
                targetMap.addMapItem(marker)
                markerItems.push(marker)
            }
        }
        
        var coordStatus = offsetCorrectionEnabled ? " (偏移纠正)" : " (原始坐标)"
        console.log("FuelUnloadingDisplay: 创建了", markerItems.length, "个", plateNumber, "的卸油标记" + coordStatus)
        
        // 调整视图
        fitViewToMarkers()
    }
    
    // 创建卸油标记
    function createUnloadingMarkers() {
        if (!targetMap || !dataLoader.isLoaded) {
            console.warn("FuelUnloadingDisplay: 地图或数据未准备好")
            return
        }
        
        // 清除现有标记
        clearMarkers()
        
        var colors = ["#e74c3c", "#3498db", "#2ecc71", "#f39c12", "#9b59b6"]
        var colorIndex = 0
        var allRecords = dataLoader.getAllRecords()
        
        for (var i = 0; i < allRecords.length; i++) {
            var record = allRecords[i]
            var coordinate = getCoordinate(record, offsetCorrectionEnabled)
            
            // 每个车辆使用不同颜色
            if (i > 0 && allRecords[i].plateNumber !== allRecords[i-1].plateNumber) {
                colorIndex++
            }
            
            var marker = fuelMarkerComponent.createObject(targetMap, {
                coordinate: coordinate,
                plateNumber: record.plateNumber,
                date: record.date,
                time: record.time,
                fuelType: record.fuelType,
                amount: record.amount,
                markerColor: colors[colorIndex % colors.length]
            })
            
            if (marker) {
                targetMap.addMapItem(marker)
                markerItems.push(marker)
                
                var coordType = offsetCorrectionEnabled ? "纠正后" : "原始"
                console.log("创建标记:", record.plateNumber, record.date, record.fuelType, 
                           record.amount + "吨", "(" + coordType + "坐标)")
            }
        }
        
        var coordStatus = offsetCorrectionEnabled ? " (偏移纠正)" : " (原始坐标)"
        console.log("FuelUnloadingDisplay: 创建了", markerItems.length, "个卸油标记" + coordStatus)
    }
    
    // 清除所有标记
    function clearMarkers() {
        if (!targetMap) return
        
        for (var i = 0; i < markerItems.length; i++) {
            targetMap.removeMapItem(markerItems[i])
        }
        markerItems = []
    }
    
    // 调整视图以显示所有标记
    function fitViewToMarkers() {
        if (!targetMap || markerItems.length === 0) {
            console.warn("FuelUnloadingDisplay: 没有地图或标记可显示")
            return
        }
        
        // 创建包含所有标记的地理路径
        var geoPath = QtPositioning.path()
        
        for (var i = 0; i < markerItems.length; i++) {
            geoPath.addCoordinate(markerItems[i].coordinate)
        }
        
        // 调整地图视图
        targetMap.fitViewportToGeoShape(geoPath, Qt.size(50, 50))
        console.log("FuelUnloadingDisplay: 调整视图以显示", markerItems.length, "个标记")
    }
    
    // 显示详细信息对话框
    function showDetailDialog(marker) {
        detailDialog.plateNumber = marker.plateNumber
        detailDialog.date = marker.date
        detailDialog.time = marker.time
        detailDialog.fuelType = marker.fuelType
        detailDialog.amount = marker.amount
        detailDialog.longitude = marker.coordinate.longitude
        detailDialog.latitude = marker.coordinate.latitude
        detailDialog.visible = true
        
        console.log("显示详情:", marker.plateNumber, marker.date)
    }
    
    // 应用偏移纠正
    function applyOffsetCorrection(enabled) {
        console.log("应用偏移纠正:", enabled ? "启用" : "禁用")
        
        // 重新创建标记以应用新的坐标
        createUnloadingMarkers()
        
        // 重新调整视图
        fitViewToMarkers()
        
        // 显示通知
        var message = enabled ? "已启用地图偏移纠正 (GCJ-02 → WGS84)" : "已关闭地图偏移纠正"
        showNotification(message)
    }
    
    // 获取坐标（根据偏移纠正状态）
    function getCoordinate(record, offsetCorrectionEnabled) {
        if (offsetCorrectionEnabled && record.correctedLongitude && record.correctedLatitude) {
            return QtPositioning.coordinate(record.correctedLatitude, record.correctedLongitude)
        } else {
            return QtPositioning.coordinate(record.latitude, record.longitude)
        }
    }
    
    // 显示通知
    function showNotification(message) {
        notificationText.text = message
        notificationRect.visible = true
        notificationTimer.restart()
    }
}