import QtQuick
import QtLocation
import QtPositioning

// 车辆标记组件 - 从MapDisplay.qml提取
MapQuickItem {
    id: marker
    
    // 公共属性
    property string plateNumber: ""
    property int direction: 0
    property double speed: 0
    property string vehicleColor: "yellow"
    property int visitDays: 0  // 到达目标区域的天数
    
    // 信号
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
            
            // 车头指示箭头
            Rectangle {
                width: 8
                height: 2
                color: "white"
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -6
                radius: 1
            }
            
            // 速度指示器
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
        
        // 到达天数标识（右上角）
        Rectangle {
            id: visitDaysIndicator
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
        
        // 车牌号标签
        Rectangle {
            width: plateText.width + 8
            height: plateText.height + 4
            color: "yellow"  // 使用车辆颜色作为背景色
            border.color: "white"       // 白色边框以确保可见性
            border.width: 1
            radius: 3
            anchors.top: vehicleIcon.bottom
            anchors.horizontalCenter: vehicleIcon.horizontalCenter
            anchors.topMargin: 2
            
            Text {
                id: plateText
                text: marker.plateNumber
                font.pixelSize: 11
                color: "black"  // 白色文字以确保在彩色背景上的可读性
                style: Text.Outline
                styleColor: "white"
                anchors.centerIn: parent
            }
        }
    }
    
    // 点击事件
    MouseArea {
        anchors.fill: parent
        onClicked: {
            marker.vehicleClicked(marker.plateNumber, marker.speed, marker.direction)
        }
        
        // 悬停效果
        hoverEnabled: true
        onEntered: {
            vehicleIcon.scale = 1.2
        }
        onExited: {
            vehicleIcon.scale = 1.0
        }
    }
}