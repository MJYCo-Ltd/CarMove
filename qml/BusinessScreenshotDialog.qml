import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: screenshotDialog

    required property var excelModel

    property string outputFolderPath: ""

    title: "批量轨迹截图"
    modal: true
    anchors.centerIn: parent
    width: 480
    padding: 16

    readonly property bool canStart: excelModel.hasData
                                   && excelModel.defaultPlateColumnNumber > 0
                                   && excelModel.detectedDateColumnCount > 0
                                   && outputFolderPath.length > 0

    onOpened: columnSection.resetSelections()

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
            text: "按业务 Excel 中的车牌、开始时间、结束时间，从 PostGIS 加载轨迹并自动截图。"
                  + " 将自动切换到轨迹页；无轨迹的记录会跳过。"
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: "#7f8c8d"
            font.pixelSize: 11
            text: "数据源：CarMoveTracker.ini 中的 PostGIS 配置"
        }

        BusinessPlateColumnHint {
            excelModel: screenshotDialog.excelModel
            missingText: "未检测到车牌列"
        }

        BusinessDateColumnSection {
            id: columnSection
            Layout.fillWidth: true
            dateColumns: excelModel.dateColumnOptions
            detectedDateColumnCount: excelModel.detectedDateColumnCount
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "输出文件夹"
                Layout.preferredWidth: 72
            }

            TextField {
                id: outputFolderField
                Layout.fillWidth: true
                readOnly: true
                placeholderText: "选择截图保存目录"
                text: controller ? controller.normalizeLocalPath(screenshotDialog.outputFolderPath)
                                 : screenshotDialog.outputFolderPath
            }

            Button {
                text: "浏览..."
                onClicked: outputFolderDialog.open()
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: "开始截图"
            enabled: canStart
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    FolderDialog {
        id: outputFolderDialog
        title: "选择截图输出文件夹"
        onAccepted: {
            if (controller) {
                screenshotDialog.outputFolderPath = controller.normalizeLocalPath(selectedFolder.toString())
            }
            outputFolderField.text = controller
                ? controller.normalizeLocalPath(screenshotDialog.outputFolderPath)
                : screenshotDialog.outputFolderPath
        }
    }

    onAccepted: {
        const config = columnSection.buildColumnConfig()
        let validationError = columnSection.validateColumnConfig(config)
        if (validationError.length === 0 && outputFolderPath.length === 0) {
            validationError = "请选择截图输出文件夹"
        }

        if (validationError.length > 0) {
            screenshotDialog.close()
            screenshotFailed(validationError)
            return
        }

        screenshotDialog.close()
        requestBatchScreenshot(config, outputFolderPath)
    }

    signal requestBatchScreenshot(var config, string outputFolderPath)
    signal screenshotFailed(string message)
}
