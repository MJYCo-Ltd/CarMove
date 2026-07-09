import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: exportDialog

    required property var excelModel

    property var pendingConfig: null

    title: "导出业务数据"
    modal: true
    anchors.centerIn: parent
    width: 460
    padding: 16

    readonly property bool canExport: excelModel.hasData
                                   && excelModel.defaultPlateColumnNumber > 0
                                   && excelModel.detectedDateColumnCount > 0

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
            text: "导出工作簿中的车牌、开始时间、结束时间到 CSV 文件。"
                  + (excelModel.sheetCount > 1
                     ? " 多工作表时每张表单独导出一个 CSV，文件名格式：Excel名-表名.csv。"
                     : "")
        }

        BusinessPlateColumnHint {
            excelModel: exportDialog.excelModel
            missingText: "未检测到车牌列，无法导出"
        }

        Text {
            Layout.fillWidth: true
            visible: excelModel.hasData
                     && excelModel.defaultPlateColumnNumber > 0
                     && excelModel.detectedDateColumnCount === 0
            wrapMode: Text.WordWrap
            color: "#e74c3c"
            text: "未检测到日期列，无法导出"
        }

        BusinessDateColumnSection {
            id: columnSection
            Layout.fillWidth: true
            dateColumns: excelModel.dateColumnOptions
            detectedDateColumnCount: excelModel.detectedDateColumnCount
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: "下一步"
            enabled: exportDialog.canExport
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        const config = columnSection.buildColumnConfig()
        const validationError = columnSection.validateColumnConfig(config)
        if (validationError.length > 0) {
            exportDialog.close()
            exportFailed(validationError)
            return
        }

        pendingConfig = config
        exportDialog.close()
        requestSaveFile(config)
    }

    signal requestSaveFile(var config)
    signal exportFailed(string message)
}
