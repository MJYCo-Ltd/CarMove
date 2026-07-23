import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// 地名搜索结果单条卡片（名称/地址/坐标 + 操作按钮）
Rectangle {
    id: root

    property string resultName: ""
    property string resultAddress: ""
    property double resultLatitude: 0
    property double resultLongitude: 0
    property string actionButtonText: ""
    property string secondaryButtonText: ""

    signal actionTriggered()
    signal secondaryActionTriggered()

    implicitHeight: col.implicitHeight + 24
    color: "white"
    border.color: "#dce0e8"
    border.width: 1
    radius: 4

    Column {
        id: col
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            margins: 8
        }
        spacing: 3
        width: parent.width - 16

        Label {
            width: parent.width
            text: root.resultName
            font.bold: true
            font.pixelSize: 12
            color: "#2c3e50"
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            text: root.resultAddress
            font.pixelSize: 11
            color: "#7f8c8d"
            wrapMode: Text.WordWrap
            visible: root.resultAddress.length > 0
        }

        Label {
            width: parent.width
            text: root.resultLatitude.toFixed(6) + ", " + root.resultLongitude.toFixed(6)
            font.pixelSize: 10
            color: "#95a5a6"
        }

        RowLayout {
            width: parent.width
            spacing: 6

            Button {
                Layout.fillWidth: true
                text: root.actionButtonText
                height: 32
                font.pixelSize: 11
                onClicked: root.actionTriggered()
            }

            Button {
                visible: root.secondaryButtonText.length > 0
                text: root.secondaryButtonText
                height: 32
                font.pixelSize: 11
                Layout.fillWidth: true
                onClicked: root.secondaryActionTriggered()
            }
        }
    }
}
