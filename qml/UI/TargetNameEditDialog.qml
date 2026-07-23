import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Dialog {
    id: root

    title: qsTr("编辑目标名称")
    modal: true
    anchors.centerIn: parent
    width: 400
    padding: 16

    property string initialName: ""

    signal nameConfirmed(string name)

    onOpened: {
        nameField.text = initialName
        nameField.forceActiveFocus()
        nameField.selectAll()
    }

    background: Rectangle {
        color: "#ffffff"
        border.color: "#dcdde1"
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 8

        TextField {
            id: nameField
            Layout.fillWidth: true
            placeholderText: qsTr("请输入目标区域名称")
            selectByMouse: true
            maximumLength: 120
            Keys.onReturnPressed: root.accept()
        }
    }

    footer: DialogButtonBox {
        Button {
            text: qsTr("取消")
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: qsTr("确定")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: root.nameConfirmed(nameField.text.trim())
}
