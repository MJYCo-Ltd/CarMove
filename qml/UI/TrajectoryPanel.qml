import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CarMove 1.0

// 轨迹模式左侧面板：数据源设置 + 车辆列表 + 轨迹截取
SidePanelContainer {
    id: trajectoryPanel
    columnSpacing: 10

    signal openFolderDialogRequested()

    property var mapRef: null
    property int activePointCount: 0
    property bool suppressClipReload: false
    /// 当前车辆在数据源中的全量最早/最晚时间（时间轴总范围）
    property var vehicleDataStart: null
    property var vehicleDataEnd: null
    /// 当前截取开始/结束（与「轨迹截取」控件同步，供时间轴竖线使用）
    property var clipStart: null
    property var clipEnd: null
    readonly property bool databaseMode: controller ? controller.useDatabaseTrajectorySource : false

    function refreshActivePointCount() {
        activePointCount = controller ? controller.activeTrajectoryPointCount() : 0
    }

    function refreshClipMirror() {
        clipStart = parseDateTimeText(startTimeField.dateTimeText)
        clipEnd = parseDateTimeText(endTimeField.dateTimeText)
    }

    /// 端点切换：首点 → 到场=开始时间；末点 → 到场=结束时间
    function syncArrivalTimeFromEndpoint() {
        var sourceText = (endpointCombo.currentIndex === 1)
                         ? endTimeField.dateTimeText
                         : startTimeField.dateTimeText
        var dt = parseDateTimeText(sourceText)
        if (dt)
            arrivalTimeField.setFromDateTime(dt, true)
    }

    function clearVehicleDataRange() {
        vehicleDataStart = null
        vehicleDataEnd = null
        clipStart = null
        clipEnd = null
    }

    function resolvePlateNumber(inputPlate) {
        var plate = (inputPlate || "").trim()
        if (!plate || !controller)
            return ""

        var list = controller.vehicleList || []
        for (var i = 0; i < list.length; ++i) {
            if (String(list[i]).toUpperCase() === plate.toUpperCase())
                return list[i]
        }

        var filtered = controller.filteredVehicleList || []
        if (filtered.length === 1)
            return filtered[0]

        return plate
    }

    function parseDateTimeText(text) {
        var value = (text || "").trim()
        if (!value)
            return null
        var m = value.match(/^(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})$/)
        if (!m)
            return null
        var dt = new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]),
                          Number(m[4]), Number(m[5]), Number(m[6]))
        if (isNaN(dt.getTime()))
            return null
        return dt
    }

    function validateQueryTimeRange() {
        var startText = startTimeField.dateTimeText
        var endText = endTimeField.dateTimeText
        if (!startText && !endText)
            return ""

        if (!startText || !endText)
            return "请同时选择开始时间和结束时间"

        var startDt = parseDateTimeText(startText)
        var endDt = parseDateTimeText(endText)
        if (!startDt || !endDt)
            return "时间格式无效"

        if (endDt.getTime() < startDt.getTime())
            return "结束时间不能早于开始时间"

        var now = new Date()
        if (startDt.getTime() > now.getTime() || endDt.getTime() > now.getTime())
            return "开始/结束时间不能晚于当前时间"

        return ""
    }

    function applyVehicleDefaultTimeRange(plate) {
        if (!controller)
            return

        var range = controller.vehicleTrajectoryTimeRange(plate)
        if (!range) {
            clearVehicleDataRange()
            return
        }

        vehicleDataStart = range.startTime || null
        vehicleDataEnd = range.endTime || null

        if (range.startTime)
            startTimeField.setFromDateTime(range.startTime, false)
        if (range.endTime)
            endTimeField.setFromDateTime(range.endTime, false)
        refreshClipMirror()
        syncArrivalTimeFromEndpoint()
    }

    function applyClipTimesFromTimeline(startDate, endDate, reload) {
        if (!startDate || !endDate)
            return

        suppressClipReload = true
        startTimeField.setFromDateTime(startDate, false)
        endTimeField.setFromDateTime(endDate, false)
        refreshClipMirror()
        syncArrivalTimeFromEndpoint()
        suppressClipReload = false

        if (reload)
            loadTrajectoryForCurrentSelection(true)
    }

    function loadTrajectoryForCurrentSelection(showErrors) {
        if (!controller || controller.isLoading)
            return

        var plate = resolvePlateNumber(plateField.text)
        if (!plate) {
            if (showErrors)
                controller.reportError("请选择车牌号")
            return
        }

        var timeError = validateQueryTimeRange()
        if (timeError) {
            if (showErrors)
                controller.reportError(timeError)
            return
        }

        plateField.text = plate
        controller.selectVehicle(plate,
                                 startTimeField.dateTimeText,
                                 endTimeField.dateTimeText)
    }

    function selectVehicleFromList(plate) {
        if (!controller || controller.isLoading || !plate)
            return

        complementErrorStrip.text = ""
        plateField.text = plate
        controller.setSearchText(plate)

        suppressClipReload = true
        applyVehicleDefaultTimeRange(plate)
        suppressClipReload = false

        loadTrajectoryForCurrentSelection(true)
    }

    function onClipTimeChanged() {
        refreshClipMirror()
        if (suppressClipReload || !controller || controller.isLoading)
            return
        if (plateField.text.trim().length === 0)
            return
        loadTrajectoryForCurrentSelection(true)
    }

    function runComplementToTargetArea() {
        complementErrorStrip.text = ""
        if (!controller) {
            complementErrorStrip.text = "控制器未就绪"
            return
        }
        if (typeof routePlanner === "undefined" || !routePlanner) {
            complementErrorStrip.text = "路线规划服务不可用"
            return
        }
        if (routePlanner.busy) {
            complementErrorStrip.text = "正在规划中，请稍候"
            return
        }
        if (activePointCount <= 0) {
            complementErrorStrip.text = "请先选择车辆加载轨迹"
            return
        }

        var useLastPoint = endpointCombo.currentIndex === 1
        var routePoint = controller.trajectoryRoutePointWgs84(useLastPoint)
        if (!routePoint || routePoint.latitude === undefined || routePoint.longitude === undefined) {
            complementErrorStrip.text = "无法读取轨迹端点坐标"
            return
        }

        var destLat = controller.targetAreaLatitude
        var destLon = controller.targetAreaLongitude
        if (destLat === undefined || destLon === undefined
                || (Math.abs(destLat) < 1e-9 && Math.abs(destLon) < 1e-9)) {
            complementErrorStrip.text = "请先在搜索页设置目标区域"
            return
        }

        routePlanner.requestRoute(routePoint.longitude, routePoint.latitude,
                                  destLon, destLat, 0, "")
    }

    Connections {
        target: (typeof routePlanner !== "undefined") ? routePlanner : null
        enabled: trajectoryPanel.visible

        function onRouteReady(pathPoints) {
            complementErrorStrip.text = ""
            if (!mapRef || !pathPoints || pathPoints.length < 2)
                return
            mapRef.showComplementRoute(pathPoints, arrivalTimeField.dateTimeText)
        }

        function onRouteFailed(msg) {
            complementErrorStrip.text = msg || "路线规划失败"
        }
    }

    Connections {
        target: controller
        function onFilteredVehicleListChanged() {
            var plates = controller ? controller.filteredVehicleList : []
            vehicleListView.model = []
            vehicleListView.model = plates
        }
        function onSelectedVehicleChanged() {
            if (controller && controller.selectedVehicle
                    && plateField.text.trim() !== controller.selectedVehicle) {
                plateField.text = controller.selectedVehicle
            }
            if (!controller || !controller.selectedVehicle)
                trajectoryPanel.clearVehicleDataRange()
            trajectoryPanel.refreshActivePointCount()
        }
        function onTrajectoryLoaded(success, message) {
            trajectoryPanel.refreshActivePointCount()
        }
        function onLoadingChanged() {
            if (controller && !controller.isLoading)
                trajectoryPanel.refreshActivePointCount()
        }
    }

    GroupBox {
        title: "目标区域"
        Layout.fillWidth: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: "#2c3e50"
                text: {
                    if (!controller)
                        return "未设置"
                    var name = controller.targetAreaName
                    return (name && name.length > 0) ? name : "未设置"
                }
            }

            Button {
                text: "搜索更改"
                Layout.fillWidth: true
                onClicked: {
                    if (mapRef && mapRef.openTargetAreaSearch)
                        mapRef.openTargetAreaSearch()
                }
            }
        }
    }

    GroupBox {
        title: "轨迹数据源"
        Layout.fillWidth: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            ComboBox {
                id: sourceModeCombo
                Layout.fillWidth: true
                model: ["Excel 文件夹", "PostGIS 数据库"]
                enabled: controller && !controller.isLoading
                currentIndex: trajectoryPanel.databaseMode ? 1 : 0
                onActivated: function(index) {
                    if (!controller)
                        return
                    controller.setTrajectorySourceMode(index === 1 ? "database" : "folder")
                }
            }

            Button {
                text: "选择文件夹"
                Layout.fillWidth: true
                visible: !databaseMode
                enabled: controller ? !controller.isLoading : true
                onClicked: trajectoryPanel.openFolderDialogRequested()
            }

            Text {
                visible: !databaseMode
                text: (controller && controller.currentFolder) ? controller.currentFolder : "未选择文件夹"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: (controller && controller.currentFolder) ? "#2c3e50" : "#7f8c8d"
            }

            BusyIndicator {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                visible: !databaseMode && controller && controller.isLoading
                running: visible
            }

            Text {
                visible: !databaseMode && controller && controller.isLoading
                text: (controller && controller.loadingMessage) ? controller.loadingMessage : ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 10
                color: "#7f8c8d"
            }
        }
    }

    GroupBox {
        title: "轨迹查询"
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "车牌"
                    color: "#2c3e50"
                    Layout.preferredWidth: 32
                }

                TextField {
                    id: plateField
                    Layout.fillWidth: true
                    placeholderText: "输入或从下方列表选择车牌"
                    enabled: controller ? !controller.isLoading : true
                    onTextChanged: {
                        if (controller)
                            controller.setSearchText(text)
                    }
                }
            }

            GroupBox {
                title: "轨迹截取"
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    DateTimePickerField {
                        id: startTimeField
                        label: "开始"
                        defaultHour: 0
                        defaultMinute: 0
                        defaultSecond: 0
                        enabled: controller ? !controller.isLoading : true
                        onDateTimeChanged: trajectoryPanel.onClipTimeChanged()
                    }

                    DateTimePickerField {
                        id: endTimeField
                        label: "结束"
                        defaultHour: 23
                        defaultMinute: 59
                        defaultSecond: 59
                        enabled: controller ? !controller.isLoading : true
                        onDateTimeChanged: trajectoryPanel.onClipTimeChanged()
                    }
                }
            }

            GroupBox {
                title: "补全到目标区域轨迹"
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "端点"
                            color: "#2c3e50"
                            Layout.preferredWidth: 32
                        }

                        ComboBox {
                            id: endpointCombo
                            Layout.fillWidth: true
                            model: ["轨迹第一个点", "轨迹最后一个点"]
                            enabled: controller && !controller.isLoading
                                     && trajectoryPanel.activePointCount > 0
                            onCurrentIndexChanged: trajectoryPanel.syncArrivalTimeFromEndpoint()
                        }
                    }

                    DateTimePickerField {
                        id: arrivalTimeField
                        label: "到场时间"
                        labelWidth: 56
                        defaultHour: 12
                        defaultMinute: 0
                        defaultSecond: 0
                        enabled: controller && !controller.isLoading
                                 && trajectoryPanel.activePointCount > 0
                        onDateTimeChanged: {
                            if (mapRef)
                                mapRef.updateComplementArrivalTime(arrivalTimeField.dateTimeText)
                        }
                    }

                    Button {
                        text: (typeof routePlanner !== "undefined" && routePlanner && routePlanner.busy)
                              ? "规划中…" : "补全路线"
                        Layout.fillWidth: true
                        enabled: controller && !controller.isLoading
                                 && trajectoryPanel.activePointCount > 0
                                 && typeof routePlanner !== "undefined" && routePlanner
                                 && !routePlanner.busy
                        onClicked: trajectoryPanel.runComplementToTargetArea()
                    }

                    BusyIndicator {
                        Layout.alignment: Qt.AlignHCenter
                        visible: typeof routePlanner !== "undefined" && routePlanner && routePlanner.busy
                        running: visible
                        Layout.preferredHeight: 28
                    }

                    FormErrorStrip {
                        id: complementErrorStrip
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: controller ? ("车辆 " + controller.filteredVehicleList.length
                                   + " / " + controller.vehicleList.length) : ""
                font.pixelSize: 10
                color: "#7f8c8d"
                visible: controller && controller.vehicleList
                         && controller.vehicleList.length > 0
            }

            ListView {
                id: vehicleListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: []
                focus: true
                keyNavigationEnabled: true
                clip: true
                cacheBuffer: 0

                Component.onCompleted: {
                    model = controller ? controller.filteredVehicleList : []
                }

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (!controller)
                            return ""
                        if (trajectoryPanel.databaseMode) {
                            if (controller.isLoading)
                                return "正在连接 PostGIS 数据库..."
                            if (!controller.databaseConnected)
                                return "数据库未连接，请检查 CarMoveTracker.ini"
                            if (controller.vehicleList && controller.vehicleList.length === 0)
                                return "数据库中未找到车辆轨迹"
                        } else {
                            if (!controller.currentFolder)
                                return "请先选择包含车辆数据的文件夹"
                            if (controller.vehicleList && controller.vehicleList.length === 0)
                                return "该文件夹中未找到车辆数据"
                        }
                        if (plateField.text.trim().length > 0)
                            return "未找到匹配的车辆"
                        return ""
                    }
                    color: "#7f8c8d"
                    font.pixelSize: 12
                    visible: vehicleListView.count === 0
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    width: parent.width - 20
                }

                delegate: VehicleInfoCard {
                    width: vehicleListView.width
                    height: 60
                    plateNumber: modelData
                    isSelected: controller && controller.selectedVehicle === modelData
                    layoutMode: "horizontal"
                    showSelectionIndicator: true
                    onClicked: trajectoryPanel.selectVehicleFromList(modelData)
                }
            }
        }
    }
}
