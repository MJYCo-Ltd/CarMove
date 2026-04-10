import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CarMove 1.0

// 轨迹模式左侧面板：文件夹选择 + 车辆列表
Rectangle {
    id: trajectoryPanel
    color: "#f0f0f0"
    border.color: "#ccc"

    signal openFolderDialogRequested()

    function updateVehicleListModel() {
        vehicleListView.model = controller ? (controller.filteredVehicleList || []) : []
    }

    Connections {
        target: controller
        function onSearchTextChanged() { trajectoryPanel.updateVehicleListModel() }
        function onVehicleListChanged()  { trajectoryPanel.updateVehicleListModel() }
    }

    Component.onCompleted: updateVehicleListModel()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10

        GroupBox {
            title: "数据文件夹"
            Layout.fillWidth: true

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
                            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: 8; width: 16; height: 16; color: "transparent"
                            Text { anchors.centerIn: parent; text: "🔍"; font.pixelSize: 12; color: "#7f8c8d" }
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
                    font.pixelSize: 10; color: "#7f8c8d"
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
                            if (!controller || !controller.currentFolder) return "请先选择包含车辆数据的文件夹"
                            if (controller.vehicleList && controller.vehicleList.length === 0) return "该文件夹中未找到车辆数据"
                            if (controller.searchText && controller.searchText.length > 0) return "未找到匹配的车辆"
                            return ""
                        }
                        color: "#7f8c8d"; font.pixelSize: 12
                        visible: vehicleListView.count === 0
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap; width: parent.width - 20
                    }

                    delegate: VehicleInfoCard {
                        width: vehicleListView.width; height: 60
                        plateNumber: modelData
                        isSelected: controller && controller.selectedVehicle === modelData
                        layoutMode: "horizontal"; showSelectionIndicator: true
                        onClicked: { if (controller) controller.selectVehicle(modelData) }
                    }
                }
            }
        }
    }
}
