import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// 左侧功能面板统一底：灰底 + 边框 + 内边距 ColumnLayout
Rectangle {
    id: root
    color: "#f0f0f0"
    border.color: "#ccc"

    property int columnSpacing: 8

    default property alias content: column.data

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.margins: 10
        spacing: root.columnSpacing
    }
}
