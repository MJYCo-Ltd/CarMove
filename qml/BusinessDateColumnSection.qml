import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: root

    property var dateColumns: []
    property int detectedDateColumnCount: 0

    readonly property bool hasTwoDateColumns: detectedDateColumnCount >= 2
    readonly property bool hasOneDateColumn: detectedDateColumnCount === 1

    spacing: 8

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

    function buildColumnConfig() {
        const config = {
            dayOffset: dayOffsetSpin.value
        }

        if (hasTwoDateColumns) {
            config.startColumnNumber = columnNumberAt(startColumnBox)
            config.endColumnNumber = columnNumberAt(endColumnBox)
            config.singleTimeRole = ""
        } else if (hasOneDateColumn) {
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

    function validateColumnConfig(config) {
        if (!config) {
            return "当前数据无法处理"
        }

        if (hasTwoDateColumns && config.startColumnNumber === config.endColumnNumber) {
            return "开始时间和结束时间不能选择同一列"
        }

        return ""
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.hasTwoDateColumns
        spacing: 8

        Text {
            text: "检测到 " + root.detectedDateColumnCount + " 列时间，请分别选择开始和结束时间所在列："
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
                model: root.dateColumns
                textRole: "sample"
                displayText: currentIndex >= 0 ? root.columnLabel(dateColumns[currentIndex]) : ""
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
                model: root.dateColumns
                textRole: "sample"
                displayText: currentIndex >= 0 ? root.columnLabel(dateColumns[currentIndex]) : ""
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        visible: root.hasOneDateColumn
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
                model: root.dateColumns
                textRole: "sample"
                displayText: currentIndex >= 0 ? root.columnLabel(dateColumns[currentIndex]) : ""
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
