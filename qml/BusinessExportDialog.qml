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

    readonly property var dateColumns: excelModel.dateColumnOptions
    readonly property bool hasTwoDateColumns: excelModel.detectedDateColumnCount >= 2
    readonly property bool hasOneDateColumn: excelModel.detectedDateColumnCount === 1
    readonly property bool canExport: excelModel.hasData
                                   && excelModel.defaultPlateColumnNumber > 0
                                   && excelModel.detectedDateColumnCount > 0

    onOpened: resetSelections()

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

    function buildExportConfig() {
        if (!canExport) {
            return null
        }

        const config = {
            dayOffset: dayOffsetSpin.value
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
            return "当前数据无法导出"
        }

        if (hasTwoDateColumns) {
            if (config.startColumnNumber === config.endColumnNumber) {
                return "开始时间和结束时间不能选择同一列"
            }
        }

        return ""
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
            text: "导出工作簿中的车牌、开始时间、结束时间到 CSV 文件。"
                  + (excelModel.sheetCount > 1
                     ? " 多工作表时每张表单独导出一个 CSV，文件名格式：Excel名-表名.csv。"
                     : "")
        }

        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            color: excelModel && excelModel.defaultPlateColumnNumber > 0 ? "#1e8449" : "#e74c3c"
            text: excelModel && excelModel.defaultPlateColumnNumber > 0
                  ? "车牌列：列 " + excelModel.defaultPlateColumnNumber + "（自动识别标红列）"
                  : "未检测到车牌列，无法导出"
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
    }

    footer: DialogButtonBox {
        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: "下一步"
            enabled: canExport
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: {
        const config = buildExportConfig()
        const validationError = validateConfig(config)
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
