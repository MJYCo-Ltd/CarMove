import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: importDialog

    required property var excelModel
    property var configManager: null
    property string trajectoryFolderPath: ""

    title: "导入轨迹到 PostGIS 数据库"
    modal: true
    anchors.centerIn: parent
    width: 480
    padding: 16

    readonly property bool canImport: trajectoryFolderPath.length > 0
                                   && configManager
                                   && !excelModel.loading

    FolderDialog {
        id: trajectoryFolderDialog
        title: "选择轨迹 Excel 文件夹"
        onAccepted: importDialog.trajectoryFolderPath = selectedFolder.toString()
    }

    function folderDisplayPath(folderUrl) {
        if (!folderUrl || folderUrl.length === 0)
            return "未选择"
        const path = folderUrl.toString()
        if (path.startsWith("file:///"))
            return decodeURIComponent(path.substring(8))
        return path
    }

    function dbSummary() {
        if (!configManager)
            return "未加载配置（请从轨迹面板设置 PostGIS 连接）"
        return configManager.dbUser + "@" + configManager.dbHost + ":"
               + configManager.dbPort + "/" + configManager.dbName
               + "  schema=" + configManager.dbSchema
    }

    background: Rectangle {
        color: "#ffffff"
        border.color: "#dcdde1"
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 12

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#2c3e50"
            text: "扫描所选文件夹（含子目录）中的轨迹 Excel，解析后写入 PostGIS 数据库。"
                  + " 车牌号和时间范围会从 Excel 内容读取；同一车牌同一时间段已有数据会自动跳过。"
        }

        GroupBox {
            title: "数据库连接（来自 CarMoveTracker.ini）"
            Layout.fillWidth: true

            Text {
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: 12
                color: configManager ? "#1e8449" : "#e74c3c"
                text: dbSummary()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "轨迹目录"
                Layout.preferredWidth: 64
            }

            Text {
                Layout.fillWidth: true
                text: folderDisplayPath(importDialog.trajectoryFolderPath)
                elide: Text.ElideMiddle
                font.pixelSize: 12
                color: importDialog.trajectoryFolderPath.length > 0 ? "#2c3e50" : "#7f8c8d"
            }

            Button {
                text: "浏览"
                onClicked: trajectoryFolderDialog.open()
            }
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font.pixelSize: 11
            color: "#7f8c8d"
            text: "数据库连接参数见 CarMoveTracker.ini 的 [PostGISDatabase] 段"
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: excelModel.loading ? "导入中..." : "开始导入"
            enabled: canImport
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        if (!canImport)
            return
        importDialog.close()
        requestImport(importDialog.trajectoryFolderPath)
    }

    signal requestImport(string folderPath)
}

