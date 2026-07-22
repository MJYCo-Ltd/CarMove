import QtQuick
import QtQuick.Controls

/// 与地图航线分段一致的时间轴：蓝色为有轨迹时间段，灰色为断档/跨天无轨迹区间
/// 轴总范围优先用车辆数据源全量起止；绿/红竖线为截取开始/结束
Item {
    id: root

    property var timelineController: null
    property bool interactive: false
    property int segmentCount: 0

    /// 时间轴总范围（数据库车辆最早/最晚）；无效时回退到已加载轨迹范围
    property var axisStartTime: null
    property var axisEndTime: null
    /// 截取开始/结束（与侧栏「轨迹截取」同步）
    property var clipStartTime: null
    property var clipEndTime: null
    property bool clipMarkersEnabled: false

    signal clipStartMoved(var dateTime)
    signal clipEndMoved(var dateTime)
    signal clipRangeCommitRequested()

    function dateToMs(value) {
        if (!value || typeof value.getTime !== "function")
            return 0
        var ms = value.getTime()
        return isNaN(ms) ? 0 : ms
    }

    function refreshSegments() {
        segmentCount = timelineController ? timelineController.trajectorySegmentCount() : 0
        updateHandlePosition()
        updateClipMarkers()
    }

    function trajectoryStartMs() {
        if (!timelineController)
            return 0
        return dateToMs(timelineController.trajectoryStartTime)
    }

    function trajectoryEndMs() {
        if (!timelineController)
            return 0
        return dateToMs(timelineController.trajectoryEndTime)
    }

    function axisStartMs() {
        var ms = dateToMs(axisStartTime)
        if (ms > 0)
            return ms
        return trajectoryStartMs()
    }

    function axisEndMs() {
        var ms = dateToMs(axisEndTime)
        if (ms > 0)
            return ms
        return trajectoryEndMs()
    }

    function totalRangeMs() {
        var start = axisStartMs()
        var end = axisEndMs()
        return end > start ? end - start : 0
    }

    function msToX(ms) {
        var total = totalRangeMs()
        if (total <= 0 || width <= 0)
            return 0
        return Math.max(0, Math.min(width, (ms - axisStartMs()) * width / total))
    }

    function xToMs(xPos) {
        var total = totalRangeMs()
        if (total <= 0 || width <= 0)
            return axisStartMs()
        var ratio = Math.max(0, Math.min(1, xPos / width))
        return axisStartMs() + ratio * total
    }

    function msToDate(ms) {
        return new Date(ms)
    }

    function segmentStartProgress(index) {
        var total = totalRangeMs()
        if (!timelineController || total <= 0 || index < 0 || index >= segmentCount)
            return 0
        var t = timelineController.trajectorySegmentStartTime(index)
        return t && t.getTime ? Math.max(0, Math.min(1, (t.getTime() - axisStartMs()) / total)) : 0
    }

    function segmentEndProgress(index) {
        var total = totalRangeMs()
        if (!timelineController || total <= 0 || index < 0 || index >= segmentCount)
            return 0
        var t = timelineController.trajectorySegmentEndTime(index)
        return t && t.getTime ? Math.max(0, Math.min(1, (t.getTime() - axisStartMs()) / total)) : 0
    }

    function segmentStartX(index) {
        return segmentStartProgress(index) * width
    }

    function segmentWidth(index) {
        var start = segmentStartProgress(index)
        var end = segmentEndProgress(index)
        if (end <= start)
            return 0
        return Math.max(2, (end - start) * width)
    }

    function xToSegmentAndProgress(xPos) {
        if (!timelineController || segmentCount <= 0 || width <= 0)
            return { index: -1, progress: 0 }

        xPos = Math.max(0, Math.min(xPos, width))
        for (var i = 0; i < segmentCount; ++i) {
            var sx = segmentStartX(i)
            var sw = segmentWidth(i)
            if (xPos >= sx && xPos <= sx + sw) {
                var progress = sw > 0 ? (xPos - sx) / sw : 0
                return { index: i, progress: Math.max(0, Math.min(1, progress)) }
            }
        }
        return { index: -1, progress: 0 }
    }

    function applySeekFromX(xPos) {
        var hit = xToSegmentAndProgress(xPos)
        if (hit.index >= 0)
            timelineController.seekTrajectorySegment(hit.index, hit.progress)
    }

    function updateHandlePosition() {
        if (!timelineController || segmentCount <= 0) {
            handle.visible = false
            return
        }
        var idx = timelineController.trajectoryActiveSegmentIndex()
        if (idx < 0) {
            handle.visible = false
            return
        }
        handle.visible = interactive
        var localProgress = timelineController.trajectorySegmentLocalProgress(idx)
        var cx = segmentStartX(idx) + localProgress * segmentWidth(idx)
        handle.x = Math.max(0, Math.min(width - handle.width, cx - handle.width / 2))
    }

    function updateClipMarkers() {
        var total = totalRangeMs()
        var canShow = root.clipMarkersEnabled && total > 0 && width > 0
                      && dateToMs(clipStartTime) > 0 && dateToMs(clipEndTime) > 0
        clipStartMarker.visible = canShow
        clipEndMarker.visible = canShow
        if (!canShow)
            return

        var startMs = Math.max(axisStartMs(), Math.min(axisEndMs(), dateToMs(clipStartTime)))
        var endMs = Math.max(axisStartMs(), Math.min(axisEndMs(), dateToMs(clipEndTime)))
        if (endMs < startMs) {
            var tmp = startMs
            startMs = endMs
            endMs = tmp
        }
        clipStartMarker.x = msToX(startMs) - clipStartMarker.width / 2
        clipEndMarker.x = msToX(endMs) - clipEndMarker.width / 2
    }

    function moveClipStartToX(xPos, commit) {
        var total = totalRangeMs()
        if (total <= 0)
            return
        var endMs = dateToMs(clipEndTime)
        if (endMs <= 0)
            endMs = axisEndMs()
        var minMs = axisStartMs()
        var maxMs = Math.max(minMs, endMs)
        var ms = Math.max(minMs, Math.min(maxMs, xToMs(xPos)))
        var dt = msToDate(ms)
        clipStartMoved(dt)
        if (commit)
            clipRangeCommitRequested()
    }

    function moveClipEndToX(xPos, commit) {
        var total = totalRangeMs()
        if (total <= 0)
            return
        var startMs = dateToMs(clipStartTime)
        if (startMs <= 0)
            startMs = axisStartMs()
        var minMs = startMs
        var maxMs = axisEndMs()
        var ms = Math.max(minMs, Math.min(maxMs, xToMs(xPos)))
        var dt = msToDate(ms)
        clipEndMoved(dt)
        if (commit)
            clipRangeCommitRequested()
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width
        height: 8
        radius: 4
        color: "#566573"
        opacity: 0.55
    }

    Repeater {
        model: root.segmentCount
        Rectangle {
            y: (parent.height - height) / 2
            x: root.segmentStartX(index)
            width: root.segmentWidth(index)
            height: 8
            radius: 4
            color: root.interactive ? "#3498db" : "#566573"
            opacity: root.interactive ? 1.0 : 0.55
        }
    }

    Rectangle {
        id: handle
        width: 14
        height: 14
        radius: 7
        y: (parent.height - height) / 2
        color: root.interactive ? "#ecf0f1" : "#95a5a6"
        border.color: root.interactive ? "#2c3e50" : "#7f8c8d"
        border.width: 2
        visible: false
        z: 5
    }

    Rectangle {
        id: clipStartMarker
        width: 3
        height: parent.height
        color: "#27ae60"
        visible: false
        z: 30

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: 10
            height: 10
            radius: 2
            color: "#27ae60"
            border.color: "#1e8449"
            border.width: 1
        }

        MouseArea {
            anchors.centerIn: parent
            width: 18
            height: parent.height
            enabled: root.clipMarkersEnabled
            cursorShape: Qt.SizeHorCursor
            preventStealing: true

            property real dragOriginMouseX: 0
            property real dragOriginMarkerCenterX: 0

            onPressed: function(mouse) {
                dragOriginMouseX = mapToItem(root, mouse.x, 0).x
                dragOriginMarkerCenterX = clipStartMarker.x + clipStartMarker.width / 2
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                var mouseX = mapToItem(root, mouse.x, 0).x
                var centerX = dragOriginMarkerCenterX + (mouseX - dragOriginMouseX)
                root.moveClipStartToX(centerX, false)
            }
            onReleased: function(mouse) {
                var mouseX = mapToItem(root, mouse.x, 0).x
                var centerX = dragOriginMarkerCenterX + (mouseX - dragOriginMouseX)
                root.moveClipStartToX(centerX, true)
            }
        }
    }

    Rectangle {
        id: clipEndMarker
        width: 3
        height: parent.height
        color: "#e74c3c"
        visible: false
        z: 30

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            width: 10
            height: 10
            radius: 2
            color: "#e74c3c"
            border.color: "#922b21"
            border.width: 1
        }

        MouseArea {
            anchors.centerIn: parent
            width: 18
            height: parent.height
            enabled: root.clipMarkersEnabled
            cursorShape: Qt.SizeHorCursor
            preventStealing: true

            property real dragOriginMouseX: 0
            property real dragOriginMarkerCenterX: 0

            onPressed: function(mouse) {
                dragOriginMouseX = mapToItem(root, mouse.x, 0).x
                dragOriginMarkerCenterX = clipEndMarker.x + clipEndMarker.width / 2
            }
            onPositionChanged: function(mouse) {
                if (!pressed)
                    return
                var mouseX = mapToItem(root, mouse.x, 0).x
                var centerX = dragOriginMarkerCenterX + (mouseX - dragOriginMouseX)
                root.moveClipEndToX(centerX, false)
            }
            onReleased: function(mouse) {
                var mouseX = mapToItem(root, mouse.x, 0).x
                var centerX = dragOriginMarkerCenterX + (mouseX - dragOriginMouseX)
                root.moveClipEndToX(centerX, true)
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive && root.segmentCount > 0
        hoverEnabled: true
        z: 1

        onPressed: function(mouse) {
            var hit = root.xToSegmentAndProgress(mouse.x)
            if (hit.index < 0)
                return
            timelineController.seekTrajectorySegment(hit.index, hit.progress)
        }
        onPositionChanged: function(mouse) {
            if (pressed)
                root.applySeekFromX(mouse.x)
        }
        onReleased: function(mouse) {
            root.applySeekFromX(mouse.x)
        }
        onWheel: function(wheel) {
            if (!timelineController)
                return
            var idx = timelineController.trajectoryActiveSegmentIndex()
            if (idx < 0)
                return
            var step = wheel.angleDelta.y > 0 ? 0.02 : -0.02
            var progress = timelineController.trajectorySegmentLocalProgress(idx) + step
            timelineController.seekTrajectorySegment(idx, progress)
            wheel.accepted = true
        }
    }

    Connections {
        target: timelineController
        function onTrajectorySegmentsChanged() { root.refreshSegments() }
        function onTrajectoryCurrentTimeChanged() { root.updateHandlePosition() }
        function onTrajectoryTimeRangeChanged() { root.refreshSegments() }
    }

    onTimelineControllerChanged: refreshSegments()
    onWidthChanged: {
        updateHandlePosition()
        updateClipMarkers()
    }
    onAxisStartTimeChanged: updateClipMarkers()
    onAxisEndTimeChanged: updateClipMarkers()
    onClipStartTimeChanged: updateClipMarkers()
    onClipEndTimeChanged: updateClipMarkers()
    onClipMarkersEnabledChanged: updateClipMarkers()
    Component.onCompleted: refreshSegments()
}
