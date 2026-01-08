import QtQuick
import QtQuick.Controls

/**
 * StyledComboBox - 可重用的样式化下拉框组件
 * 
 * 提供统一的 ComboBox 样式
 */
ComboBox {
    id: styledComboBox
    
    property bool hasError: false
    property color errorColor: "#dc2626"
    property color normalBorderColor: "#ced4da"
    property color hoverBorderColor: "#2196f3"
    property color normalBackgroundColor: "#ffffff"
    property color hoverBackgroundColor: "#f8f9fa"
    property int fontSize: 13
    
    background: Rectangle {
        color: hasError ? "#fff5f5" : 
               (styledComboBox.hovered ? hoverBackgroundColor : normalBackgroundColor)
        border.color: hasError ? errorColor : 
                      (styledComboBox.hovered ? hoverBorderColor : normalBorderColor)
        border.width: hasError ? 2 : 1
        radius: 6
    }
    
    contentItem: Text {
        leftPadding: 12
        rightPadding: styledComboBox.indicator.width + styledComboBox.spacing
        text: styledComboBox.displayText
        font.pixelSize: fontSize
        color: "#212529"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    
    popup.background: Rectangle {
        color: "#ffffff"
        border.color: "#dee2e6"
        border.width: 1
        radius: 6
    }
}

