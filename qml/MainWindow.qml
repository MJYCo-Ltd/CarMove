import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CarMove 1.0

ApplicationWindow {
    id: mainWindow
    width: 1200
    height: 800
    title: "CarMove 车辆轨迹追踪系统"
    visible: true
    
    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: folderDialog.open()
    }
    
    Shortcut {
        sequence: "Space"
        onActivated: {
            if (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle) {
                if (typeof controller.isPlaying !== 'undefined' && controller.isPlaying) {
                    if (typeof controller.pausePlayback === 'function') {
                        controller.pausePlayback()
                    }
                } else {
                    if (typeof controller.startPlayback === 'function') {
                        controller.startPlayback()
                    }
                }
            }
        }
    }
    
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (controller && typeof controller.stopPlayback === 'function') {
                controller.stopPlayback()
            }
        }
    }
    
    // Status bar for displaying information
    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            
            Label {
                text: (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle) ? 
                      "已选择车辆: " + controller.selectedVehicle : 
                      "未选择车辆"
                Layout.fillWidth: true
            }
            
            Label {
                text: (controller && controller.vehicleList && typeof controller.vehicleList.length !== 'undefined') ? 
                      "车辆数量: " + controller.vehicleList.length : 
                      "无车辆数据"
            }
            
            Label {
                text: (controller && typeof controller.isPlaying !== 'undefined' && controller.isPlaying) ? "播放中" : "已暂停"
                color: (controller && typeof controller.isPlaying !== 'undefined' && controller.isPlaying) ? "#27ae60" : "#7f8c8d"
            }
        }
    }
    
    RowLayout {
        anchors.fill: parent
        spacing: 0
        
        // 左侧功能侧边栏
        SidebarPanel {
            id: sidebarPanel
            Layout.preferredWidth: 60
            Layout.fillHeight: true
            
            onModeChanged: function(mode) {
                console.log("切换到模式:", mode)
                if (mode === "trajectory") {
                    leftPanel.visible = true
                    fuelRecordsPanel.visible = false
                    // 清除卸油标记
                    mapDisplay.clearFuelMarkers()
                } else if (mode === "fuel") {
                    leftPanel.visible = false
                    fuelRecordsPanel.visible = true
                    // 清除轨迹
                    mapDisplay.clearTrajectory()
                }
            }
        }
        
        // 左侧面板：文件夹选择和车辆列表（轨迹模式）
        Rectangle {
            id: leftPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: "#f0f0f0"
            border.color: "#ccc"
            visible: sidebarPanel.currentMode === "trajectory"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                
                // 文件夹选择区域
                GroupBox {
                    title: "数据文件夹"
                    Layout.fillWidth: true
                    
                    ColumnLayout {
                        anchors.fill: parent
                        
                        Button {
                            text: "选择文件夹"
                            Layout.fillWidth: true
                            enabled: controller ? (typeof controller.isLoading !== 'undefined' ? !controller.isLoading : true) : true
                            onClicked: folderDialog.open()
                        }
                        
                        Text {
                            text: (controller && typeof controller.currentFolder !== 'undefined' && controller.currentFolder) ? controller.currentFolder : "未选择文件夹"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: (controller && typeof controller.currentFolder !== 'undefined' && controller.currentFolder) ? "#2c3e50" : "#7f8c8d"
                        }
                        
                        // Loading indicator
                        BusyIndicator {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            visible: controller ? (typeof controller.isLoading !== 'undefined' ? controller.isLoading : false) : false
                            running: controller ? (typeof controller.isLoading !== 'undefined' ? controller.isLoading : false) : false
                        }
                        
                        // Progress text
                        Text {
                            text: (controller && typeof controller.loadingMessage !== 'undefined' && controller.loadingMessage) ? controller.loadingMessage : ""
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 10
                            color: "#7f8c8d"
                            visible: controller ? (typeof controller.isLoading !== 'undefined' ? controller.isLoading : false) : false
                        }
                    }
                }
                
                // 车辆列表区域
                GroupBox {
                    title: "车辆列表"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 5
                        
                        // 搜索框
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            
                            TextField {
                                id: searchField
                                Layout.fillWidth: true
                                placeholderText: "输入车牌号前缀搜索 (如: 冀A)..."
                                text: controller ? controller.searchText : ""
                                
                                // Add search icon
                                leftPadding: 30
                                
                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.leftMargin: 8
                                    width: 16
                                    height: 16
                                    color: "transparent"
                                    
                                    Text {
                                        anchors.centerIn: parent
                                        text: "🔍"
                                        font.pixelSize: 12
                                        color: "#7f8c8d"
                                    }
                                }
                                
                                onTextChanged: {
                                    if (controller && typeof controller.setSearchText === 'function') {
                                        controller.setSearchText(text)
                                    }
                                }
                                
                                // Add keyboard shortcuts
                                Keys.onEscapePressed: {
                                    if (controller && typeof controller.clearSearch === 'function') {
                                        controller.clearSearch()
                                    }
                                }
                            }
                            
                            Button {
                                text: "清除"
                                enabled: searchField.text.length > 0
                                Layout.preferredWidth: 50
                                
                                onClicked: {
                                    if (controller && typeof controller.clearSearch === 'function') {
                                        controller.clearSearch()
                                    }
                                }
                            }
                        }
                        
                        // Search results info
                        Text {
                            Layout.fillWidth: true
                            text: {
                                if (controller && controller.searchText && controller.searchText.length > 0) {
                                    var totalCount = controller.vehicleList ? controller.vehicleList.length : 0
                                    var filteredCount = controller.filteredVehicleList ? controller.filteredVehicleList.length : 0
                                    return "找到 " + filteredCount + " / " + totalCount + " 辆车"
                                }
                                return ""
                            }
                            font.pixelSize: 10
                            color: "#7f8c8d"
                            visible: controller && controller.searchText && controller.searchText.length > 0
                        }
                        
                        ListView {
                            id: vehicleListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: (controller && controller.filteredVehicleList) ? controller.filteredVehicleList : []
                            focus: true
                            keyNavigationEnabled: true
                            clip: true
                            
                            // Empty state
                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (!controller || typeof controller.currentFolder === 'undefined' || !controller.currentFolder) {
                                        return "请先选择包含车辆数据的文件夹"
                                    } else if (controller.vehicleList && controller.vehicleList.length === 0) {
                                        return "该文件夹中未找到车辆数据"
                                    } else if (controller.searchText && controller.searchText.length > 0) {
                                        return "未找到匹配的车辆"
                                    } else {
                                        return ""
                                    }
                                }
                                color: "#7f8c8d"
                                font.pixelSize: 12
                                visible: vehicleListView.count === 0
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                width: parent.width - 20
                            }
                            
                            delegate: VehicleInfoCard {
                                width: vehicleListView.width
                                height: 60
                                
                                plateNumber: modelData
                                isSelected: controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData
                                layoutMode: "horizontal"
                                showSelectionIndicator: true
                                
                                onClicked: {
                                    if (controller && typeof controller.selectVehicle === 'function') {
                                        controller.selectVehicle(modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // 卸油记录面板（卸油模式）
        FuelRecordsPanel {
            id: fuelRecordsPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: sidebarPanel.currentMode === "fuel"
            
            onVehicleSelected: function(plateNumber) {
                console.log("选择车辆:", plateNumber)
                mapDisplay.showVehicleFuelRecords(plateNumber)
            }
            
            onShowAllRecords: {
                console.log("显示所有卸油记录")
                mapDisplay.showAllFuelRecords()
            }
        }
        
        // 右侧：地图和控制面板
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 地图显示区域
            MapDisplay {
                id: mapDisplay
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            
            // 播放控制面板
            PlaybackControls {
                id: playbackControls
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                
                // Handle coordinate conversion toggle
                onCoordinateConversionToggled: {
                    // Update map display with converted trajectory
                    if (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle &&
                        typeof controller.getConvertedTrajectory === 'function') {
                        mapDisplay.updateTrajectoryCoordinates(controller.getConvertedTrajectory())
                    }
                    
                    // Update fuel unloading display with converted coordinates
                    if (sidebarPanel.currentMode === "fuel") {
                        // The FuelUnloadingDisplay will automatically update via the Connections
                        console.log("坐标转换状态已更新，卸油标记将自动更新")
                    }
                }
            }
        }
    }
    
    // 文件夹选择对话框
    FolderDialog {
        id: folderDialog
        title: "选择包含车辆数据的文件夹"
        onAccepted: {
            if (controller && typeof controller.selectFolder === 'function') {
                controller.selectFolder(selectedFolder)
            }
        }
    }
    
    // Error and success notifications
    NotificationDialog {
        id: errorDialog
        onOpened: {
            showError(errorMessage)
        }
        
        property string errorMessage: ""
        
        function showErrorMessage(message) {
            errorMessage = message
            showError(message)
        }
    }
    
    NotificationDialog {
        id: successDialog
        onOpened: {
            showSuccess(successMessage)
        }
        
        property string successMessage: ""
        
        function showSuccessMessage(message) {
            successMessage = message
            showSuccess(message)
        }
    }
    
    // Connect to controller signals
    Connections {
        target: controller
        
        function onFolderScanned(success, message) {
            if (success) {
                successDialog.showSuccessMessage(message)
            } else {
                errorDialog.showErrorMessage(message)
            }
        }
        
        function onTrajectoryLoaded(success, message) {
            if (success) {
                console.log("Trajectory loaded successfully:", message)
                // Update map display with the loaded trajectory
                if (controller && typeof controller.getConvertedTrajectory === 'function' && 
                    typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle) {
                    var trajectory = controller.getConvertedTrajectory()
                    if (trajectory && trajectory.length > 0) {
                        mapDisplay.addVehicleTrajectory(controller.selectedVehicle, trajectory, "#3498db")
                    }
                }
            } else {
                errorDialog.showErrorMessage(message)
            }
        }
        
        function onTrajectoryConverted() {
            console.log("Trajectory converted, updating map display")
            // Update map display with converted coordinates
            if (controller && typeof controller.getConvertedTrajectory === 'function') {
                var trajectory = controller.getConvertedTrajectory()
                if (trajectory && trajectory.length > 0) {
                    mapDisplay.updateTrajectoryCoordinates(trajectory)
                }
            }
        }
        
        function onErrorOccurred(error) {
            errorDialog.showErrorMessage(error)
        }
        
        function onVehiclePositionUpdated(plateNumber, position, direction, speed) {
            // Forward to map display for real-time position updates
            mapDisplay.updateVehiclePosition(plateNumber, position, direction, speed)
        }
        
        function onSelectedVehicleChanged() {
            // Clear map when vehicle selection changes
            if (!controller || typeof controller.selectedVehicle === 'undefined' || !controller.selectedVehicle) {
                mapDisplay.clearTrajectory()
            }
        }
        
        function onCurrentTimeChanged() {
            // Update UI elements that depend on current time
            if (controller && typeof controller.currentTime !== 'undefined') {
                console.log("Current time changed:", controller.currentTime)
            }
        }
        
        function onPlaybackStateChanged() {
            // Update UI elements that depend on playback state
            if (controller && typeof controller.isPlaying !== 'undefined') {
                console.log("Playback state changed, isPlaying:", controller.isPlaying)
            }
        }
    }
}
