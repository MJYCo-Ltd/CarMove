import QtQuick
import QtQuick.Controls

Rectangle {
    id: statusButton
    
    // Public properties for customization
    property string iconText: "📍"
    property color buttonColor: "#3498db"
    property color hoverColor: "#2980b9"
    property string tooltipText: ""
    property int buttonSize: 50
    
    // Signals
    signal clicked()
    
    // Button appearance
    width: buttonSize
    height: buttonSize
    color: buttonColor
    radius: buttonSize / 2
    border.color: "white"
    border.width: 2
    z: 1000
    
    // Icon
    Text {
        text: statusButton.iconText
        font.pixelSize: 24
        color: "white"
        anchors.centerIn: parent
    }
    
    // Mouse interaction
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        
        onClicked: {
            statusButton.clicked()
        }
        
        onEntered: {
            parent.color = statusButton.hoverColor
            parent.scale = 1.1
        }
        
        onExited: {
            parent.color = statusButton.buttonColor
            parent.scale = 1.0
        }
    }
    
    // Hover animations
    Behavior on color {
        ColorAnimation { duration: 200 }
    }
    
    Behavior on scale {
        NumberAnimation { duration: 200; easing.type: Easing.OutQuad }
    }
    
    // Tooltip
    ToolTip.visible: tooltipText !== "" && hovered
    ToolTip.text: tooltipText
    
    property bool hovered: false
    
    // Track hover state for tooltip
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        
        onEntered: statusButton.hovered = true
        onExited: statusButton.hovered = false
    }
}