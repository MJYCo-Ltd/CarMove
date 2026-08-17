import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: screenshotDialog

    required property var excelModel

    property string outputFolderPath: ""
    property bool onlyLargeImage: false

    title: "批量轨迹截图"
    modal: true
    anchors.centerIn: parent
    width: 480
    padding: 16

    readonly property bool canStart: excelModel.hasData
                                   && excelModel.defaultPlateColumnNumber > 0
                                   && excelModel.detectedDateColumnCount > 0
                                   && outputFolderPath.length > 0

    onOpened: {
        columnSection.resetSelections()
        onlyLargeImageCheck.checked = onlyLargeImage
    }

    background: Rectangle {
        color: "#ffffff"
        border.color: "#dcdde1"
        radius: 8
    }

    contentItem: ColumnLayout {
        spacing: 12

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

        CheckBox {
            id: onlyLargeImageCheck
            text: "只截大图"
            checked: screenshotDialog.onlyLargeImage
            onCheckedChanged: screenshotDialog.onlyLargeImage = checked
        }

        Text {
            Layout.fillWidth: true
            visible: onlyLargeImageCheck.checked
            wrapMode: Text.WordWrap
            color: "#7f8c8d"
            font.pixelSize: 12
            text: "视口仅覆盖全部航点，不包含目标位置，且不截目标区小图"
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
        requestBatchScreenshot(config, outputFolderPath, onlyLargeImage)
    }

    signal requestBatchScreenshot(var config, string outputFolderPath, bool onlyLargeImage)
    signal screenshotFailed(string message)
}
