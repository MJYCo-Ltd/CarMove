import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

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
        spacing: 10
        
        // 左侧面板：文件夹选择和车辆列表
        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: "#f0f0f0"
            border.color: "#ccc"
            
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
                    
                    ListView {
                        id: vehicleListView
                        anchors.fill: parent
                        model: (controller && controller.vehicleList) ? controller.vehicleList : []
                        focus: true
                        keyNavigationEnabled: true
                        
                        // Empty state
                        Text {
                            anchors.centerIn: parent
                            text: (controller && typeof controller.currentFolder !== 'undefined' && controller.currentFolder) ? 
                                  "该文件夹中未找到车辆数据" : 
                                  "请先选择包含车辆数据的文件夹"
                            color: "#7f8c8d"
                            font.pixelSize: 12
                            visible: vehicleListView.count === 0
                            horizontalAlignment: Text.AlignHCenter
                        }
                        
                        delegate: ItemDelegate {
                            width: vehicleListView.width
                            height: 60
                            hoverEnabled: true
                            
                            Rectangle {
                                anchors.fill: parent
                                color: (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData) ? "#3498db" : 
                                       (parent.hovered ? "#ecf0f1" : "transparent")
                                border.color: "#bdc3c7"
                                radius: 4
                                
                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    
                                    Text {
                                        text: modelData
                                        font.bold: true
                                        font.pixelSize: 14
                                        color: (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData) ? "white" : "black"
                                    }
                                    
                                    Text {
                                        text: (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData) ? 
                                              "已选择 - 点击查看轨迹" : 
                                              "点击选择此车辆"
                                        font.pixelSize: 10
                                        color: (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData) ? "#ecf0f1" : "#7f8c8d"
                                    }
                                }
                                
                                // Selection indicator
                                Rectangle {
                                    width: 4
                                    height: parent.height
                                    color: "#3498db"
                                    visible: controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData
                                    anchors.left: parent.left
                                }
                            }
                            
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
    
    // Error dialog
    Dialog {
        id: errorDialog
        title: "错误"
        modal: true
        anchors.centerIn: parent
        width: 350
        height: 200
        
        property string errorMessage: ""
        
        contentItem: Column {
            spacing: 10
            padding: 20
            
            Text {
                text: errorDialog.errorMessage
                wrapMode: Text.WordWrap
                width: 300
            }
            
            Button {
                text: "确定"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: errorDialog.close()
            }
        }
    }
    
    // Success notification
    Dialog {
        id: successDialog
        title: "成功"
        modal: true
        anchors.centerIn: parent
        width: 350
        height: 200
        
        property string successMessage: ""
        
        contentItem: Column {
            spacing: 10
            padding: 20
            
            Text {
                text: successDialog.successMessage
                wrapMode: Text.WordWrap
                width: 300
                color: "#27ae60"
            }
            
            Button {
                text: "确定"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: successDialog.close()
            }
        }
    }
    
    // Connect to controller signals
    Connections {
        target: controller
        
        function onFolderScanned(success, message) {
            if (success) {
                successDialog.successMessage = message
                successDialog.open()
            } else {
                errorDialog.errorMessage = message
                errorDialog.open()
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
                errorDialog.errorMessage = message
                errorDialog.open()
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
            errorDialog.errorMessage = error
            errorDialog.open()
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
