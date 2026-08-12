import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CarMove 1.0

Rectangle {
    id: businessPanel
    color: "#f5f6fa"

    property var configManager: null
    property var batchScreenshotController: null

    readonly property bool batchScreenshotRunning: batchScreenshotController
                                                   && batchScreenshotController.running

    function mountMap(mapItem) {
        if (!mapItem)
            return
        mapItem.parent = mapPreviewHost
        mapItem.anchors.fill = mapPreviewHost
    }

    function unmountMap(mapItem, mapColumn) {
        if (!mapItem || !mapColumn)
            return
        mapItem.parent = mapColumn
        mapItem.anchors.fill = undefined
        mapItem.Layout.fillWidth = true
        mapItem.Layout.fillHeight = true
    }

    ExcelPreviewModel {
        id: excelModel
        onLoadFinished: function(success) {
            if (!success && excelModel.errorMessage.length > 0)
                errorDialog.showError(excelModel.errorMessage)
        }
        onSheetsChanged: sheetTabBar.currentIndex = excelModel.currentSheetIndex
    }

    Component.onCompleted: {
        if (batchScreenshotController)
            batchScreenshotController.excelModel = excelModel
    }

    FileDialog {
        id: excelFileDialog
        title: "选择 Excel 文件"
        nameFilters: ["Excel 文件 (*.xlsx *.xls)", "所有文件 (*)"]
        onAccepted: excelModel.loadFile(selectedFile.toString())
    }

    FileDialog {
        id: saveXlsxDialog
        title: "导出 XLSX 文件"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Excel 文件 (*.xlsx)", "所有文件 (*)"]
        defaultSuffix: "xlsx"
        onAccepted: {
            if (!exportDialog.pendingConfig)
                return

            const config = exportDialog.pendingConfig
            const success = excelModel.exportBusinessWithConfig(selectedFile, config)

            exportDialog.pendingConfig = null
            if (success) {
                errorDialog.showSuccess(excelModel.statusMessage)
            } else if (excelModel.errorMessage.length > 0) {
                errorDialog.showError(excelModel.errorMessage)
            }
        }
    }

    FolderDialog {
        id: saveXlsxFolderDialog
        title: "选择 XLSX 导出文件夹"
        onAccepted: {
            if (!exportDialog.pendingConfig)
                return

            const config = exportDialog.pendingConfig
            const success = excelModel.exportBusinessFolderWithConfig(selectedFolder, config)

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
                saveXlsxFolderDialog.currentFolder = excelModel.suggestedExportFolderUrl()
                saveXlsxFolderDialog.open()
            } else {
                saveXlsxDialog.currentFile = excelModel.suggestedExportFileUrl()
                saveXlsxDialog.open()
            }
        }

        onExportFailed: function(message) {
            errorDialog.showError(message)
        }
    }

    BusinessImportDatabaseDialog {
        id: importDatabaseDialog
        excelModel: excelModel
        configManager: businessPanel.configManager
        parent: Overlay.overlay

        onRequestImport: function(folderPath) {
            const success = excelModel.importTrajectoryFolderToDatabase(folderPath)
            if (success) {
                errorDialog.showSuccess(excelModel.statusMessage)
            } else if (excelModel.errorMessage.length > 0) {
                errorDialog.showError(excelModel.errorMessage)
            }
        }
    }

    BusinessScreenshotDialog {
        id: screenshotDialog
        excelModel: excelModel
        parent: Overlay.overlay

        onRequestBatchScreenshot: function(config, outputFolderPath) {
            if (!excelModel.beginScreenshotTasks(config)) {
                errorDialog.showError(excelModel.errorMessage.length > 0
                                      ? excelModel.errorMessage
                                      : "没有可截图的业务行")
                return
            }
            if (businessPanel.batchScreenshotController)
                businessPanel.batchScreenshotController.start(outputFolderPath)
        }

        onScreenshotFailed: function(message) {
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

    function openImportDatabaseDialog() {
        importDatabaseDialog.open()
    }

    function openScreenshotDialog() {
        if (!excelModel.hasData) {
            errorDialog.showError("请先打开 Excel 文件")
            return
        }
        screenshotDialog.open()
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

            Button {
                text: "导入数据库"
                enabled: !excelModel.loading && !(batchScreenshotController && batchScreenshotController.running)
                onClicked: openImportDatabaseDialog()
            }

            Button {
                text: batchScreenshotController && batchScreenshotController.running ? "截图中..." : "批量截图"
                enabled: excelModel.hasData && !excelModel.loading
                         && !(batchScreenshotController && batchScreenshotController.running)
                onClicked: openScreenshotDialog()
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
                text: batchScreenshotController && batchScreenshotController.running
                      ? ("正在截图 第 " + batchScreenshotController.processedCount
                         + " 条：" + batchScreenshotController.currentLabel)
                      : excelModel.statusMessage
                color: batchScreenshotController && batchScreenshotController.running ? "#2980b9" : "#27ae60"
                font.pixelSize: 12
                visible: (batchScreenshotController && batchScreenshotController.running)
                         || (excelModel.statusMessage.length > 0 && !excelModel.loading)
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
            visible: excelModel.sheetCount > 0 && !businessPanel.batchScreenshotRunning
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
            visible: excelModel.sheetCount > 0 && !excelModel.loading && !businessPanel.batchScreenshotRunning
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: excelModel.hasData && !excelModel.loading && !businessPanel.batchScreenshotRunning

            Row {
                spacing: 4
                Rectangle { width: 12; height: 12; color: "#ffd6d6"; border.color: "#e74c3c"; radius: 2 }
                Text { text: "车牌列"; font.pixelSize: 11; color: "#c0392b" }
            }

            Row {
                spacing: 4
                Rectangle { width: 12; height: 12; color: "#d5f5e3"; border.color: "#27ae60"; radius: 2 }
                Text { text: "时间列"; font.pixelSize: 11; color: "#1e8449" }
            }

            Text {
                Layout.fillWidth: true
                text: excelModel.detectedPlateColumnCount > 0 || excelModel.detectedDateColumnCount > 0
                      ? ("已识别 " + excelModel.detectedPlateColumnCount + " 列车牌、"
                         + excelModel.detectedDateColumnCount + " 列时间（导出时再处理数据）")
                      : "未识别到车牌或时间列，请检查 Excel 内容"
                font.pixelSize: 11
                color: excelModel.detectedPlateColumnCount > 0 && excelModel.detectedDateColumnCount > 0
                       ? "#2c3e50" : "#e67e22"
                wrapMode: Text.WordWrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#ffffff"
            border.color: "#dcdde1"
            border.width: 1
            radius: 4
            clip: true

            Item {
                id: mapPreviewHost
                anchors.fill: parent
                anchors.margins: 1
            }

            ScrollView {
                id: tableScroll
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                visible: !businessPanel.batchScreenshotRunning
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
                      : ""
                color: "#7f8c8d"
                font.pixelSize: 14
                visible: excelModel.loading && !businessPanel.batchScreenshotRunning
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
