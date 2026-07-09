import QtQuick

/// 导航起/终点旁一侧：车牌(MapPlateBadge) + 地名(MapGeoNameLabel)，避免 MapVehicleLayer 左右列重复
Column {
    id: root
    spacing: 4
    width: 200

    property bool alignNameEnd: true
    property bool plateAlignRight: true
    property string displayName: ""
    property color nameTextColor: "#d5f5e3"
    property color nameStrokeColor: "#1a1a1a"
    property int nameFontPixelSize: 12
    property string plateNumber: ""
    property bool showPlate: false

    Item {
        width: parent.width
        height: (root.showPlate && root.plateNumber.length > 0) ? plateBadge.height : 0
        visible: root.showPlate && root.plateNumber.length > 0

        MapPlateBadge {
            id: plateBadge
            x: root.plateAlignRight ? (parent.width - width) : 0
            y: 0
            plateText: root.plateNumber
        }
    }

    MapGeoNameLabel {
        width: 200
        alignEnd: root.alignNameEnd
        nameFontPixelSize: root.nameFontPixelSize
        nameTextColor: root.nameTextColor
        nameStrokeColor: root.nameStrokeColor
        text: root.displayName
    }
}
