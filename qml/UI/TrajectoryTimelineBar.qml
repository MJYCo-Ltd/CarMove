import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: trajectoryTimelineBar
    /// 轨迹模式且已选择车辆时为 true（由 MainWindow 绑定）
    property bool mapVehicleContextActive: false
    visible: mapVehicleContextActive

    color: "#34495e"
    border.color: "#2c3e50"
    opacity: timelineEnabled ? 1.0 : 0.42

    property int segmentCount: 0
    property int activeSegmentIndex: -1
    readonly property bool timelineEnabled: segmentCount > 0
    readonly property bool showDateOnTimeline: controller && controller.trajectorySpansMultipleDays

    readonly property color labelTimeColor: timelineEnabled ? "white" : "#7f8c8d"
    readonly property color labelDateColor: timelineEnabled ? "#bdc3c7" : "#636e72"

    function updateSegmentState() {
        if (!controller) {
            segmentCount = 0
            activeSegmentIndex = -1
            return
        }
        segmentCount = controller.trajectorySegmentCount()
        activeSegmentIndex = controller.trajectoryActiveSegmentIndex()
    }

    Connections {
        target: controller
        function onTrajectoryLoaded() { trajectoryTimelineBar.updateSegmentState() }
        function onTrajectoryConverted() { trajectoryTimelineBar.updateSegmentState() }
        function onSelectedVehicleChanged() {
            segmentCount = 0
            activeSegmentIndex = -1
        }
        function onTrajectorySegmentsChanged() { trajectoryTimelineBar.updateSegmentState() }
        function onTrajectoryCurrentTimeChanged() { trajectoryTimelineBar.updateSegmentState() }
        function onTrajectoryTimeRangeChanged() { trajectoryTimelineBar.updateSegmentState() }
    }

    Component.onCompleted: updateSegmentState()

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 12

        TimeDisplay {
            id: currentTimeDisplay
            Layout.preferredWidth: trajectoryTimelineBar.showDateOnTimeline ? 92 : 72
            dateTime: controller ? controller.trajectoryCurrentTime : null
            showTime: true
            showDate: trajectoryTimelineBar.showDateOnTimeline
            dateAboveTime: trajectoryTimelineBar.showDateOnTimeline
            dateFormat: "yyyy-MM-dd"
            timeColor: trajectoryTimelineBar.labelTimeColor
            dateColor: trajectoryTimelineBar.labelDateColor
            timeFontSize: 12
            dateFontSize: 10
            timeFontBold: true
        }

        SegmentTimeline {
            id: segmentTimeline
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            timelineController: controller
            interactive: trajectoryTimelineBar.timelineEnabled
        }

        TimeDisplay {
            id: segmentEndDisplay
            Layout.preferredWidth: trajectoryTimelineBar.showDateOnTimeline ? 92 : 72
            dateTime: {
                if (!controller || trajectoryTimelineBar.activeSegmentIndex < 0)
                    return controller ? controller.trajectoryEndTime : null
                return controller.trajectorySegmentEndTime(trajectoryTimelineBar.activeSegmentIndex)
            }
            showTime: true
            showDate: trajectoryTimelineBar.showDateOnTimeline
            dateAboveTime: trajectoryTimelineBar.showDateOnTimeline
            dateFormat: "yyyy-MM-dd"
            timeColor: trajectoryTimelineBar.labelTimeColor
            dateColor: trajectoryTimelineBar.labelDateColor
            timeFontSize: 12
            dateFontSize: 10
            timeFontBold: true
        }
    }
}
