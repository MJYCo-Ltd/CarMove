import QtQuick

/// 地图上车牌样式小块（非 MapItem，嵌入 MapQuickItem / VehicleMarker 使用）
Rectangle {
    id: root

    property string plateText: ""
    property color plateBackgroundColor: "yellow"
    property color plateBorderColor: "white"
    property color plateTextColor: "black"
    property color plateTextStyleColor: "white"
    property int plateBorderWidth: 1
    property int fontPixelSize: 15
    property bool fontBold: true

    color: plateBackgroundColor
    border.color: plateBorderColor
    border.width: plateBorderWidth
    radius: 3
    width: plateLabel.width + 8
    height: plateLabel.height + 4

    Text {
        id: plateLabel
        anchors.centerIn: parent
        text: root.plateText
        font.pixelSize: root.fontPixelSize
        font.bold: root.fontBold
        color: root.plateTextColor
        style: Text.Outline
        styleColor: root.plateTextStyleColor
    }
}
