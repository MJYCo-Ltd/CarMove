import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CarMove 1.0

Rectangle {
    id: businessPanel
    color: "#f5f6fa"

    ExcelPreviewModel {
        id: excelModel
        onLoadFinished: function(success) {
            if (!success && excelModel.errorMessage.length > 0)
                errorDialog.showError(excelModel.errorMessage)
        }
        onSheetsChanged: sheetTabBar.currentIndex = excelModel.currentSheetIndex
    }

    FileDialog {
        id: excelFileDialog
        title: "选择 Excel 文件"
        nameFilters: ["Excel 文件 (*.xlsx *.xls)", "所有文件 (*)"]
        onAccepted: excelModel.loadFile(selectedFile.toString())
    }

    FileDialog {
        id: saveCsvDialog
        title: "导出 CSV 文件"
        fileMode: FileDialog.SaveFile
        nameFilters: ["CSV 文件 (*.csv)", "所有文件 (*)"]
        defaultSuffix: "csv"
        onAccepted: {
            if (!exportDialog.pendingConfig)
                return

            const config = exportDialog.pendingConfig
            const success = excelModel.exportBusinessCsv(
                selectedFile,
                config.startColumnNumber,
                config.endColumnNumber,
                config.singleTimeRole,
                config.dayOffset
            )

            exportDialog.pendingConfig = null
            if (success) {
                errorDialog.showSuccess(excelModel.statusMessage)
            } else if (excelModel.errorMessage.length > 0) {
                errorDialog.showError(excelModel.errorMessage)
            }
        }
    }

    FolderDialog {
        id: saveCsvFolderDialog
        title: "选择 CSV 导出文件夹"
        onAccepted: {
            if (!exportDialog.pendingConfig)
                return

            const config = exportDialog.pendingConfig
            const success = excelModel.exportBusinessCsvToFolder(
                selectedFolder,
                config.startColumnNumber,
                config.endColumnNumber,
                config.singleTimeRole,
                config.dayOffset
            )

            exportDialog.pendingConfig = null
            if (success) {
                errorDialog.showSuccess(excelModel.statusMessage)
            } else if (excelModel.errorMessage.length > 0) {
                errorDialog.showError(excelModel.errorMessage)
            }
        }
    }

    BusinessExportDialog {
        id: exportDialog
        excelModel: excelModel
        parent: Overlay.overlay

        onRequestSaveFile: function(config) {
            exportDialog.pendingConfig = config
            if (excelModel.exportUsesFolder()) {
                saveCsvFolderDialog.currentFolder = excelModel.suggestedExportFolderUrl()
                saveCsvFolderDialog.open()
            } else {
                saveCsvDialog.currentFile = excelModel.suggestedExportFileUrl()
                saveCsvDialog.open()
            }
        }

        onExportFailed: function(message) {
            errorDialog.showError(message)
        }
    }

    function openExcelFile() {
        excelFileDialog.open()
    }

    function openExportDialog() {
        if (!excelModel.hasData) {
            errorDialog.showError("请先打开 Excel 文件")
            return
        }
        exportDialog.open()
    }

    NotificationDialog { id: errorDialog }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: excelModel.loading ? "加载中..." : "打开 Excel 文件"
                enabled: !excelModel.loading
                onClicked: excelFileDialog.open()
            }

            Button {
                text: "导出"
                enabled: excelModel.hasData && !excelModel.loading
                onClicked: openExportDialog()
            }

            Text {
                Layout.fillWidth: true
                text: excelModel.fileName.length > 0 ? excelModel.fileName : "未选择文件"
                elide: Text.ElideMiddle
                color: excelModel.fileName.length > 0 ? "#2c3e50" : "#7f8c8d"
                font.pixelSize: 14
                font.bold: excelModel.fileName.length > 0
            }

            Text {
                text: excelModel.statusMessage
                color: "#27ae60"
                font.pixelSize: 12
                visible: excelModel.statusMessage.length > 0 && !excelModel.loading
            }

            BusyIndicator {
                visible: excelModel.loading
                running: excelModel.loading
                Layout.preferredWidth: 28
                Layout.preferredHeight: 28
            }
        }

        Text {
            Layout.fillWidth: true
            text: excelModel.errorMessage
            wrapMode: Text.WordWrap
            color: "#e74c3c"
            font.pixelSize: 12
            visible: excelModel.errorMessage.length > 0 && !excelModel.loading
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            visible: excelModel.sheetCount > 0
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AsNeeded
            ScrollBar.vertical.policy: ScrollBar.AlwaysOff

            TabBar {
                id: sheetTabBar
                width: Math.max(parent.width, implicitWidth)
                currentIndex: excelModel.currentSheetIndex
                onCurrentIndexChanged: {
                    if (currentIndex !== excelModel.currentSheetIndex)
                        excelModel.setCurrentSheetIndex(currentIndex)
                }

                Repeater {
                    model: excelModel.sheetNames
                    TabButton {
                        text: modelData
                        width: Math.max(implicitWidth + 24, 72)
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: excelModel.currentSheetStatus
            color: "#7f8c8d"
            font.pixelSize: 11
            visible: excelModel.sheetCount > 0 && !excelModel.loading
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#dcdde1"
            border.width: 1
            radius: 4

            ScrollView {
                id: tableScroll
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                ScrollBar.vertical.policy: ScrollBar.AsNeeded

                readonly property int cellWidth: 120
                readonly property int cellHeight: 28
                readonly property int rowNumberWidth: 52

                TableView {
                    id: excelTable
                    width: Math.max(1, tableScroll.rowNumberWidth
                                    + excelModel.previewDataColumnCount * tableScroll.cellWidth)
                    height: Math.max(1, excelModel.previewRowCount * tableScroll.cellHeight)
                    model: excelModel
                    clip: true

                    columnWidthProvider: function(column) {
                        if (column === 0) {
                            return tableScroll.rowNumberWidth
                        }
                        if (column < excelModel.previewColumnCount) {
                            return tableScroll.cellWidth
                        }
                        return 0
                    }
                    rowHeightProvider: function(row) {
                        return row < excelModel.previewRowCount ? tableScroll.cellHeight : 0
                    }

                    delegate: Rectangle {
                        required property var display
                        required property int row
                        required property int column

                        readonly property bool isRowNumberColumn: column === 0
                        readonly property bool isPlateColumn: !isRowNumberColumn
                                                            && excelModel.isPlateColumn(column)
                        readonly property bool isDateColumn: !isRowNumberColumn
                                                           && excelModel.isDateColumn(column)

                        visible: column < excelModel.previewColumnCount
                        implicitWidth: isRowNumberColumn ? tableScroll.rowNumberWidth : tableScroll.cellWidth
                        implicitHeight: tableScroll.cellHeight
                        color: isRowNumberColumn ? "#eceff1"
                             : isPlateColumn ? (row % 2 === 0 ? "#ffe5e5" : "#ffd6d6")
                             : isDateColumn ? (row % 2 === 0 ? "#e8f8f0" : "#d5f5e3")
                             : (row % 2 === 0 ? "#ffffff" : "#f8f9fa")
                        border.color: isPlateColumn ? "#e74c3c"
                                      : isDateColumn ? "#27ae60" : "#e0e0e0"
                        border.width: (isPlateColumn || isDateColumn) ? 1 : 0.5

                        Text {
                            anchors.fill: parent
                            anchors.margins: 4
                            text: display ?? ""
                            font.pixelSize: 11
                            color: isRowNumberColumn ? "#7f8c8d"
                                 : isPlateColumn ? "#c0392b"
                                 : isDateColumn ? "#1e8449" : "#2c3e50"
                            font.bold: isPlateColumn || isDateColumn
                            horizontalAlignment: isRowNumberColumn ? Text.AlignHCenter : Text.AlignLeft
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                width: parent.width - 40
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: excelModel.loading
                      ? "正在加载 Excel 文件..."
                      : "请点击「打开 Excel 文件」或左侧「业务」按钮选择 .xlsx / .xls 文件"
                color: "#7f8c8d"
                font.pixelSize: 14
                visible: !excelModel.hasData && !excelModel.loading
            }
        }
    }

    Connections {
        target: excelModel
        function onCurrentSheetIndexChanged() {
            if (sheetTabBar.currentIndex !== excelModel.currentSheetIndex)
                sheetTabBar.currentIndex = excelModel.currentSheetIndex
        }
    }
}
