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

    Shortcut { sequence: "Ctrl+O"; onActivated: folderDialog.open() }
    Shortcut {
        sequence: "Space"
        onActivated: {
            if (!controller || !controller.playback || !mapDisplay.mapVehicleContextActive) return
            if (controller.playback.isPlaying) controller.playback.pausePlayback()
            else controller.playback.startPlayback()
        }
    }
    Shortcut { sequence: "Escape"; onActivated: { if (controller && controller.playback) controller.playback.stopPlayback() } }

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
                text: (controller && controller.vehicleList)
                      ? ("车辆数量: " + controller.vehicleList.length)
                      : "无车辆数据"
            }
            Label {
                visible: mapDisplay.mapVehicleContextActive
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

    Connections {
        target: controller

        function onFolderScanned(success, message) {
            if (success) successDialog.showSuccess(message)
            else errorDialog.showError(message)
        }

        function onTrajectoryLoaded(success, message) {
            if (!success) { errorDialog.showError(message); return }
            if (controller && controller.selectedVehicle) {
                var traj = controller.getConvertedTrajectory()
                if (traj && traj.length > 0)
                    mapDisplay.addVehicleTrajectory(controller.selectedVehicle, traj, "#3498db")
            }
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

        function onSelectedVehicleChanged() {
            if (!controller || !controller.selectedVehicle) mapDisplay.clearTrajectory()
        }
    }
}
