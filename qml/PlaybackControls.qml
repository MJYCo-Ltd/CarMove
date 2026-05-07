import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: playbackControls
    /// 与 MapDisplay 中坐标切换按钮显隐同源（由 MainWindow 绑定 mapDisplay.mapVehicleContextActive）；控制整条回放栏显隐
    property bool mapVehicleContextActive: false
    visible: mapVehicleContextActive

    color: "#34495e"
    border.color: "#2c3e50"

    readonly property var pb: controller && controller.playback ? controller.playback : null

    property bool isPlaying: pb ? pb.isPlaying : false
    property double playbackProgress: pb ? pb.playbackProgress : 0.0
    property bool hasValidTimeRange: pb && pb.hasValidPlaybackTimeRange

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 15

        Rectangle {
            visible: playbackControls.hasValidTimeRange && pb && pb.playbackIsLongTerm
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
                    text: pb ? pb.playbackTimeRangeSummary : ""
                    color: "white"
                    font.pixelSize: 11
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: pb ? pb.playbackTimeRangeTooltip() : ""
        }

        RowLayout {
            spacing: 5

            Button {
                id: playButton
                text: playbackControls.isPlaying ? "⏸" : "▶"
                font.pixelSize: 16
                enabled: playbackControls.hasValidTimeRange && playbackControls.mapVehicleContextActive

                ToolTip.visible: hovered
                ToolTip.text: playbackControls.isPlaying ? "暂停播放 (Space)" : "开始播放 (Space)"

                onClicked: {
                    if (!pb) return
                    if (playbackControls.isPlaying)
                        pb.pausePlayback()
                    else
                        pb.startPlayback()
                }
            }

            Button {
                id: stopButton
                text: "⏹"
                font.pixelSize: 16
                enabled: playbackControls.hasValidTimeRange && playbackControls.mapVehicleContextActive

                ToolTip.visible: hovered
                ToolTip.text: "停止播放 (Esc)"

                onClicked: {
                    if (pb)
                        pb.stopPlayback()
                }
            }
        }

        TimeDisplay {
            id: currentTimeDisplay
            dateTime: pb && pb.currentTime ? pb.currentTime : null
            showTime: true
            showDate: playbackControls.hasValidTimeRange && pb && pb.playbackIsLongTerm
            timeColor: "white"
            dateColor: "#bdc3c7"
            timeFontSize: 12
            dateFontSize: 9
            timeFontBold: true
        }

        Slider {
            id: timeSlider
            Layout.fillWidth: true
            from: 0
            to: 1.0
            value: playbackControls.playbackProgress
            enabled: playbackControls.hasValidTimeRange && playbackControls.mapVehicleContextActive

            property bool isDragging: false
            property bool wasPlayingBeforeDrag: false

            Connections {
                target: pb
                function onProgressChanged() {
                    if (pb && !timeSlider.pressed && !timeSlider.isDragging)
                        timeSlider.value = pb.playbackProgress
                }
            }

            onPressedChanged: {
                if (!pb) return

                if (pressed) {
                    timeSlider.isDragging = true
                    timeSlider.wasPlayingBeforeDrag = pb.isPlaying

                    pb.setDraggingMode(true)

                    if (pb.isPlaying)
                        pb.pausePlayback()

                    pb.seekToProgress(value)
                } else {
                    timeSlider.isDragging = false

                    pb.setDraggingMode(false)

                    pb.seekToProgress(value)

                    if (timeSlider.wasPlayingBeforeDrag)
                        pb.startPlayback()
                }
            }

            onMoved: {
                if (pressed && isDragging && pb)
                    pb.seekToProgress(value)
            }

            onValueChanged: {
                if ((pressed || isDragging) && pb)
                    pb.seekToProgress(value)
            }

            Keys.onLeftPressed: {
                if (enabled && pb)
                    pb.seekProgressDelta(-0.01)
            }

            Keys.onRightPressed: {
                if (enabled && pb)
                    pb.seekProgressDelta(0.01)
            }

            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.NoButton

                onWheel: {
                    if (timeSlider.enabled && wheel.angleDelta.y !== 0 && pb) {
                        pb.seekProgressDelta(wheel.angleDelta.y > 0 ? 0.005 : -0.005)
                        wheel.accepted = true
                    }
                }
            }

            ToolTip.visible: hovered || pressed
            ToolTip.text: pb ? pb.formatSeekTooltip(timeSlider.value) : ""
        }

        TimeDisplay {
            id: endTimeDisplay
            dateTime: pb && pb.endTime ? pb.endTime : null
            showTime: true
            showDate: playbackControls.hasValidTimeRange && pb && pb.playbackIsLongTerm
            timeColor: "white"
            dateColor: "#bdc3c7"
            timeFontSize: 12
            dateFontSize: 9
            timeFontBold: true
        }

        RowLayout {
            spacing: 5

            Text {
                text: "速度:"
                color: "white"
                font.pixelSize: 12
            }

            ComboBox {
                id: speedCombo
                model: pb ? pb.playbackSpeedLabels : []
                enabled: playbackControls.mapVehicleContextActive
                Layout.preferredWidth: 80

                ToolTip.visible: hovered
                ToolTip.text: pb && pb.playbackIsLongTerm
                             ? "调整播放速度 (长期数据可使用更高倍速)"
                             : "调整播放速度"

                onActivated: function (index) {
                    if (pb)
                        pb.setPlaybackSpeedFromLabelIndex(index)
                }
            }
        }
    }

    Connections {
        target: pb
        function onTimeRangeChanged() {
            if (!pb) return
            speedCombo.currentIndex = Math.min(pb.playbackSpeedDefaultIndex, speedCombo.count - 1)
            pb.setPlaybackSpeedFromLabelIndex(speedCombo.currentIndex)
        }
    }

    Connections {
        target: controller
        function onSelectedVehicleChanged() {
            if (!controller) return
            speedSyncTimer.restart()
        }
    }

    Timer {
        id: speedSyncTimer
        interval: 0
        repeat: false
        onTriggered: {
            if (!pb || speedCombo.count <= 0)
                return
            speedCombo.currentIndex = Math.min(pb.playbackSpeedDefaultIndex, speedCombo.count - 1)
            pb.setPlaybackSpeedFromLabelIndex(speedCombo.currentIndex)
        }
    }

    Component.onCompleted: {
        if (pb && speedCombo.count > 0) {
            speedCombo.currentIndex = Math.min(pb.playbackSpeedDefaultIndex, speedCombo.count - 1)
            pb.setPlaybackSpeedFromLabelIndex(speedCombo.currentIndex)
        }
    }
}
