import QtQuick
import QtLocation
import QtPositioning

// 地图折线：z 勿用负值，部分插件会把折线画在底图之下
MapPolyline {
    id: root

    property color lineColor: "red"
    property int lineWidth: 5
    /// 经纬度序列：元素可为 { latitude, longitude }、带 coordinate 字段的对象或 coordinate
    property var coordinateSequence: []

    line.color: lineColor
    line.width: lineWidth
    opacity: 0.8

    onCoordinateSequenceChanged: _syncPath()

    Component.onCompleted: _syncPath()

    function _syncPath() {
        var pts = coordinateSequence
        if (!pts || pts.length === 0) {
            root.path = []
            return
        }
        if (typeof controller !== "undefined" && controller)
            root.path = controller.trajectoryPolylinePath(pts)
        else
            root.path = []
    }
}
