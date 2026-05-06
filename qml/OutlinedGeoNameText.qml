import QtQuick

/// 地图上导航点名称：描边大字，可左/右对齐
Text {
    id: root

    property bool alignEnd: true
    property bool isStartStyle: true
    property int fontPixelSize: 14

    width: visible ? 200 : 0
    wrapMode: Text.WordWrap
    maximumLineCount: 2
    elide: Text.ElideRight
    font.pixelSize: fontPixelSize
    font.bold: true
    style: Text.Outline
    styleColor: "#1a1a1a"
    horizontalAlignment: alignEnd ? Text.AlignRight : Text.AlignLeft
    color: isStartStyle ? "#d5f5e3" : "#fadbd8"
}
