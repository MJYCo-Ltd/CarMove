import QtQuick
import QtLocation
import QtPositioning

// 车辆标记组件（独立使用）；地图层请用 MapPlacemark placemarkKind: "vehicle"
MapQuickItem {
    id: marker
    
    property string plateNumber: ""
    property int direction: 0
    property double speed: 0
    property string vehicleColor: "yellow"
    property int visitDays: 0
    property color plateBackgroundColor: "yellow"
    property color plateBorderColor: "white"
    property color plateTextColor: "black"
    property color plateTextStyleColor: "white"

    signal vehicleClicked(string plateNumber, double speed, int direction)
    
    coordinate: QtPositioning.coordinate(0, 0)
    anchorPoint.x: vehicleIcon.width / 2
    anchorPoint.y: vehicleIcon.height / 2
    
    sourceItem: Item {
        width: 40
        height: 50
        
        Rectangle {
            id: vehicleIcon
            width: 24
            height: 24
            color: marker.vehicleColor
            radius: 12
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -8
            rotation: marker.direction
            border.color: "white"
            border.width: 2
            
            Rectangle {
                width: 8
                height: 2
                color: "white"
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -6
                radius: 1
            }
            
            Rectangle {
                width: 4
                height: 4
                color: marker.speed > 0 ? "#27ae60" : "#e74c3c"
                radius: 2
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -2
                anchors.topMargin: -2
            }
        }
        
        Rectangle {
            width: visitDaysText.width + 6
            height: visitDaysText.height + 4
            color: "#e74c3c"
            border.color: "white"
            border.width: 1
            radius: 8
            visible: marker.visitDays > 0
            anchors.right: vehicleIcon.right
            anchors.top: vehicleIcon.top
            anchors.rightMargin: -8
            anchors.topMargin: -8
            z: 10
            
            Text {
                id: visitDaysText
                text: marker.visitDays.toString()
                font.pixelSize: 10
                font.bold: true
                color: "white"
                anchors.centerIn: parent
            }
        }
        
        MapPlateBadge {
            anchors.top: vehicleIcon.bottom
            anchors.horizontalCenter: vehicleIcon.horizontalCenter
            anchors.topMargin: 2
            plateText: marker.plateNumber
            plateBackgroundColor: marker.plateBackgroundColor
            plateBorderColor: marker.plateBorderColor
            plateTextColor: marker.plateTextColor
            plateTextStyleColor: marker.plateTextStyleColor
            fontPixelSize: 11
            fontBold: false
        }
    }
    
    MouseArea {
        anchors.fill: parent
        onClicked: marker.vehicleClicked(marker.plateNumber, marker.speed, marker.direction)
        hoverEnabled: true
        onEntered: { vehicleIcon.scale = 1.2 }
        onExited: { vehicleIcon.scale = 1.0 }
    }
}
