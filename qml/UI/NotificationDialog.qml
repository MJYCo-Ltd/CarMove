import QtQuick
import QtQuick.Controls

Dialog {
    id: notificationDialog
    
    // Public properties for customization
    property string dialogTitle: "通知"
    property string message: ""
    property string buttonText: "确定"
    property color messageColor: "#2c3e50"
    property color backgroundColor: "white"
    property bool autoHide: false
    property int autoHideDelay: 5000
    
    // Dialog configuration
    title: dialogTitle
    modal: true
    anchors.centerIn: parent
    width: 350
    height: 200
    
    // Auto-hide timer
    Timer {
        id: autoHideTimer
        interval: notificationDialog.autoHideDelay
        onTriggered: notificationDialog.close()
    }
    
    // Content
    contentItem: Column {
        spacing: 10
        padding: 20
        
        Text {
            text: notificationDialog.message
            wrapMode: Text.WordWrap
            width: 300
            color: notificationDialog.messageColor
        }
        
        Button {
            text: notificationDialog.buttonText
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: notificationDialog.close()
        }
    }
    
    // Background
    background: Rectangle {
        color: notificationDialog.backgroundColor
        border.color: "#bdc3c7"
        border.width: 1
        radius: 8
    }
    
    // Auto-hide functionality
    onOpened: {
        if (autoHide) {
            autoHideTimer.restart()
        }
    }
    
    onClosed: {
        autoHideTimer.stop()
    }
    
    // Public methods
    function showMessage(title, msg, autoHideEnabled) {
        dialogTitle = title || "通知"
        message = msg || ""
        autoHide = autoHideEnabled || false
        open()
    }
    
    function showError(msg) {
        dialogTitle = "错误"
        message = msg || ""
        messageColor = "#e74c3c"
        autoHide = false
        open()
    }
    
    function showSuccess(msg) {
        dialogTitle = "成功"
        message = msg || ""
        messageColor = "#27ae60"
        autoHide = true
        open()
    }
}