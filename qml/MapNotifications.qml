import QtQuick

// 地图通知组件 - 从MapDisplay.qml提取
Item {
    id: mapNotifications
    
    // 公共函数
    function showScreenshotNotification(fileName) {
        screenshotNotification.fileName = fileName
        screenshotNotification.visible = true
        screenshotNotificationTimer.restart()
    }
    
    function showMapTypeNotification(typeName, typeDesc) {
        mapTypeNotification.typeName = typeName
        mapTypeNotification.typeDesc = typeDesc
        mapTypeNotification.visible = true
        mapTypeNotificationTimer.restart()
    }

    function showErrorNotification(message) {
        errorNotification.message = message
        errorNotification.visible = true
        errorNotificationTimer.restart()
    }

    // 错误提示（如地理编码失败）
    Rectangle {
        id: errorNotification
        width: 320
        height: 70
        color: "#e74c3c"
        border.color: "white"
        border.width: 2
        radius: 8
        visible: false
        z: 1001
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 20

        property string message: ""

        Text {
            anchors.fill: parent
            anchors.margins: 10
            text: errorNotification.message
            font.pixelSize: 13
            color: "white"
            wrapMode: Text.WordWrap
            verticalAlignment: Text.AlignVCenter
        }

        Timer {
            id: errorNotificationTimer
            interval: 4000
            onTriggered: errorNotification.visible = false
        }

        MouseArea {
            anchors.fill: parent
            onClicked: errorNotification.visible = false
        }
    }
    
    // 截屏成功通知
    Rectangle {
        id: screenshotNotification
        width: 300
        height: 80
        color: "#2ecc71"
        border.color: "white"
        border.width: 2
        radius: 8
        visible: false
        z: 1001
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 20
        
        property string fileName: ""
        
        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            
            Text {
                text: "截屏成功！"
                font.bold: true
                font.pixelSize: 14
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            Text {
                text: "已保存为: " + screenshotNotification.fileName
                font.pixelSize: 12
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
                wrapMode: Text.WordWrap
                width: parent.width - 20
            }
        }
        
        // 自动隐藏定时器
        Timer {
            id: screenshotNotificationTimer
            interval: 3000
            onTriggered: screenshotNotification.visible = false
        }
        
        // 点击关闭
        MouseArea {
            anchors.fill: parent
            onClicked: screenshotNotification.visible = false
        }
    }
    
    // 地图类型切换通知
    Rectangle {
        id: mapTypeNotification
        width: 320
        height: 80
        color: "#9b59b6"
        border.color: "white"
        border.width: 2
        radius: 8
        visible: false
        z: 1001
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 110  // 位置稍微下移，避免与截屏通知重叠
        
        property string typeName: ""
        property string typeDesc: ""
        
        Column {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 5
            
            Text {
                text: "地图类型已切换"
                font.bold: true
                font.pixelSize: 14
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            
            Text {
                text: mapTypeNotification.typeName
                font.pixelSize: 12
                font.bold: true
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
                wrapMode: Text.WordWrap
                width: parent.width - 20
            }
            
            Text {
                text: mapTypeNotification.typeDesc
                font.pixelSize: 10
                color: "white"
                anchors.horizontalCenter: parent.horizontalCenter
                wrapMode: Text.WordWrap
                width: parent.width - 20
                visible: mapTypeNotification.typeDesc !== ""
            }
        }
        
        // 自动隐藏定时器
        Timer {
            id: mapTypeNotificationTimer
            interval: 2500
            onTriggered: mapTypeNotification.visible = false
        }
        
        // 点击关闭
        MouseArea {
            anchors.fill: parent
            onClicked: mapTypeNotification.visible = false
        }
    }
}