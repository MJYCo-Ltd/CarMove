import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: playbackControls
    /// 轨迹模式且已选择车辆时为 true（由 MainWindow 绑定）
    property bool mapVehicleContextActive: false
    visible: mapVehicleContextActive

    color: "#34495e"
    border.color: "#2c3e50"
    opacity: timelineEnabled ? 1.0 : 0.42

    readonly property var pb: controller && controller.playback ? controller.playback : null
    property int segmentCount: 0
    property int activeSegmentIndex: -1
    readonly property bool timelineEnabled: segmentCount > 0
    readonly property bool showDateOnTimeline: pb && pb.playbackSpansMultipleDays

    readonly property color labelTimeColor: timelineEnabled ? "white" : "#7f8c8d"
    readonly property color labelDateColor: timelineEnabled ? "#bdc3c7" : "#636e72"

    function updateSegmentState() {
        if (!controller) {
            segmentCount = 0
            activeSegmentIndex = -1
            return
        }
        segmentCount = controller.playbackSegmentCount()
        activeSegmentIndex = controller.playbackActiveSegmentIndex()
    }

    Connections {
        target: controller
        function onTrajectoryLoaded() { playbackControls.updateSegmentState() }
        function onTrajectoryConverted() { playbackControls.updateSegmentState() }
        function onSelectedVehicleChanged() {
            segmentCount = 0
            activeSegmentIndex = -1
        }
        function onPlaybackSegmentsChanged() { playbackControls.updateSegmentState() }
    }

    Connections {
        target: playbackControls.pb
        enabled: playbackControls.pb !== null
        function onCurrentTimeChanged() { playbackControls.updateSegmentState() }
        function onTimeRangeChanged() { playbackControls.updateSegmentState() }
    }

    Component.onCompleted: updateSegmentState()

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        TimeDisplay {
            id: currentTimeDisplay
            Layout.preferredWidth: playbackControls.showDateOnTimeline ? 92 : 72
            dateTime: pb && pb.currentTime ? pb.currentTime : null
            showTime: true
            showDate: playbackControls.showDateOnTimeline
            dateAboveTime: playbackControls.showDateOnTimeline
            dateFormat: "yyyy-MM-dd"
            timeColor: playbackControls.labelTimeColor
            dateColor: playbackControls.labelDateColor
            timeFontSize: 12
            dateFontSize: 10
            timeFontBold: true
        }

        SegmentTimeline {
            id: segmentTimeline
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            timelineController: controller
            interactive: playbackControls.timelineEnabled
        }

        TimeDisplay {
            id: segmentEndDisplay
            Layout.preferredWidth: playbackControls.showDateOnTimeline ? 92 : 72
            dateTime: {
                if (!controller || playbackControls.activeSegmentIndex < 0)
                    return pb && pb.endTime ? pb.endTime : null
                return controller.playbackSegmentEndTime(playbackControls.activeSegmentIndex)
            }
            showTime: true
            showDate: playbackControls.showDateOnTimeline
            dateAboveTime: playbackControls.showDateOnTimeline
            dateFormat: "yyyy-MM-dd"
            timeColor: playbackControls.labelTimeColor
            dateColor: playbackControls.labelDateColor
            timeFontSize: 12
            dateFontSize: 10
            timeFontBold: true
        }
    }
}
