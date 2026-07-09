import QtQuick
import QtQuick.Controls

/// 与地图航线分段一致的时间轴：蓝色为有轨迹时间段，灰色为断档/跨天无轨迹区间
Item {
    id: root

    property var timelineController: null
    property bool interactive: false
    property int segmentCount: 0

    function refreshSegments() {
        segmentCount = timelineController ? timelineController.playbackSegmentCount() : 0
        updateHandlePosition()
    }

    function playbackStartMs() {
        if (!timelineController || !timelineController.playback)
            return 0
        var t = timelineController.playback.startTime
        if (!t || typeof t.getTime !== "function")
            return 0
        var ms = t.getTime()
        return isNaN(ms) ? 0 : ms
    }

    function playbackEndMs() {
        if (!timelineController || !timelineController.playback)
            return 0
        var t = timelineController.playback.endTime
        if (!t || typeof t.getTime !== "function")
            return 0
        var ms = t.getTime()
        return isNaN(ms) ? 0 : ms
    }

    function totalRangeMs() {
        var start = playbackStartMs()
        var end = playbackEndMs()
        return end > start ? end - start : 0
    }

    function segmentStartProgress(index) {
        var total = totalRangeMs()
        if (!timelineController || total <= 0 || index < 0 || index >= segmentCount)
            return 0
        var t = timelineController.playbackSegmentStartTime(index)
        return t && t.getTime ? Math.max(0, Math.min(1, (t.getTime() - playbackStartMs()) / total)) : 0
    }

    function segmentEndProgress(index) {
        var total = totalRangeMs()
        if (!timelineController || total <= 0 || index < 0 || index >= segmentCount)
            return 0
        var t = timelineController.playbackSegmentEndTime(index)
        return t && t.getTime ? Math.max(0, Math.min(1, (t.getTime() - playbackStartMs()) / total)) : 0
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
            timelineController.seekPlaybackSegment(hit.index, hit.progress)
    }

    function updateHandlePosition() {
        if (!timelineController || segmentCount <= 0) {
            handle.visible = false
            return
        }
        var idx = timelineController.playbackActiveSegmentIndex()
        if (idx < 0) {
            handle.visible = false
            return
        }
        handle.visible = interactive
        var localProgress = timelineController.playbackSegmentLocalProgress(idx)
        var cx = segmentStartX(idx) + localProgress * segmentWidth(idx)
        handle.x = Math.max(0, Math.min(width - handle.width, cx - handle.width / 2))
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
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive && root.segmentCount > 0
        hoverEnabled: true

        onPressed: function(mouse) {
            var hit = root.xToSegmentAndProgress(mouse.x)
            if (hit.index < 0)
                return
            if (timelineController && timelineController.playback)
                timelineController.playback.setDraggingMode(true)
            timelineController.seekPlaybackSegment(hit.index, hit.progress)
        }
        onPositionChanged: function(mouse) {
            if (pressed)
                root.applySeekFromX(mouse.x)
        }
        onReleased: function(mouse) {
            root.applySeekFromX(mouse.x)
            if (timelineController && timelineController.playback)
                timelineController.playback.setDraggingMode(false)
        }
        onWheel: function(wheel) {
            if (!timelineController)
                return
            var idx = timelineController.playbackActiveSegmentIndex()
            if (idx < 0)
                return
            var step = wheel.angleDelta.y > 0 ? 0.02 : -0.02
            var progress = timelineController.playbackSegmentLocalProgress(idx) + step
            timelineController.seekPlaybackSegment(idx, progress)
            wheel.accepted = true
        }
    }

    Connections {
        target: timelineController
        function onPlaybackSegmentsChanged() { root.refreshSegments() }
    }

    Connections {
        target: timelineController && timelineController.playback ? timelineController.playback : null
        function onCurrentTimeChanged() { root.updateHandlePosition() }
        function onProgressChanged() { root.updateHandlePosition() }
        function onTimeRangeChanged() { root.refreshSegments() }
    }

    onTimelineControllerChanged: refreshSegments()
    onWidthChanged: updateHandlePosition()
    Component.onCompleted: refreshSegments()
}
