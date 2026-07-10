import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import CarMove 1.0

ApplicationWindow {
    id: mainWindow
    width: 1200
    height: 800
    title: "CarMove 车辆轨迹追踪系统"
    visible: true

    onClosing: function(close) {
        batchScreenshotController.stopSilently()
        mapDisplay.cancelTileWait()
        close.accepted = true
    }

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: {
            if (controller && controller.useDatabaseTrajectorySource)
                return
            folderDialog.open()
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (!controller)
                return
            if (controller.trajectorySegmentCount() > 0)
                controller.seekTrajectorySegment(0, 0)
            else
                controller.seekTrajectoryToProgress(0)
        }
    }

    property bool isBusinessMode: sidebarPanel.currentMode === "business"
    readonly property bool batchScreenshotActive: batchScreenshotController.running

    footer: ToolBar {
        visible: !mainWindow.isBusinessMode && !mainWindow.batchScreenshotActive
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            /// 与 MapDisplay.mapVehicleContextActive、时间轴栏、坐标切换按钮同源
            Label {
                text: mapDisplay.mapVehicleContextActive
                      ? ("已选择车辆: " + controller.selectedVehicle)
                      : "未选择车辆"
                Layout.fillWidth: true
            }
            Label {
                visible: controller && controller.useDatabaseTrajectorySource
                text: controller
                      ? (controller.databaseConnected
                         ? ("数据库: " + controller.databaseStatus)
                         : "数据库: 未连接")
                      : ""
                color: controller && controller.databaseConnected ? "#1e8449" : "#7f8c8d"
            }
            Label {
                text: (controller && controller.vehicleList)
                      ? ("车辆数量: " + controller.vehicleList.length)
                      : "无车辆数据"
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        SidebarPanel {
            id: sidebarPanel
            Layout.preferredWidth: 60
            Layout.fillHeight: true
            onModeChanged: function(mode) {
                trajectoryPanel.visible = (mode === "trajectory")
                businessPanel.visible = (mode === "business")
                geoSearchPanel.visible   = (mode === "search")
                navigationPanel.visible  = (mode === "nav")
                if (mode === "trajectory") {
                    mapDisplay.clearSearchResult()
                    mapDisplay.clearNavigationRoute()
                    mapDisplay.clearNavigationEndpointMarkers()
                } else if (mode === "business") {
                    mapDisplay.clearSearchResult()
                    mapDisplay.clearNavigationRoute()
                    mapDisplay.clearNavigationEndpointMarkers()
                } else if (mode === "nav") {
                    mapDisplay.clearSearchResult()
                }
            }
        }

        BusinessPanel {
            id: businessPanel
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: mainWindow.isBusinessMode || mainWindow.batchScreenshotActive
            configManager: controller ? controller.configManager : null
            batchScreenshotController: batchScreenshotController
        }

        TrajectoryPanel {
            id: trajectoryPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: sidebarPanel.currentMode === "trajectory" && !mainWindow.batchScreenshotActive
            onOpenFolderDialogRequested: folderDialog.open()
        }

        GeoSearchPanel {
            id: geoSearchPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: false
            onLocateRequested: function(lat, lon, name) { mapDisplay.locateToPlace(lat, lon) }
            onTargetAreaRequested: function(lat, lon, name) { mapDisplay.setTargetAreaFromSearch(lat, lon, name) }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !mainWindow.isBusinessMode || mainWindow.batchScreenshotActive

            MapDisplay {
                id: mapDisplay
                Layout.fillWidth: true
                Layout.fillHeight: true
                trajectoryModeActive: sidebarPanel.currentMode === "trajectory"
                                      || batchScreenshotController.running
                batchScreenshotActive: batchScreenshotController.running
            }

            TrajectoryTimelineBar {
                id: trajectoryTimelineBar
                Layout.fillWidth: true
                Layout.preferredHeight: 62
                mapVehicleContextActive: mapDisplay.mapVehicleContextActive
                                         && mapDisplay.trajectoryModeActive
                                         && !batchScreenshotController.running
            }
        }

        NavigationPanel {
            id: navigationPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: false
            mapRef: mapDisplay
        }
    }

    FolderDialog {
        id: folderDialog
        title: "选择包含车辆数据的文件夹"
        onAccepted: { if (controller) controller.selectFolder(selectedFolder) }
    }

    NotificationDialog { id: errorDialog }
    NotificationDialog { id: successDialog }

    QtObject {
        id: batchScreenshotController

        property var excelModel: null
        property bool running: false
        property var currentTask: null
        property int processedCount: 0
        property string outputDir: ""
        property int capturedCount: 0
        property int targetCapturedCount: 0
        property int skippedCount: 0
        property string currentLabel: ""

        function normalizeFolderPath(path) {
            if (!path || !controller)
                return ""
            return controller.normalizeLocalPath(path)
        }

        function start(outputFolderPath) {
            if (!controller || !excelModel) {
                errorDialog.showError("没有可截图的业务行")
                return
            }

            const folder = normalizeFolderPath(outputFolderPath)
            if (!controller.ensureScreenshotOutputDirectory(folder)) {
                errorDialog.showError("无法创建截图输出目录")
                return
            }

            currentTask = null
            processedCount = 0
            outputDir = folder
            capturedCount = 0
            targetCapturedCount = 0
            skippedCount = 0
            running = true

            if (!controller.useDatabaseTrajectorySource)
                controller.setTrajectorySourceMode("database")

            if (!controller.databaseConnected)
                controller.connectPostGisDatabase()

            if (!controller.databaseConnected) {
                running = false
                excelModel.cancelScreenshotTasks()
                errorDialog.showError("PostGIS 数据库未连接，请检查 CarMoveTracker.ini")
                return
            }

            processNext()
        }

        function processNext() {
            if (!running) {
                finish()
                return
            }

            mapDisplay.cancelTileWait()

            const task = excelModel.nextScreenshotTask()
            if (!task || !task.plate) {
                finish()
                return
            }

            currentTask = task
            processedCount++

            if (controller.screenshotFileExists(outputDir, task.plate, task.startDate, task.endDate)) {
                skippedCount++
                processNext()
                return
            }

            currentLabel = task.plate + "  " + task.startDate + " ~ " + task.endDate
            controller.loadTrajectoryForCapture(task.plate, task.startDate, task.endDate)
        }

        function onCaptureReady(success, pointCount) {
            if (!running)
                return

            mapDisplay.cancelTileWait()

            if (!success || pointCount < 2) {
                skippedCount++
                processNext()
                return
            }

            if (!currentTask)
                return

            const traj = controller.getConvertedTrajectory()
            mapDisplay.prepareAndCaptureWhenReady(
                function() {
                    mapDisplay.prepareTrajectoryForCapture(currentTask.plate, traj, "#3498db")
                },
                controller.screenshotFilePath(outputDir, currentTask.plate,
                                            currentTask.startDate, currentTask.endDate),
                function(ok) {
                    if (!running || !currentTask)
                        return
                    if (!ok) {
                        skippedCount++
                        processNext()
                        return
                    }
                    capturedCount++
                    captureTargetAreaIfNeeded()
                })
        }

        function captureTargetAreaIfNeeded() {
            if (!running || !currentTask || !controller)
                return

            if (controller.targetAreaVisitCountForPlate(currentTask.plate) <= 0) {
                processNext()
                return
            }

            mapDisplay.prepareAndCaptureWhenReady(
                function() { mapDisplay.centerToLocation(true, 18, true) },
                controller.targetAreaScreenshotFilePath(outputDir, currentTask.plate,
                                                      currentTask.startDate, currentTask.endDate),
                function(ok) {
                    if (ok)
                        targetCapturedCount++
                    processNext()
                })
        }

        function finish() {
            if (!running)
                return
            running = false
            currentTask = null
            currentLabel = ""
            if (excelModel)
                excelModel.cancelScreenshotTasks()
            mapDisplay.cancelTileWait()
            mapDisplay.resetCaptureViewportMargin()
            successDialog.showSuccess("截图完成：共处理 " + processedCount + " 条，大图 "
                                    + capturedCount + " 张，目标区小图 " + targetCapturedCount
                                    + " 张，跳过 " + skippedCount + " 条")
        }

        function stopSilently() {
            if (!running)
                return
            running = false
            currentTask = null
            currentLabel = ""
            if (excelModel)
                excelModel.cancelScreenshotTasks()
            mapDisplay.cancelTileWait()
            mapDisplay.resetCaptureViewportMargin()
        }
    }

    Connections {
        target: controller

        function onCaptureTrajectoryReady(success, pointCount) {
            batchScreenshotController.onCaptureReady(success, pointCount)
        }

        function onFolderScanned(success, message) {
            if (success) successDialog.showSuccess(message)
            else errorDialog.showError(message)
        }

        function onSelectedVehicleChanged() {
            if (batchScreenshotController.running)
                return
        }

        function onTrajectoryLoaded(success, message) {
            if (batchScreenshotController.running)
                return
            if (!success) { errorDialog.showError(message); return }
            if (!controller || !controller.selectedVehicle)
                return
            var traj = controller.getConvertedTrajectory()
            if (!traj || traj.length === 0) {
                errorDialog.showError("未加载到有效轨迹点")
                return
            }
            mapDisplay.clearTrajectory()
            mapDisplay.addVehicleTrajectory(controller.selectedVehicle, traj, "#3498db")
        }

        function onTrajectoryConverted() {
            var traj = controller.getConvertedTrajectory()
            if (traj && traj.length > 0)
                mapDisplay.updateTrajectoryCoordinates(traj)
        }

        function onErrorOccurred(error) { errorDialog.showError(error) }

        function onVehiclePositionUpdated(plateNumber, position, direction, speed) {
            mapDisplay.updateVehiclePosition(plateNumber, position, direction, speed)
        }
    }
}
