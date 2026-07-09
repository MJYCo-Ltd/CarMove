import QtQuick
import QtLocation
import QtPositioning

// 地图折线：parent 须为 null，由 Map.addMapItem 挂载
MapPolyline {
    id: root

    property color lineColor: "red"
    property int lineWidth: 5
    /// QGeoCoordinate 列表，对应 MapPolyline.path
    property var pathCoordinates: []

    line.color: lineColor
    line.width: lineWidth
    opacity: 0.8

    onPathCoordinatesChanged: _applyPath()
    onLineColorChanged: line.color = lineColor
    onLineWidthChanged: line.width = lineWidth

    Component.onCompleted: _applyPath()

    function _applyPath() {
        if (pathCoordinates && pathCoordinates.length >= 2)
            root.path = pathCoordinates
    }
}
