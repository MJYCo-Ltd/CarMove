import QtQuick
import QtQuick.Controls

Rectangle {
    id: vehicleInfoCard
    
    // 基本属性
    property string plateNumber: ""
    property string vehicleInfo: ""
    property double speed: 0
    property int direction: 0
    property bool isSelected: false
    property bool isClickable: true
    
    // 颜色属性
    property color selectedColor: "#3498db"
    property color hoverColor: "#ecf0f1"
    property color normalColor: "transparent"
    property color borderColor: "#bdc3c7"
    property color textColor: "black"
    property color selectedTextColor: "white"
    property color infoTextColor: "#7f8c8d"
    
    // 布局属性
    property string layoutMode: "horizontal" // "horizontal" or "vertical"
    property bool showBorder: true
    property bool showSelectionIndicator: true
    
    // 信号
    signal clicked()
    signal doubleClicked()
    
    // 外观
    color: isSelected ? selectedColor : (mouseArea.containsMouse ? hoverColor : normalColor)
    border.color: showBorder ? borderColor : "transparent"
    border.width: showBorder ? 1 : 0
    radius: 4
    
    // 水平布局 - 绝对不会溢出的版本
    Item {
        visible: layoutMode === "horizontal"
        anchors.fill: parent
        anchors.margins: 4
        
        // 选择指示器
        Rectangle {
            id: indicator
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: showSelectionIndicator && isSelected ? 4 : 0
            color: selectedColor
            visible: showSelectionIndicator && isSelected
        }
        
        // 文本区域 - 绝对约束在边界内
        Item {
            anchors.left: indicator.right
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            
            Column {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                spacing: 2
                
                // 车牌号文本
                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    text: plateNumber
                    font.bold: true
                    font.pixelSize: 14
                    color: isSelected ? selectedTextColor : textColor
                    elide: Text.ElideRight
                    wrapMode: Text.NoWrap
                    clip: true
                }
            }
        }
    }

    // 垂直布局（用于弹窗信息）
    Column {
        visible: layoutMode === "vertical"
        anchors.fill: parent
        anchors.margins: 10
        spacing: 5

        Text {
            text: "车辆信息"
            font.bold: true
            font.pixelSize: 14
            color: textColor
        }

        Text {
            text: "车牌号: " + plateNumber
            font.pixelSize: 12
            color: textColor
        }

        Text {
            text: "速度: " + speed.toFixed(1) + " km/h"
            font.pixelSize: 12
            color: textColor
            visible: speed > 0
        }

        Text {
            text: "方向: " + direction + "°"
            font.pixelSize: 12
            color: textColor
            visible: direction >= 0
        }

        Text {
            text: vehicleInfo
            font.pixelSize: 12
            color: infoTextColor
            visible: vehicleInfo !== ""
            wrapMode: Text.WordWrap
            width: parent.width
        }
    }
    
    // 鼠标交互
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: isClickable
        enabled: isClickable
        
        onClicked: vehicleInfoCard.clicked()
        onDoubleClicked: vehicleInfoCard.doubleClicked()
    }
    
    // 平滑颜色过渡
    Behavior on color {
        ColorAnimation { duration: 200 }
    }
}
