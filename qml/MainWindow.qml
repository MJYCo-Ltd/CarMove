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

    Shortcut {
        sequence: "Ctrl+O"
        onActivated: {
            if (controller && controller.useDatabaseTrajectorySource)
                return
            folderDialog.open()
        }
    }
    Shortcut {
        sequence: "Space"
        onActivated: {
            if (!controller || !controller.playback || !mapDisplay.mapVehicleContextActive) return
            if (!mapDisplay.simulationPanelActive) return
            if (controller.playback.isPlaying) controller.playback.pausePlayback()
            else controller.playback.startPlayback()
        }
    }
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (mapDisplay.simulationPanelActive) {
                mapDisplay.closeSimulationPanel()
                return
            }
            if (controller && controller.playback)
                controller.playback.stopPlayback()
        }
    }

    property bool isBusinessMode: sidebarPanel.currentMode === "business"

    footer: ToolBar {
        visible: !mainWindow.isBusinessMode
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            /// 与 MapDisplay.mapVehicleContextActive、回放栏、坐标切换按钮同源
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
            Label {
                visible: mapDisplay.mapVehicleContextActive
                         && mapDisplay.trajectoryModeActive
                         && mapDisplay.simulationPanelActive
                text: (controller && controller.playback && controller.playback.isPlaying) ? "播放中" : "已暂停"
                color: (controller && controller.playback && controller.playback.isPlaying) ? "#27ae60" : "#7f8c8d"
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
                fuelRecordsPanel.visible = (mode === "fuel")
                geoSearchPanel.visible   = (mode === "search")
                navigationPanel.visible  = (mode === "nav")
                if (mode === "trajectory") {
                    mapDisplay.clearFuelMarkers()
                    mapDisplay.clearSearchResult()
                    mapDisplay.clearNavigationRoute()
                    mapDisplay.clearNavigationEndpointMarkers()
                } else if (mode === "business") {
                    mapDisplay.clearFuelMarkers()
                    mapDisplay.clearSearchResult()
                    mapDisplay.clearNavigationRoute()
                    mapDisplay.clearNavigationEndpointMarkers()
                } else if (mode === "fuel") {
                    mapDisplay.clearTrajectory()
                    mapDisplay.clearSearchResult()
                    mapDisplay.clearNavigationRoute()
                    mapDisplay.clearNavigationEndpointMarkers()
                } else if (mode === "nav") {
                    mapDisplay.clearFuelMarkers()
                    mapDisplay.clearSearchResult()
                }
            }
            onBusinessFileOpenRequested: businessPanel.openExcelFile()
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
            visible: sidebarPanel.currentMode === "trajectory"
            onOpenFolderDialogRequested: folderDialog.open()
        }

        FuelRecordsPanel {
            id: fuelRecordsPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: sidebarPanel.currentMode === "fuel"
            onVehicleSelected: function(pn) { mapDisplay.showVehicleFuelRecords(pn) }
            onShowAllRecords: mapDisplay.showAllFuelRecords()
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
            visible: !mainWindow.isBusinessMode

            MapDisplay {
                id: mapDisplay
                Layout.fillWidth: true
                Layout.fillHeight: true
                trajectoryModeActive: sidebarPanel.currentMode === "trajectory"
            }

            PlaybackControls {
                id: playbackControls
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                mapVehicleContextActive: mapDisplay.mapVehicleContextActive
                                         && mapDisplay.trajectoryModeActive
                                         && mapDisplay.simulationPanelActive
                onCloseRequested: mapDisplay.closeSimulationPanel()
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

        property bool running: false
        property var queue: []
        property int index: 0
        property string outputDir: ""
        property string restoreMode: "business"
        property int capturedCount: 0
        property int skippedCount: 0
        property string currentLabel: ""

        function normalizeFolderPath(path) {
            if (!path || !controller)
                return ""
            return controller.normalizeLocalPath(path)
        }

        function start(tasks, outputFolderPath) {
            if (!controller || !tasks || tasks.length === 0) {
                errorDialog.showError("没有可截图的业务行")
                return
            }

            const folder = normalizeFolderPath(outputFolderPath)
            if (!controller.ensureScreenshotOutputDirectory(folder)) {
                errorDialog.showError("无法创建截图输出目录")
                return
            }

            queue = tasks
            index = 0
            outputDir = folder
            capturedCount = 0
            skippedCount = 0
            running = true
            restoreMode = sidebarPanel.currentMode

            if (!controller.useDatabaseTrajectorySource)
                controller.setTrajectorySourceMode("database")

            if (!controller.databaseConnected)
                controller.connectPostGisDatabase()

            if (!controller.databaseConnected) {
                running = false
                errorDialog.showError("PostGIS 数据库未连接，请检查 CarMoveTracker.ini")
                return
            }

            sidebarPanel.activateMode("trajectory")
            mapDisplay.waitForMapSettled(function() {
                batchScreenshotController.processNext()
            }, 800, 1200, 8000)
        }

        function processNext() {
            if (!running || index >= queue.length) {
                finish()
                return
            }

            const task = queue[index]
            currentLabel = task.plate + "  " + task.startDate + " ~ " + task.endDate
            controller.loadTrajectoryForCapture(task.plate, task.startDate, task.endDate)
        }

        function onCaptureReady(success, pointCount) {
            if (!running)
                return

            if (!success || pointCount < 2) {
                skippedCount++
                index++
                Qt.callLater(processNext)
                return
            }

            const task = queue[index]
            const traj = controller.getConvertedTrajectory()
            mapDisplay.prepareTrajectoryForCapture(task.plate, traj, "#3498db")
            mapDisplay.waitForMapSettled(function() {
                batchScreenshotController.takeShotAndContinue()
            }, 1200, 2000, 12000)
        }

        function takeShotAndContinue() {
            if (!running || index >= queue.length)
                return

            const task = queue[index]
            const path = controller.screenshotFilePath(outputDir, task.plate, task.startDate, task.endDate)
            mapDisplay.captureScreenshotTo(path, function(ok) {
                if (ok)
                    capturedCount++
                else
                    skippedCount++
                index++
                processNext()
            })
        }

        function finish() {
            if (!running)
                return
            running = false
            currentLabel = ""
            mapDisplay.cancelMapSettleWait()
            mapDisplay.resetCaptureViewportMargin()
            sidebarPanel.activateMode(restoreMode)
            successDialog.showSuccess("截图完成：共 " + queue.length + " 条，成功 "
                                    + capturedCount + " 张，跳过 " + skippedCount + " 条")
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
            mapDisplay.closeSimulationPanel()
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
