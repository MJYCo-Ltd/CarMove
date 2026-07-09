import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
    id: root

    property string label: "文件夹"
    property string folderPath: ""
    property string placeholder: "未选择"

    spacing: 8

    Text {
        text: root.label
        Layout.preferredWidth: 72
    }

    Text {
        Layout.fillWidth: true
        text: root.folderPath.length > 0 ? root.folderPath : root.placeholder
        elide: Text.ElideMiddle
        color: root.folderPath.length > 0 ? "#2c3e50" : "#7f8c8d"
        font.pixelSize: 12
    }

    Button {
        text: "浏览"
        onClicked: root.browseRequested()
    }

    signal browseRequested()
}
