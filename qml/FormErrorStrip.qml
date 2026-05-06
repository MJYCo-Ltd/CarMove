import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// 表单/检索错误提示条（红底）
Rectangle {
    id: root

    property alias text: errLabel.text

    Layout.fillWidth: true
    Layout.preferredHeight: visible ? Math.max(36, errLabel.implicitHeight + 16) : 0
    visible: errLabel.text.length > 0

    color: "#fdecea"
    border.color: "#e74c3c"
    border.width: 1
    radius: 4

    Label {
        id: errLabel
        anchors.fill: parent
        anchors.margins: 8
        text: ""
        font.pixelSize: 11
        color: "#c0392b"
        wrapMode: Text.WordWrap
    }
}
