import QtQuick

/// 地名 / POI 描边文字（非 MapItem；地图上请用 MapPlacemark.qml，placemarkKind: "geoName"）
Text {
    id: root

    property bool alignEnd: true
    property int nameFontPixelSize: 14
    property color nameTextColor: "#ffffff"
    property color nameStrokeColor: "#1a1a1a"
    property bool nameStrokeEnabled: true

    width: visible ? 200 : 0
    wrapMode: Text.WordWrap
    maximumLineCount: 2
    elide: Text.ElideRight
    font.pixelSize: nameFontPixelSize
    font.bold: true
    color: nameTextColor
    style: nameStrokeEnabled ? Text.Outline : Text.PlainText
    styleColor: nameStrokeColor
    horizontalAlignment: alignEnd ? Text.AlignRight : Text.AlignLeft
}
