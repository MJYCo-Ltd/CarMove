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
            visible: mainWindow.isBusinessMode
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
            visible: sidebarPanel.currentMode === "search"
            onLocateRequested: function(lat, lon, name) { mapDisplay.locateToPlace(lat, lon) }
            onTargetAreaRequested: function(lat, lon, name) { mapDisplay.setTargetAreaFromSearch(lat, lon, name) }
        }

        ColumnLayout {
            id: mapColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !mainWindow.isBusinessMode

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
            visible: sidebarPanel.currentMode === "nav"
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
        /// 当前批量截图阶段："" | "trajectory" | "targetArea"
        property string capturePhase: ""

        property real _perfBatchStartMs: 0
        property real _perfTaskStartMs: 0
        property real _perfLastMs: 0

        function _perfLog(step, detail) {
            const now = Date.now()
            const stepMs = _perfLastMs > 0 ? (now - _perfLastMs) : 0
            const taskMs = _perfTaskStartMs > 0 ? (now - _perfTaskStartMs) : 0
            const batchMs = _perfBatchStartMs > 0 ? (now - _perfBatchStartMs) : 0
            const detailText = (detail !== undefined && detail !== null && detail !== "") ? detail : ""
            console.log("[BatchShot]",
                        Qt.formatDateTime(new Date(now), "hh:mm:ss.zzz"),
                        step,
                        detailText,
                        "| step +" + stepMs + "ms",
                        "| task " + taskMs + "ms",
                        "| batch " + batchMs + "ms")
            _perfLastMs = now
        }

        function _perfResetTask() {
            _perfTaskStartMs = Date.now()
            _perfLastMs = _perfTaskStartMs
        }

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
            capturePhase = ""
            running = true
            businessPanel.mountMap(mapDisplay)
            _perfBatchStartMs = Date.now()
            _perfLastMs = _perfBatchStartMs
            _perfLog("batch.start", "outputDir=" + folder)

            if (!controller.useDatabaseTrajectorySource)
                controller.setTrajectorySourceMode("database")

            if (!controller.databaseConnected)
                controller.connectPostGisDatabase()

            if (!controller.databaseConnected) {
                running = false
                businessPanel.unmountMap(mapDisplay, mapColumn)
                excelModel.cancelScreenshotTasks()
                _perfLog("batch.failed", "PostGIS 未连接")
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
            _perfLog("task.loop", "processed=" + processedCount)

            const task = excelModel.nextScreenshotTask()
            if (!task || !task.plate) {
                _perfLog("task.queue.empty", "")
                finish()
                return
            }

            currentTask = task
            processedCount++
            _perfResetTask()
            _perfLog("task.begin", "#" + processedCount + " " + task.plate + " " + task.startDate + "~" + task.endDate)

            if (controller.screenshotFileExists(outputDir, task.plate, task.startDate, task.endDate)) {
                skippedCount++
                _perfLog("task.skip.exists", task.plate)
                processNext()
                return
            }

            currentLabel = task.plate + "  " + task.startDate + " ~ " + task.endDate
            _perfLog("trajectory.load.request", currentLabel)
            controller.loadTrajectoryForCapture(task.plate, task.startDate, task.endDate)
        }

        function onCaptureReady(success, pointCount) {
            if (!running)
                return

            mapDisplay.cancelTileWait()
            _perfLog("trajectory.load.ready", "success=" + success + " points=" + pointCount)

            if (!success || pointCount < 2) {
                skippedCount++
                _perfLog("task.skip.noTrajectory", currentLabel)
                processNext()
                return
            }

            if (!currentTask)
                return

            _perfLog("trajectory.capture.request", "points=" + pointCount)
            capturePhase = "trajectory"
            mapDisplay.beginBatchCaptureTrajectory(
                currentTask.plate,
                "#3498db",
                controller.screenshotFilePath(outputDir, currentTask.plate,
                                              currentTask.startDate, currentTask.endDate),
                "trajectory")
        }

        function onBatchCaptureFinished(success, captureLabel) {
            if (!running || !currentTask)
                return

            if (capturePhase === "trajectory") {
                if (!success) {
                    skippedCount++
                    _perfLog("trajectory.capture.failed", currentTask.plate)
                    capturePhase = ""
                    processNext()
                    return
                }
                capturedCount++
                _perfLog("trajectory.capture.done", currentTask.plate)
                beginTargetAreaCaptureIfNeeded()
                return
            }

            if (capturePhase === "targetArea") {
                if (success) {
                    targetCapturedCount++
                    _perfLog("targetArea.capture.done", currentTask.plate)
                } else {
                    _perfLog("targetArea.capture.failed", currentTask.plate)
                }
                capturePhase = ""
                processNext()
            }
        }

        function beginTargetAreaCaptureIfNeeded() {
            if (!running || !currentTask || !controller)
                return

            const visitCount = controller.targetAreaVisitCountForPlate(currentTask.plate)
            if (visitCount <= 0) {
                _perfLog("targetArea.skip", "visitCount=0")
                capturePhase = ""
                processNext()
                return
            }

            _perfLog("targetArea.capture.request", "visitCount=" + visitCount)
            capturePhase = "targetArea"
            mapDisplay.beginBatchCaptureTargetArea(
                controller.targetAreaScreenshotFilePath(outputDir, currentTask.plate,
                                                        currentTask.startDate, currentTask.endDate),
                "targetArea")
        }

        function finish() {
            if (!running)
                return
            _perfLog("batch.finish",
                     "processed=" + processedCount
                     + " captured=" + capturedCount
                     + " target=" + targetCapturedCount
                     + " skipped=" + skippedCount)
            running = false
            currentTask = null
            currentLabel = ""
            capturePhase = ""
            if (excelModel)
                excelModel.cancelScreenshotTasks()
            mapDisplay.cancelTileWait()
            mapDisplay.resetCaptureViewportMargin()
            businessPanel.unmountMap(mapDisplay, mapColumn)
            successDialog.showSuccess("截图完成：共处理 " + processedCount + " 条，大图 "
                                    + capturedCount + " 张，目标区小图 " + targetCapturedCount
                                    + " 张，跳过 " + skippedCount + " 条")
        }

        function stopSilently() {
            if (!running)
                return
            _perfLog("batch.stop", "")
            running = false
            currentTask = null
            currentLabel = ""
            capturePhase = ""
            if (excelModel)
                excelModel.cancelScreenshotTasks()
            mapDisplay.cancelTileWait()
            mapDisplay.resetCaptureViewportMargin()
            businessPanel.unmountMap(mapDisplay, mapColumn)
        }
    }

    Connections {
        target: mapDisplay
        function onBatchCaptureFinished(success, captureLabel) {
            batchScreenshotController.onBatchCaptureFinished(success, captureLabel)
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
            if (controller.activeTrajectoryPointCount() < 1) {
                errorDialog.showError("未加载到有效轨迹点")
                return
            }
            mapDisplay.clearTrajectory()
            mapDisplay.showSelectedVehicleTrajectory(controller.selectedVehicle, "#3498db")
        }

        function onTrajectoryConverted() {
            if (batchScreenshotController.running)
                return
            if (controller && controller.activeTrajectoryPointCount() > 0)
                mapDisplay.refreshSelectedVehicleTrajectory()
        }

        function onErrorOccurred(error) { errorDialog.showError(error) }

        function onVehiclePositionUpdated(plateNumber, position, direction, speed, timestamp) {
            mapDisplay.updateVehiclePosition(plateNumber, position, direction, speed, timestamp)
        }
    }
}
