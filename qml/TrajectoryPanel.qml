import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CarMove 1.0

// 轨迹模式左侧面板：数据源设置 + 车辆列表
SidePanelContainer {
    id: trajectoryPanel
    columnSpacing: 10

    signal openFolderDialogRequested()

    readonly property var cfg: controller ? controller.configManager : null
    readonly property bool databaseMode: controller ? controller.useDatabaseTrajectorySource : false

    function updateVehicleListModel() {
        vehicleListView.model = controller ? (controller.filteredVehicleList || []) : []
    }

    function saveDatabaseSettings() {
        if (controller)
            controller.savePostGisSettings()
    }

    Connections {
        target: controller
        function onSearchTextChanged() { trajectoryPanel.updateVehicleListModel() }
        function onVehicleListChanged() { trajectoryPanel.updateVehicleListModel() }
        function onTrajectorySourceModeChanged() { trajectoryPanel.updateVehicleListModel() }
    }

    Component.onCompleted: updateVehicleListModel()

    GroupBox {
        title: "轨迹数据源"
        Layout.fillWidth: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                RadioButton {
                    id: folderSourceRadio
                    text: "Excel 文件夹"
                    checked: !trajectoryPanel.databaseMode
                    enabled: controller && !controller.isLoading
                    onClicked: {
                        if (controller)
                            controller.setTrajectorySourceMode("folder")
                    }
                }

                RadioButton {
                    id: databaseSourceRadio
                    text: "PostGIS 数据库"
                    checked: trajectoryPanel.databaseMode
                    enabled: controller && !controller.isLoading
                    onClicked: {
                        if (controller)
                            controller.setTrajectorySourceMode("database")
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                color: "#7f8c8d"
                text: databaseMode
                      ? "从 PostgreSQL/PostGIS 读取轨迹；配置保存在 CarMoveTracker.ini。"
                      : "从本地 Excel 文件夹读取轨迹文件。"
            }
        }
    }

    GroupBox {
        title: "Excel 文件夹"
        Layout.fillWidth: true
        visible: !databaseMode

        ColumnLayout {
            anchors.fill: parent

            Button {
                text: "选择文件夹"
                Layout.fillWidth: true
                enabled: controller ? !controller.isLoading : true
                onClicked: trajectoryPanel.openFolderDialogRequested()
            }

            Text {
                text: (controller && controller.currentFolder) ? controller.currentFolder : "未选择文件夹"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: (controller && controller.currentFolder) ? "#2c3e50" : "#7f8c8d"
            }
        }
    }

    GroupBox {
        title: "PostGIS 数据库"
        Layout.fillWidth: true
        visible: databaseMode

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "主机"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    placeholderText: "localhost"
                    text: cfg ? cfg.dbHost : ""
                    onTextChanged: if (cfg) cfg.dbHost = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "端口"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.preferredWidth: 80
                    font.pixelSize: 12
                    placeholderText: "5432"
                    text: cfg ? String(cfg.dbPort) : "5432"
                    inputMethodHints: Qt.ImhDigitsOnly
                    onTextChanged: if (cfg) cfg.dbPort = parseInt(text) || 5432
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
                Text { text: "数据库"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    placeholderText: "carmove"
                    text: cfg ? cfg.dbName : ""
                    onTextChanged: if (cfg) cfg.dbName = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "用户"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    text: cfg ? cfg.dbUser : ""
                    onTextChanged: if (cfg) cfg.dbUser = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "密码"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    echoMode: TextInput.Password
                    text: cfg ? cfg.dbPassword : ""
                    onTextChanged: if (cfg) cfg.dbPassword = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "模式"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.preferredWidth: 80
                    font.pixelSize: 12
                    text: cfg ? cfg.dbSchema : "public"
                    onTextChanged: if (cfg) cfg.dbSchema = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
                Text { text: "轨迹表"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    text: cfg ? cfg.dbTrajectoryTable : "trajectory_points"
                    onTextChanged: if (cfg) cfg.dbTrajectoryTable = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text { text: "车辆表"; Layout.preferredWidth: 48; font.pixelSize: 12 }
                TextField {
                    Layout.fillWidth: true
                    font.pixelSize: 12
                    text: cfg ? cfg.dbVehiclesTable : "vehicles"
                    onTextChanged: if (cfg) cfg.dbVehiclesTable = text
                    onEditingFinished: trajectoryPanel.saveDatabaseSettings()
                }
            }

            Button {
                text: controller && controller.isLoading ? "连接中..." : "连接数据库"
                Layout.fillWidth: true
                enabled: controller && !controller.isLoading && cfg
                onClicked: {
                    trajectoryPanel.saveDatabaseSettings()
                    if (controller)
                        controller.connectPostGisDatabase()
                }
            }

            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 11
                color: controller && controller.databaseConnected ? "#1e8449" : "#7f8c8d"
                text: {
                    if (!controller)
                        return ""
                    if (controller.databaseConnected)
                        return "已连接：" + controller.databaseStatus
                    if (controller.databaseStatus && controller.databaseStatus.length > 0)
                        return controller.databaseStatus
                    return "未连接数据库"
                }
            }
        }
    }

    GroupBox {
        title: "加载状态"
        Layout.fillWidth: true

        ColumnLayout {
            anchors.fill: parent

            BusyIndicator {
                Layout.fillWidth: true
                Layout.preferredHeight: 30
                visible: controller ? controller.isLoading : false
                running: visible
            }

            Text {
                text: (controller && controller.loadingMessage) ? controller.loadingMessage : ""
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                font.pixelSize: 10
                color: "#7f8c8d"
                visible: controller ? controller.isLoading : false
            }
        }
    }

    GroupBox {
        title: "车辆列表"
        Layout.fillWidth: true
        Layout.fillHeight: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                TextField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: "输入车牌号搜索 (如: 冀A)..."
                    text: (controller && controller.searchText !== undefined) ? controller.searchText : ""
                    leftPadding: 30

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 8
                        width: 16
                        height: 16
                        color: "transparent"
                        Text {
                            anchors.centerIn: parent
                            text: "🔍"
                            font.pixelSize: 12
                            color: "#7f8c8d"
                        }
                    }

                    onTextChanged: { if (controller) controller.setSearchText(text) }
                    Keys.onEscapePressed: { if (controller) controller.clearSearch() }

                    Connections {
                        target: controller
                        function onSearchTextChanged() {
                            if (!searchField.activeFocus && controller)
                                searchField.text = controller.searchText
                        }
                    }
                }

                Button {
                    text: "清除"
                    enabled: searchField.text.length > 0
                    Layout.preferredWidth: 50
                    onClicked: { if (controller) controller.clearSearch(); searchField.text = "" }
                }
            }

            Text {
                Layout.fillWidth: true
                text: controller ? ("找到 " + controller.filteredVehicleList.length + " / " + controller.vehicleList.length + " 辆车") : ""
                font.pixelSize: 10
                color: "#7f8c8d"
                visible: controller && controller.searchText && String(controller.searchText).trim().length > 0
            }

            ListView {
                id: vehicleListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: []
                focus: true
                keyNavigationEnabled: true
                clip: true

                Text {
                    anchors.centerIn: parent
                    text: {
                        if (!controller)
                            return ""
                        if (trajectoryPanel.databaseMode) {
                            if (!controller.databaseConnected)
                                return "请先连接 PostGIS 数据库"
                            if (controller.vehicleList && controller.vehicleList.length === 0)
                                return "数据库中未找到车辆轨迹"
                        } else {
                            if (!controller.currentFolder)
                                return "请先选择包含车辆数据的文件夹"
                            if (controller.vehicleList && controller.vehicleList.length === 0)
                                return "该文件夹中未找到车辆数据"
                        }
                        if (controller.searchText && controller.searchText.length > 0)
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
                    onClicked: { if (controller) controller.selectVehicle(modelData) }
                }
            }
        }
    }
}
