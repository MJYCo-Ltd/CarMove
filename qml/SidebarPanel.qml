import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: sidebarPanel
    width: 60
    color: "#2c3e50"
    border.color: "#34495e"
    border.width: 1

    property string currentMode: "trajectory"

    signal modeChanged(string mode)
    signal searchRequested()
    signal businessFileOpenRequested()

    Column {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 10

        Text {
            text: "功能"
            color: "#ecf0f1"
            font.pixelSize: 12
            font.bold: true
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Rectangle {
            width: parent.width
            height: 1
            color: "#34495e"
        }

        // 业务 Excel 预览（位于轨迹之前）
        Button {
            id: businessButton
            width: parent.width - 10
            height: 50
            anchors.horizontalCenter: parent.horizontalCenter

            background: Rectangle {
                color: sidebarPanel.currentMode === "business" ? "#f39c12" : "#34495e"
                border.color: sidebarPanel.currentMode === "business" ? "#d68910" : "#2c3e50"
                border.width: 1
                radius: 4
            }

            contentItem: Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "📊"
                    font.pixelSize: 16
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "业务"
                    font.pixelSize: 10
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            onClicked: {
                const entering = sidebarPanel.currentMode !== "business"
                if (entering) {
                    sidebarPanel.currentMode = "business"
                    sidebarPanel.modeChanged("business")
                }
                sidebarPanel.businessFileOpenRequested()
            }

            ToolTip.visible: hovered
            ToolTip.text: "打开并预览 Excel 文件"
        }

        // 轨迹功能按钮
        Button {
            id: trajectoryButton
            width: parent.width - 10
            height: 50
            anchors.horizontalCenter: parent.horizontalCenter

            background: Rectangle {
                color: sidebarPanel.currentMode === "trajectory" ? "#3498db" : "#34495e"
                border.color: sidebarPanel.currentMode === "trajectory" ? "#2980b9" : "#2c3e50"
                border.width: 1
                radius: 4
            }

            contentItem: Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "🚗"
                    font.pixelSize: 16
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "轨迹"
                    font.pixelSize: 10
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            onClicked: {
                if (sidebarPanel.currentMode !== "trajectory") {
                    sidebarPanel.currentMode = "trajectory"
                    sidebarPanel.modeChanged("trajectory")
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: "车辆轨迹追踪"
        }

        // 卸油记录功能按钮
        Button {
            id: fuelButton
            width: parent.width - 10
            height: 50
            anchors.horizontalCenter: parent.horizontalCenter

            background: Rectangle {
                color: sidebarPanel.currentMode === "fuel" ? "#e74c3c" : "#34495e"
                border.color: sidebarPanel.currentMode === "fuel" ? "#c0392b" : "#2c3e50"
                border.width: 1
                radius: 4
            }

            contentItem: Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "⛽"
                    font.pixelSize: 16
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "卸油"
                    font.pixelSize: 10
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            onClicked: {
                if (sidebarPanel.currentMode !== "fuel") {
                    sidebarPanel.currentMode = "fuel"
                    sidebarPanel.modeChanged("fuel")
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: "卸油记录查看"
        }

        // 地点搜索功能按钮
        Button {
            id: searchButton
            width: parent.width - 10
            height: 50
            anchors.horizontalCenter: parent.horizontalCenter

            background: Rectangle {
                color: sidebarPanel.currentMode === "search" ? "#8e44ad" : "#34495e"
                border.color: sidebarPanel.currentMode === "search" ? "#7d3c98" : "#2c3e50"
                border.width: 1
                radius: 4
            }

            contentItem: Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "🔍"
                    font.pixelSize: 16
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "搜索"
                    font.pixelSize: 10
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            onClicked: {
                if (sidebarPanel.currentMode !== "search") {
                    sidebarPanel.currentMode = "search"
                    sidebarPanel.modeChanged("search")
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: "地点搜索"
        }

        // 驾车导航
        Button {
            id: navButton
            width: parent.width - 10
            height: 50
            anchors.horizontalCenter: parent.horizontalCenter

            background: Rectangle {
                color: sidebarPanel.currentMode === "nav" ? "#16a085" : "#34495e"
                border.color: sidebarPanel.currentMode === "nav" ? "#138d75" : "#2c3e50"
                border.width: 1
                radius: 4
            }

            contentItem: Column {
                anchors.centerIn: parent
                spacing: 2

                Text {
                    text: "🧭"
                    font.pixelSize: 16
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    text: "导航"
                    font.pixelSize: 10
                    color: "#ecf0f1"
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            onClicked: {
                if (sidebarPanel.currentMode !== "nav") {
                    sidebarPanel.currentMode = "nav"
                    sidebarPanel.modeChanged("nav")
                }
            }

            ToolTip.visible: hovered
            ToolTip.text: "天地图驾车路线"
        }
    }
}
