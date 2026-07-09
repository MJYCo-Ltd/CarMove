import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// 侧栏标题 + 可选说明（两行统一样式）
ColumnLayout {
    id: root
    spacing: 4

    property string title: ""
    property string subtitle: ""

    Layout.fillWidth: true

    Label {
        text: root.title
        font.pixelSize: 14
        font.bold: true
        color: "#2c3e50"
        Layout.fillWidth: true
        visible: root.title.length > 0
        wrapMode: Text.WordWrap
    }

    Label {
        text: root.subtitle
        font.pixelSize: 11
        color: "#7f8c8d"
        Layout.fillWidth: true
        visible: root.subtitle.length > 0
        wrapMode: Text.WordWrap
    }
}
