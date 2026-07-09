import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

Dialog {
    id: classifyDialog

    required property var excelModel

    property string trajectoryFolderPath: ""
    property string outputFolderPath: ""

    title: "归类轨迹文件"
    modal: true
    anchors.centerIn: parent
    width: 500
    padding: 16

    readonly property bool canClassify: excelModel.hasData
                                     && excelModel.defaultPlateColumnNumber > 0
                                     && excelModel.detectedDateColumnCount > 0
                                     && trajectoryFolderPath.length > 0
                                     && outputFolderPath.length > 0

    onOpened: {
        columnSection.resetSelections()
        if (outputFolderPath.length === 0) {
            outputFolderPath = excelModel.suggestedExportFolderUrl().toString()
        }
    }

    FolderDialog {
        id: trajectoryFolderDialog
        title: "选择轨迹文件目录"
        onAccepted: classifyDialog.trajectoryFolderPath = selectedFolder.toString()
    }

    FolderDialog {
        id: outputFolderDialog
        title: "选择输出目录"
        onAccepted: classifyDialog.outputFolderPath = selectedFolder.toString()
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
            text: "按业务数据中的车牌、开始时间、结束时间，将轨迹目录中的文件移动到以 CSV 文件名命名的文件夹。"
                  + " 轨迹文件命名格式：车牌-开始日期-结束日期.xlsx。"
                  + (excelModel.sheetCount > 1
                     ? " 多工作表时每张表对应一个 CSV 和一个同名文件夹。"
                     : "")
        }

        BusinessPlateColumnHint {
            excelModel: classifyDialog.excelModel
            missingText: "未检测到车牌列，无法归类"
        }

        Text {
            Layout.fillWidth: true
            visible: excelModel.hasData
                     && excelModel.defaultPlateColumnNumber > 0
                     && excelModel.detectedDateColumnCount === 0
            wrapMode: Text.WordWrap
            color: "#e74c3c"
            text: "未检测到日期列，无法归类"
        }

        BusinessDateColumnSection {
            id: columnSection
            Layout.fillWidth: true
            dateColumns: excelModel.dateColumnOptions
            detectedDateColumnCount: excelModel.detectedDateColumnCount
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#ecf0f1"
        }

        BusinessFolderPickerRow {
            Layout.fillWidth: true
            label: "轨迹目录"
            folderPath: controller ? controller.normalizeLocalPath(classifyDialog.trajectoryFolderPath) : classifyDialog.trajectoryFolderPath
            onBrowseRequested: trajectoryFolderDialog.open()
        }

        BusinessFolderPickerRow {
            Layout.fillWidth: true
            label: "输出目录"
            folderPath: controller ? controller.normalizeLocalPath(classifyDialog.outputFolderPath) : classifyDialog.outputFolderPath
            onBrowseRequested: outputFolderDialog.open()
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: "开始归类"
            enabled: classifyDialog.canClassify
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        const config = columnSection.buildColumnConfig()
        config.trajectoryFolderPath = trajectoryFolderPath
        config.outputFolderPath = outputFolderPath

        let validationError = columnSection.validateColumnConfig(config)
        if (validationError.length === 0 && config.trajectoryFolderPath.length === 0) {
            validationError = "请选择轨迹文件目录"
        }
        if (validationError.length === 0 && config.outputFolderPath.length === 0) {
            validationError = "请选择输出目录"
        }

        if (validationError.length > 0) {
            classifyDialog.close()
            classifyFailed(validationError)
            return
        }

        classifyDialog.close()
        requestClassify(config)
    }

    signal requestClassify(var config)
    signal classifyFailed(string message)
}
