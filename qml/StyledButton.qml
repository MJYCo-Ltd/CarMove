import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * StyledButton - 可重用的样式化按钮组件
 * 
 * 提供统一的按钮样式，支持多种颜色主题和状态
 */
Button {
    id: styledButton
    
    // 颜色主题: "primary", "secondary", "danger", "warning", "success", "default"
    property string buttonTheme: "primary"
    property bool showBorder: false
    property color customColor: "transparent"
    property color customHoverColor: "transparent"
    property color customPressedColor: "transparent"
    property color customDisabledColor: "#b0bec5"
    
    // 尺寸属性
    property int buttonWidth: 100
    property int buttonHeight: 36
    property int fontSize: 13
    
    // 计算颜色
    readonly property color normalColor: {
        if (customColor !== "transparent") return customColor
        switch (buttonTheme) {
            case "primary": return "#2196f3"
            case "secondary": return "#6c757d"
            case "danger": return "#f44336"
            case "warning": return "#ff9800"
            case "success": return "#4caf50"
            case "default": return "#9e9e9e"
            default: return "#2196f3"
        }
    }
    
    readonly property color hoverColor: {
        if (customHoverColor !== "transparent") return customHoverColor
        switch (buttonTheme) {
            case "primary": return "#2196f3"
            case "secondary": return "#868e96"
            case "danger": return "#d32f2f"
            case "warning": return "#f57c00"
            case "success": return "#388e3c"
            case "default": return "#9e9e9e"
            default: return "#2196f3"
        }
    }
    
    readonly property color pressedColor: {
        if (customPressedColor !== "transparent") return customPressedColor
        switch (buttonTheme) {
            case "primary": return "#1976d2"
            case "secondary": return "#6c757d"
            case "danger": return "#c62828"
            case "warning": return "#e65100"
            case "success": return "#2e7d32"
            case "default": return "#757575"
            default: return "#1976d2"
        }
    }
    
    Layout.preferredWidth: buttonWidth
    Layout.preferredHeight: buttonHeight
    implicitWidth: buttonWidth
    implicitHeight: buttonHeight
    
    background: Rectangle {
        color: styledButton.enabled ? 
               (styledButton.pressed ? styledButton.pressedColor : 
                styledButton.hovered ? styledButton.hoverColor : 
                styledButton.normalColor) : 
               styledButton.customDisabledColor
        radius: 6
        border.width: showBorder ? 1 : 0
        border.color: showBorder ? Qt.darker(parent.color, 1.2) : "transparent"
    }
    
    contentItem: Text {
        text: styledButton.text
        font.pixelSize: fontSize
        font.bold: true
        color: "white"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}

