import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: playbackControls
    color: "#34495e"
    border.color: "#2c3e50"
    
    // Properties bound to controller with proper null checks
    property bool isPlaying: (controller && typeof controller.isPlaying !== 'undefined') ? controller.isPlaying : false
    property double playbackProgress: (controller && typeof controller.playbackProgress !== 'undefined') ? controller.playbackProgress : 0.0
    property bool hasValidTimeRange: (controller && controller.startTime && controller.endTime && 
                                    controller.startTime.getTime && controller.endTime.getTime) ? 
                                    (controller.startTime.getTime() > 0 && controller.endTime.getTime() > 0) : false
    
    // Signal for external components to handle coordinate conversion updates
    signal coordinateConversionToggled()
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15
        
        // Time range indicator for long-term data
        Rectangle {
            visible: playbackControls.hasValidTimeRange && isLongTermData()
            color: "#2c3e50"
            border.color: "#34495e"
            border.width: 1
            radius: 4
            Layout.preferredWidth: 120
            Layout.preferredHeight: 40
            
            Column {
                anchors.centerIn: parent
                spacing: 2
                
                Text {
                    text: "数据范围"
                    color: "#bdc3c7"
                    font.pixelSize: 9
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: getTimeRangeInfo()
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
            
            ToolTip.visible: hovered
            ToolTip.text: "数据时间跨度: " + getTimeRangeInfo() + 
                         "\n开始: " + (controller && controller.startTime ? Qt.formatDateTime(controller.startTime, "yyyy-MM-dd hh:mm") : "N/A") +
                         "\n结束: " + (controller && controller.endTime ? Qt.formatDateTime(controller.endTime, "yyyy-MM-dd hh:mm") : "N/A")
        }
        
        // 播放控制按钮
        RowLayout {
            spacing: 5
            
            Button {
                id: playButton
                text: playbackControls.isPlaying ? "⏸" : "▶"
                font.pixelSize: 16
                enabled: playbackControls.hasValidTimeRange && controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle
                
                ToolTip.visible: hovered
                ToolTip.text: playbackControls.isPlaying ? "暂停播放 (Space)" : "开始播放 (Space)"
                
                onClicked: {
                    if (!controller) return
                    if (playbackControls.isPlaying) {
                        controller.pausePlayback()
                    } else {
                        controller.startPlayback()
                    }
                }
            }
            
            Button {
                id: stopButton
                text: "⏹"
                font.pixelSize: 16
                enabled: playbackControls.hasValidTimeRange && controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle
                
                ToolTip.visible: hovered
                ToolTip.text: "停止播放 (Esc)"
                
                onClicked: {
                    if (controller) {
                        controller.stopPlayback()
                    }
                }
            }
        }
        
        // 当前时间显示 - Enhanced for long-term data
        TimeDisplay {
            id: currentTimeDisplay
            dateTime: controller && controller.currentTime ? controller.currentTime : null
            showTime: true
            showDate: playbackControls.hasValidTimeRange && isLongTermData()
            timeColor: "white"
            dateColor: "#bdc3c7"
            timeFontSize: 12
            dateFontSize: 9
            timeFontBold: true
        }
        
        // 可拖动的时间条
        Slider {
            id: timeSlider
            Layout.fillWidth: true
            from: 0
            to: 1.0
            value: playbackControls.playbackProgress
            enabled: playbackControls.hasValidTimeRange && controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle
            
            property bool isDragging: false
            property bool wasPlayingBeforeDrag: false
            
            // Update slider value when controller progress changes (only if not dragging)
            Connections {
                target: controller
                function onProgressChanged() {
                    if (controller && typeof controller.playbackProgress !== 'undefined' && !timeSlider.pressed && !timeSlider.isDragging) {
                        timeSlider.value = controller.playbackProgress
                    }
                }
            }
            
            // Enhanced drag handling with real-time position updates
            onPressedChanged: {
                if (!controller) return
                
                if (pressed) {
                    // Start dragging
                    timeSlider.isDragging = true
                    timeSlider.wasPlayingBeforeDrag = (typeof controller.isPlaying !== 'undefined') ? controller.isPlaying : false
                    
                    // Set dragging mode for optimized performance
                    if (typeof controller.setDraggingMode === 'function') {
                        controller.setDraggingMode(true)
                    }
                    
                    // Pause playback during dragging for smoother experience
                    if (controller.isPlaying && typeof controller.pausePlayback === 'function') {
                        controller.pausePlayback()
                    }
                    
                    // Immediate position update on press
                    if (typeof controller.seekToProgress === 'function') {
                        controller.seekToProgress(value)
                    }
                } else {
                    // End dragging
                    timeSlider.isDragging = false
                    
                    // Clear dragging mode
                    if (typeof controller.setDraggingMode === 'function') {
                        controller.setDraggingMode(false)
                    }
                    
                    // Final position update on release
                    if (typeof controller.seekToProgress === 'function') {
                        controller.seekToProgress(value)
                    }
                    
                    // Resume playback if it was playing before drag
                    if (timeSlider.wasPlayingBeforeDrag && typeof controller.startPlayback === 'function') {
                        controller.startPlayback()
                    }
                }
            }
            
            // Real-time position updates during dragging
            onMoved: {
                if (pressed && isDragging && controller) {
                    // Update vehicle position in real-time during drag
                    controller.seekToProgress(value)
                }
            }
            
            // Handle value changes from external sources (e.g., keyboard, mouse wheel)
            onValueChanged: {
                if ((pressed || isDragging) && controller) {
                    // Ensure position updates even for programmatic value changes during drag
                    controller.seekToProgress(value)
                }
            }
            
            // Keyboard support for fine control
            Keys.onLeftPressed: {
                if (enabled && controller) {
                    var newValue = Math.max(from, value - 0.01) // 1% step
                    value = newValue
                    controller.seekToProgress(newValue)
                }
            }
            
            Keys.onRightPressed: {
                if (enabled && controller) {
                    var newValue = Math.min(to, value + 0.01) // 1% step
                    value = newValue
                    controller.seekToProgress(newValue)
                }
            }
            
            // Mouse wheel support for fine control
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                
                onWheel: {
                    if (timeSlider.enabled && wheel.angleDelta.y !== 0 && controller) {
                        var delta = wheel.angleDelta.y > 0 ? 0.005 : -0.005 // 0.5% step
                        var newValue = Math.max(timeSlider.from, Math.min(timeSlider.to, timeSlider.value + delta))
                        timeSlider.value = newValue
                        controller.seekToProgress(newValue)
                        wheel.accepted = true
                    }
                }
            }
            
            // Tooltip showing current time during hover/drag - Enhanced for long-term data
            ToolTip.visible: hovered || pressed
            ToolTip.text: {
                if (!controller || !controller.startTime || !controller.endTime) return ""
                
                var totalMs = controller.endTime.getTime() - controller.startTime.getTime()
                var currentMs = controller.startTime.getTime() + (totalMs * value)
                var currentTime = new Date(currentMs)
                
                // For long-term data, show both date and time
                if (isLongTermData()) {
                    return Qt.formatDateTime(currentTime, "yyyy-MM-dd hh:mm:ss") + 
                           "\n进度: " + Math.round(value * 100) + "%"
                } else {
                    return Qt.formatDateTime(currentTime, "hh:mm:ss") + 
                           "\n进度: " + Math.round(value * 100) + "%"
                }
            }
        }
        
        // 结束时间显示 - Enhanced for long-term data
        TimeDisplay {
            id: endTimeDisplay
            dateTime: controller && controller.endTime ? controller.endTime : null
            showTime: true
            showDate: playbackControls.hasValidTimeRange && isLongTermData()
            timeColor: "white"
            dateColor: "#bdc3c7"
            timeFontSize: 12
            dateFontSize: 9
            timeFontBold: true
        }
        
        // 播放速度控制 - Enhanced for long-term data
        RowLayout {
            spacing: 5
            
            Text {
                text: "速度:"
                color: "white"
                font.pixelSize: 12
            }
            
            ComboBox {
                id: speedCombo
                model: isLongTermData() ? 
                       ["0.1x", "0.5x", "1x", "2x", "5x", "10x", "50x", "100x"] :
                       ["0.5x", "1x", "2x", "5x", "10x"]
                currentIndex: isLongTermData() ? 2 : 1  // Default to 1x
                enabled: controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle
                
                Layout.preferredWidth: 80
                
                ToolTip.visible: hovered
                ToolTip.text: isLongTermData() ? 
                             "调整播放速度 (推荐长期数据使用较高速度)" :
                             "调整播放速度"
                
                onCurrentTextChanged: {
                    if (controller && typeof controller.setPlaybackSpeed === 'function') {
                        var speed = parseFloat(currentText.replace("x", ""))
                        controller.setPlaybackSpeed(speed)
                    }
                }
            }
        }
        
        // 坐标系转换按钮
        Button {
            id: coordinateButton
            text: (controller && typeof controller.coordinateConversionEnabled !== 'undefined' && controller.coordinateConversionEnabled) ? "火星坐标" : "GPS坐标"
            font.pixelSize: 10
            enabled: controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle
            
            Layout.preferredWidth: 90
            
            onClicked: {
                if (controller && typeof controller.toggleCoordinateConversion === 'function') {
                    controller.toggleCoordinateConversion()
                    // Emit signal for external components to handle
                    playbackControls.coordinateConversionToggled()
                }
            }
            
            ToolTip.visible: hovered
            ToolTip.text: (controller && typeof controller.coordinateConversionEnabled !== 'undefined' && controller.coordinateConversionEnabled) ? 
                         "当前使用GCJ02火星坐标系\n点击切换到WGS84 GPS坐标系" : 
                         "当前使用WGS84 GPS坐标系\n点击切换到GCJ02火星坐标系"
        }
    }
    
    function isLongTermData() {
        if (!controller || !controller.startTime || !controller.endTime || 
            !controller.startTime.getTime || !controller.endTime.getTime) return false
        
        var totalMs = controller.endTime.getTime() - controller.startTime.getTime()
        var totalDays = totalMs / (1000 * 60 * 60 * 24)
        
        return totalDays > 7  // Consider data spanning more than 7 days as long-term
    }
    
    function getTimeRangeInfo() {
        if (!controller || !controller.startTime || !controller.endTime || 
            !controller.startTime.getTime || !controller.endTime.getTime) return ""
        
        return currentTimeDisplay.getTimeRangeInfo(controller.startTime, controller.endTime)
    }
}
