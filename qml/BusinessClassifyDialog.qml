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

    readonly property var dateColumns: excelModel.dateColumnOptions
    readonly property bool hasTwoDateColumns: excelModel.detectedDateColumnCount >= 2
    readonly property bool hasOneDateColumn: excelModel.detectedDateColumnCount === 1
    readonly property bool canClassify: excelModel.hasData
                                     && excelModel.defaultPlateColumnNumber > 0
                                     && excelModel.detectedDateColumnCount > 0
                                     && trajectoryFolderPath.length > 0
                                     && outputFolderPath.length > 0

    onOpened: {
        resetSelections()
        if (outputFolderPath.length === 0) {
            outputFolderPath = excelModel.suggestedExportFolderUrl().toString()
        }
    }

    function resetSelections() {
        if (dateColumns.length === 0) {
            return
        }

        if (hasTwoDateColumns) {
            startColumnBox.currentIndex = 0
            endColumnBox.currentIndex = Math.min(1, dateColumns.length - 1)
        } else if (hasOneDateColumn) {
            singleColumnBox.currentIndex = 0
            singleTimeRoleGroup.checkedButton = startTimeRadio
            dayOffsetSpin.value = 0
        }
    }

    function columnNumberAt(box) {
        if (!box || box.currentIndex < 0 || box.currentIndex >= dateColumns.length) {
            return -1
        }
        return dateColumns[box.currentIndex].columnNumber
    }

    function columnLabel(item) {
        if (!item || item.sample.length === 0) {
            return "列 " + item.columnNumber
        }
        return "列 " + item.columnNumber + "（" + item.sample + "）"
    }

    function buildClassifyConfig() {
        if (!canClassify) {
            return null
        }

        const config = {
            dayOffset: dayOffsetSpin.value,
            trajectoryFolderPath: trajectoryFolderPath,
            outputFolderPath: outputFolderPath
        }

        if (hasTwoDateColumns) {
            config.startColumnNumber = columnNumberAt(startColumnBox)
            config.endColumnNumber = columnNumberAt(endColumnBox)
            config.singleTimeRole = ""
        } else {
            const dateColumnNumber = columnNumberAt(singleColumnBox)
            if (startTimeRadio.checked) {
                config.startColumnNumber = dateColumnNumber
                config.endColumnNumber = -1
                config.singleTimeRole = "start"
            } else {
                config.startColumnNumber = -1
                config.endColumnNumber = dateColumnNumber
                config.singleTimeRole = "end"
            }
        }

        return config
    }

    function validateConfig(config) {
        if (!config) {
            return "当前数据无法归类"
        }

        if (hasTwoDateColumns) {
            if (config.startColumnNumber === config.endColumnNumber) {
                return "开始时间和结束时间不能选择同一列"
            }
        }

        if (config.trajectoryFolderPath.length === 0) {
            return "请选择轨迹文件目录"
        }

        if (config.outputFolderPath.length === 0) {
            return "请选择输出目录"
        }

        return ""
    }

    function folderDisplayPath(folderUrl) {
        if (!folderUrl || folderUrl.length === 0) {
            return "未选择"
        }
        const path = folderUrl.toString()
        if (path.startsWith("file:///")) {
            return decodeURIComponent(path.substring(8))
        }
        return path
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

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: excelModel && excelModel.defaultPlateColumnNumber > 0 ? "#1e8449" : "#e74c3c"
            text: excelModel && excelModel.defaultPlateColumnNumber > 0
                  ? "车牌列：列 " + excelModel.defaultPlateColumnNumber + "（自动识别标红列）"
                  : "未检测到车牌列，无法归类"
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

        ColumnLayout {
            Layout.fillWidth: true
            visible: hasTwoDateColumns
            spacing: 8

            Text {
                text: "检测到 " + excelModel.detectedDateColumnCount + " 列时间，请分别选择开始和结束时间所在列："
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: "#2c3e50"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "开始时间列"
                    Layout.preferredWidth: 72
                }

                ComboBox {
                    id: startColumnBox
                    Layout.fillWidth: true
                    model: dateColumns
                    textRole: "sample"
                    displayText: currentIndex >= 0 ? columnLabel(dateColumns[currentIndex]) : ""
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "结束时间列"
                    Layout.preferredWidth: 72
                }

                ComboBox {
                    id: endColumnBox
                    Layout.fillWidth: true
                    model: dateColumns
                    textRole: "sample"
                    displayText: currentIndex >= 0 ? columnLabel(dateColumns[currentIndex]) : ""
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: hasOneDateColumn
            spacing: 8

            Text {
                text: "检测到 1 列时间，请选择该列含义，并填写另一时间的天数偏移："
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: "#2c3e50"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "时间列"
                    Layout.preferredWidth: 72
                }

                ComboBox {
                    id: singleColumnBox
                    Layout.fillWidth: true
                    model: dateColumns
                    textRole: "sample"
                    displayText: currentIndex >= 0 ? columnLabel(dateColumns[currentIndex]) : ""
                }
            }

            ButtonGroup {
                id: singleTimeRoleGroup
            }

            RowLayout {
                spacing: 16

                RadioButton {
                    id: startTimeRadio
                    text: "该列为开始时间"
                    checked: true
                    ButtonGroup.group: singleTimeRoleGroup
                }

                RadioButton {
                    id: endTimeRadio
                    text: "该列为结束时间"
                    ButtonGroup.group: singleTimeRoleGroup
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: startTimeRadio.checked ? "结束时间 = 开始 + 天数" : "开始时间 = 结束 - 天数"
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    color: "#7f8c8d"
                    font.pixelSize: 11
                }

                SpinBox {
                    id: dayOffsetSpin
                    from: 0
                    to: 3650
                    value: 0
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#ecf0f1"
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "轨迹目录"
                Layout.preferredWidth: 72
            }

            Text {
                Layout.fillWidth: true
                text: folderDisplayPath(classifyDialog.trajectoryFolderPath)
                elide: Text.ElideMiddle
                color: classifyDialog.trajectoryFolderPath.length > 0 ? "#2c3e50" : "#7f8c8d"
                font.pixelSize: 12
            }

            Button {
                text: "浏览"
                onClicked: trajectoryFolderDialog.open()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "输出目录"
                Layout.preferredWidth: 72
            }

            Text {
                Layout.fillWidth: true
                text: folderDisplayPath(classifyDialog.outputFolderPath)
                elide: Text.ElideMiddle
                color: classifyDialog.outputFolderPath.length > 0 ? "#2c3e50" : "#7f8c8d"
                font.pixelSize: 12
            }

            Button {
                text: "浏览"
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
            text: "开始归类"
            enabled: canClassify
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        const config = buildClassifyConfig()
        const validationError = validateConfig(config)
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
