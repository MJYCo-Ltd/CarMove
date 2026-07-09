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
    opacity: 0.9
    z: 15

    onPathCoordinatesChanged: _applyPath()
    onLineColorChanged: line.color = lineColor
    onLineWidthChanged: line.width = lineWidth

    Component.onCompleted: _applyPath()

    function _field(value, name) {
        if (!value)
            return undefined
        var raw = value[name]
        return (typeof raw === "function") ? raw.call(value) : raw
    }

    function _asCoordinate(value) {
        if (!value)
            return null

        var lat = _field(value, "latitude")
        var lon = _field(value, "longitude")
        if (lat !== undefined && lon !== undefined)
            return QtPositioning.coordinate(Number(lat), Number(lon))

        var nested = value.coordinate
        if (nested) {
            lat = _field(nested, "latitude")
            lon = _field(nested, "longitude")
            if (lat !== undefined && lon !== undefined)
                return QtPositioning.coordinate(Number(lat), Number(lon))
        }
        return null
    }

    function _applyPath() {
        var convertedPath = []
        if (pathCoordinates && pathCoordinates.length >= 2) {
            for (var i = 0; i < pathCoordinates.length; ++i) {
                var coord = _asCoordinate(pathCoordinates[i])
                if (coord && coord.isValid)
                    convertedPath.push(coord)
            }
        }
        root.path = convertedPath
    }
}
