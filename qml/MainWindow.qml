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
    
    // Keyboard shortcuts
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: folderDialog.open()
    }
    
    Shortcut {
        sequence: "Space"
        onActivated: {
            if (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle) {
                if (typeof controller.isPlaying !== 'undefined' && controller.isPlaying) {
                    if (typeof controller.pausePlayback === 'function') {
                        controller.pausePlayback()
                    }
                } else {
                    if (typeof controller.startPlayback === 'function') {
                        controller.startPlayback()
                    }
                }
            }
        }
    }
    
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (controller && typeof controller.stopPlayback === 'function') {
                controller.stopPlayback()
            }
        }
    }
    
    // Status bar for displaying information
    footer: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: 5
            
            Label {
                text: (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle) ? 
                      "已选择车辆: " + controller.selectedVehicle : 
                      "未选择车辆"
                Layout.fillWidth: true
            }
            
            Label {
                text: (controller && controller.vehicleList && typeof controller.vehicleList.length !== 'undefined') ? 
                      "车辆数量: " + controller.vehicleList.length : 
                      "无车辆数据"
            }
            
            Label {
                text: (controller && typeof controller.isPlaying !== 'undefined' && controller.isPlaying) ? "播放中" : "已暂停"
                color: (controller && typeof controller.isPlaying !== 'undefined' && controller.isPlaying) ? "#27ae60" : "#7f8c8d"
            }
        }
    }
    
    RowLayout {
        anchors.fill: parent
        spacing: 0
        
        // 左侧功能侧边栏
        SidebarPanel {
            id: sidebarPanel
            Layout.preferredWidth: 60
            Layout.fillHeight: true
            
            onModeChanged: function(mode) {
                console.log("切换到模式:", mode)
                if (mode === "trajectory") {
                    leftPanel.visible = true
                    fuelRecordsPanel.visible = false
                    searchPanel.visible = false
                    // 清除卸油标记和搜索结果
                    mapDisplay.clearFuelMarkers()
                    mapDisplay.clearSearchResult()
                } else if (mode === "fuel") {
                    leftPanel.visible = false
                    fuelRecordsPanel.visible = true
                    searchPanel.visible = false
                    // 清除轨迹和搜索结果
                    mapDisplay.clearTrajectory()
                    mapDisplay.clearSearchResult()
                } else if (mode === "search") {
                    leftPanel.visible = false
                    fuelRecordsPanel.visible = false
                    searchPanel.visible = true
                }
            }
        }
        
        // 左侧面板：文件夹选择和车辆列表（轨迹模式）
        Rectangle {
            id: leftPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: "#f0f0f0"
            border.color: "#ccc"
            visible: sidebarPanel.currentMode === "trajectory"
            
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                
                // 文件夹选择区域
                GroupBox {
                    title: "数据文件夹"
                    Layout.fillWidth: true
                    
                    ColumnLayout {
                        anchors.fill: parent
                        
                        Button {
                            text: "选择文件夹"
                            Layout.fillWidth: true
                            enabled: controller ? (typeof controller.isLoading !== 'undefined' ? !controller.isLoading : true) : true
                            onClicked: folderDialog.open()
                        }
                        
                        Text {
                            text: (controller && typeof controller.currentFolder !== 'undefined' && controller.currentFolder) ? controller.currentFolder : "未选择文件夹"
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            color: (controller && typeof controller.currentFolder !== 'undefined' && controller.currentFolder) ? "#2c3e50" : "#7f8c8d"
                        }
                        
                        // Loading indicator
                        BusyIndicator {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 30
                            visible: controller ? (typeof controller.isLoading !== 'undefined' ? controller.isLoading : false) : false
                            running: controller ? (typeof controller.isLoading !== 'undefined' ? controller.isLoading : false) : false
                        }
                        
                        // Progress text
                        Text {
                            text: (controller && typeof controller.loadingMessage !== 'undefined' && controller.loadingMessage) ? controller.loadingMessage : ""
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            font.pixelSize: 10
                            color: "#7f8c8d"
                            visible: controller ? (typeof controller.isLoading !== 'undefined' ? controller.isLoading : false) : false
                        }
                    }
                }
                
                // 车辆列表区域
                GroupBox {
                    id: vehicleListGroupBox
                    title: "车辆列表"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    
                    // 更新车辆列表 ListView 的 model（按 searchText 前缀过滤）
                    function updateVehicleListModel() {
                        if (!controller) {
                            vehicleListView.model = []
                            return
                        }
                        vehicleListView.model = controller.filteredVehicleList ? controller.filteredVehicleList : []
                    }
                    
                    Component.onCompleted: updateVehicleListModel()
                    
                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 5
                        
                        // 搜索框
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 5
                            
                            TextField {
                                id: searchField
                                Layout.fillWidth: true
                                placeholderText: "输入车牌号前缀搜索 (如: 冀A)..."
                                // 无焦点时从 controller 同步（如点击清除后）；有焦点时仅由 onTextChanged 写入 controller
                                text: (controller && controller.searchText !== undefined) ? controller.searchText : ""
                                
                                // Add search icon
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
                                
                                onTextChanged: {
                                    if (controller) {
                                        controller.setSearchText(text)
                                        vehicleListGroupBox.updateVehicleListModel()
                                    }
                                }
                                
                                // 从 controller 同步到输入框（清除、外部设置等）
                                Connections {
                                    target: controller
                                    function onSearchTextChanged() {
                                        if (!searchField.activeFocus && controller)
                                            searchField.text = controller.searchText
                                    }
                                }
                                
                                // Add keyboard shortcuts
                                Keys.onEscapePressed: {
                                    if (controller && typeof controller.clearSearch === 'function') {
                                        controller.clearSearch()
                                    }
                                }
                            }
                            
                            Button {
                                text: "清除"
                                enabled: searchField.text.length > 0
                                Layout.preferredWidth: 50
                                
                                onClicked: {
                                    if (controller && typeof controller.clearSearch === 'function') {
                                        controller.clearSearch()
                                    }
                                    searchField.text = ""
                                }
                            }
                        }
                        
                        // Search results info（与下方 ListView 使用相同过滤逻辑）
                        Text {
                            Layout.fillWidth: true
                            text: {
                                if (!controller) return ""
                                return "找到 " + controller.filteredVehicleList.length + " / " + controller.vehicleList.length + " 辆车"
                            }
                            font.pixelSize: 10
                            color: "#7f8c8d"
                            visible: controller && controller.searchText && String(controller.searchText).trim().length > 0
                        }
                        
                        // 车辆列表：model 由 updateVehicleListModel() 方法更新
                        ListView {
                            id: vehicleListView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            model: []
                            focus: true
                            keyNavigationEnabled: true
                            clip: true
                            
                            // Empty state
                            Text {
                                anchors.centerIn: parent
                                text: {
                                    if (!controller || typeof controller.currentFolder === 'undefined' || !controller.currentFolder) {
                                        return "请先选择包含车辆数据的文件夹"
                                    } else if (controller.vehicleList && controller.vehicleList.length === 0) {
                                        return "该文件夹中未找到车辆数据"
                                    } else if (controller.searchText && controller.searchText.length > 0) {
                                        return "未找到匹配的车辆"
                                    } else {
                                        return ""
                                    }
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
                                isSelected: controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle === modelData
                                layoutMode: "horizontal"
                                showSelectionIndicator: true
                                
                                onClicked: {
                                    if (controller && typeof controller.selectVehicle === 'function') {
                                        controller.selectVehicle(modelData)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // 卸油记录面板（卸油模式）
        FuelRecordsPanel {
            id: fuelRecordsPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            visible: sidebarPanel.currentMode === "fuel"
            
            onVehicleSelected: function(plateNumber) {
                console.log("选择车辆:", plateNumber)
                mapDisplay.showVehicleFuelRecords(plateNumber)
            }
            
            onShowAllRecords: {
                console.log("显示所有卸油记录")
                mapDisplay.showAllFuelRecords()
            }
        }

        // 地点搜索面板（搜索模式）
        Rectangle {
            id: searchPanel
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: "#f0f0f0"
            border.color: "#ccc"
            visible: false

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                // 标题
                Text {
                    text: "地点搜索"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#2c3e50"
                    Layout.fillWidth: true
                }

                // 关键词输入
                GroupBox {
                    title: "搜索关键词"
                    Layout.fillWidth: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 6

                        TextField {
                            id: geoKeywordField
                            Layout.fillWidth: true
                            placeholderText: "输入地点名称，如：火车站"
                            Keys.onReturnPressed: doGeoSearch()
                            Keys.onEnterPressed: doGeoSearch()
                        }

                        Text {
                            text: "行政区范围"
                            font.pixelSize: 12
                            color: "#2c3e50"
                        }

                        // 行政区模糊匹配输入框
                        Item {
                            Layout.fillWidth: true
                            height: adminRegionField.height

                            TextField {
                                id: adminRegionField
                                width: parent.width
                                placeholderText: "输入省/市名称模糊匹配，如：天津"
                                Keys.onReturnPressed: doGeoSearch()
                                Keys.onEnterPressed: doGeoSearch()
                                Keys.onDownPressed: {
                                    if (regionSuggestList.count > 0) {
                                        regionSuggestList.currentIndex = 0
                                        regionSuggestList.forceActiveFocus()
                                    }
                                }
                                onTextChanged: regionSuggestPopup.updateSuggestions(text)
                            }

                            // 候选列表弹出层
                            Rectangle {
                                id: regionSuggestPopup
                                visible: regionSuggestList.count > 0 && adminRegionField.activeFocus
                                         || regionSuggestList.activeFocus
                                width: adminRegionField.width
                                height: Math.min(regionSuggestList.count, 6) * 32
                                anchors.top: adminRegionField.bottom
                                anchors.left: adminRegionField.left
                                z: 999
                                color: "white"
                                border.color: "#bdc3c7"
                                border.width: 1
                                radius: 3
                                clip: true

                                property var allNames: []

                                Component.onCompleted: {
                                    allNames = (typeof geocoder !== 'undefined' && geocoder)
                                               ? geocoder.adminRegionNames() : []
                                }

                                function updateSuggestions(input) {
                                    var filtered = []
                                    var kw = input.trim()
                                    if (kw.length === 0) {
                                        regionSuggestModel.clear()
                                        return
                                    }
                                    for (var i = 0; i < allNames.length; i++) {
                                        if (allNames[i].indexOf(kw) >= 0) {
                                            filtered.push(allNames[i])
                                            if (filtered.length >= 50) break
                                        }
                                    }
                                    regionSuggestModel.clear()
                                    for (var j = 0; j < filtered.length; j++) {
                                        regionSuggestModel.append({ name: filtered[j] })
                                    }
                                }

                                ListModel { id: regionSuggestModel }

                                ListView {
                                    id: regionSuggestList
                                    anchors.fill: parent
                                    model: regionSuggestModel
                                    clip: true
                                    keyNavigationEnabled: true

                                    Keys.onReturnPressed: selectCurrentSuggestion()
                                    Keys.onEnterPressed:  selectCurrentSuggestion()
                                    Keys.onEscapePressed: {
                                        regionSuggestModel.clear()
                                        adminRegionField.forceActiveFocus()
                                    }

                                    function selectCurrentSuggestion() {
                                        if (currentIndex >= 0 && currentIndex < count) {
                                            adminRegionField.text = regionSuggestModel.get(currentIndex).name
                                            regionSuggestModel.clear()
                                            adminRegionField.forceActiveFocus()
                                        }
                                    }

                                    delegate: Rectangle {
                                        width: regionSuggestList.width
                                        height: 32
                                        color: regionSuggestList.currentIndex === index
                                               ? "#d6eaf8" : (maArea.containsMouse ? "#eaf4fc" : "white")

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            anchors.left: parent.left
                                            anchors.leftMargin: 8
                                            anchors.right: parent.right
                                            anchors.rightMargin: 8
                                            text: model.name
                                            font.pixelSize: 12
                                            color: "#2c3e50"
                                            elide: Text.ElideRight
                                        }

                                        MouseArea {
                                            id: maArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                adminRegionField.text = model.name
                                                regionSuggestModel.clear()
                                                adminRegionField.forceActiveFocus()
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Button {
                            text: (typeof geocoder !== 'undefined' && geocoder && geocoder.busy) ? "搜索中..." : "搜索"
                            Layout.fillWidth: true
                            enabled: geoKeywordField.text.trim().length > 0 &&
                                     adminRegionField.text.trim().length > 0 &&
                                     !(typeof geocoder !== 'undefined' && geocoder && geocoder.busy)
                            onClicked: doGeoSearch()
                        }
                    }
                }

                // 忙碌指示器
                BusyIndicator {
                    Layout.alignment: Qt.AlignHCenter
                    visible: typeof geocoder !== 'undefined' && geocoder && geocoder.busy
                    running: visible
                    Layout.preferredHeight: 32
                }

                // 搜索结果列表
                GroupBox {
                    title: searchResultModel.count > 0
                           ? ("搜索结果（" + searchResultModel.count + " 条）")
                           : "搜索结果"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    visible: searchResultModel.count > 0

                    ListModel { id: searchResultModel }

                    ListView {
                        id: searchResultListView
                        anchors.fill: parent
                        model: searchResultModel
                        clip: true
                        spacing: 6

                        delegate: Rectangle {
                            width: searchResultListView.width
                            height: itemCol.implicitHeight + 20
                            color: "#ffffff"
                            border.color: "#dce0e8"
                            border.width: 1
                            radius: 4

                            Column {
                                id: itemCol
                                anchors {
                                    left: parent.left; right: parent.right
                                    top: parent.top; margins: 8
                                }
                                spacing: 3

                                Text {
                                    width: parent.width
                                    text: model.name
                                    font.bold: true
                                    font.pixelSize: 12
                                    color: "#2c3e50"
                                    wrapMode: Text.WordWrap
                                }

                                Text {
                                    width: parent.width
                                    text: model.address
                                    font.pixelSize: 11
                                    color: "#7f8c8d"
                                    wrapMode: Text.WordWrap
                                    visible: model.address.length > 0
                                }

                                Text {
                                    width: parent.width
                                    text: model.latitude.toFixed(6) + ", " + model.longitude.toFixed(6)
                                    font.pixelSize: 10
                                    color: "#95a5a6"
                                }

                                Button {
                                    text: "在地图上定位"
                                    height: 26
                                    font.pixelSize: 11
                                    onClicked: mapDisplay.showSearchResult(
                                        model.latitude, model.longitude, model.name)
                                }
                            }
                        }
                    }
                }

                // 错误提示
                Rectangle {
                    id: searchErrorBox
                    Layout.fillWidth: true
                    height: searchErrorText.implicitHeight + 16
                    color: "#fdecea"
                    border.color: "#e74c3c"
                    border.width: 1
                    radius: 4
                    visible: searchErrorText.text.length > 0

                    Text {
                        id: searchErrorText
                        anchors.fill: parent
                        anchors.margins: 8
                        text: ""
                        font.pixelSize: 11
                        color: "#c0392b"
                        wrapMode: Text.WordWrap
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }
        
        // 右侧：地图和控制面板
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // 地图显示区域
            MapDisplay {
                id: mapDisplay
                Layout.fillWidth: true
                Layout.fillHeight: true
            }
            
            // 播放控制面板
            PlaybackControls {
                id: playbackControls
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                
                // Handle coordinate conversion toggle
                onCoordinateConversionToggled: {
                    // Update map display with converted trajectory
                    if (controller && typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle &&
                        typeof controller.getConvertedTrajectory === 'function') {
                        mapDisplay.updateTrajectoryCoordinates(controller.getConvertedTrajectory())
                    }
                    
                    // Update fuel unloading display with converted coordinates
                    if (sidebarPanel.currentMode === "fuel") {
                        // The FuelUnloadingDisplay will automatically update via the Connections
                        console.log("坐标转换状态已更新，卸油标记将自动更新")
                    }
                }
            }
        }
    }
    
    // 文件夹选择对话框
    FolderDialog {
        id: folderDialog
        title: "选择包含车辆数据的文件夹"
        onAccepted: {
            if (controller && typeof controller.selectFolder === 'function') {
                controller.selectFolder(selectedFolder)
            }
        }
    }
    
    // Error and success notifications
    NotificationDialog {
        id: errorDialog
        onOpened: {
            showError(errorMessage)
        }
        
        property string errorMessage: ""
        
        function showErrorMessage(message) {
            errorMessage = message
            showError(message)
        }
    }
    
    NotificationDialog {
        id: successDialog
        onOpened: {
            showSuccess(successMessage)
        }
        
        property string successMessage: ""
        
        function showSuccessMessage(message) {
            successMessage = message
            showSuccess(message)
        }
    }
    
    
    // Connect to controller signals
    Connections {
        target: controller
        
        function onSearchTextChanged() {
            vehicleListGroupBox.updateVehicleListModel()
        }
        
        function onVehicleListChanged() {
            vehicleListGroupBox.updateVehicleListModel()
        }
        
        function onFolderScanned(success, message) {
            if (success) {
                successDialog.showSuccessMessage(message)
            } else {
                errorDialog.showErrorMessage(message)
            }
        }
        
        function onTrajectoryLoaded(success, message) {
            if (success) {
                console.log("Trajectory loaded successfully:", message)
                // Update map display with the loaded trajectory
                if (controller && typeof controller.getConvertedTrajectory === 'function' && 
                    typeof controller.selectedVehicle !== 'undefined' && controller.selectedVehicle) {
                    var trajectory = controller.getConvertedTrajectory()
                    if (trajectory && trajectory.length > 0) {
                        mapDisplay.addVehicleTrajectory(controller.selectedVehicle, trajectory, "#3498db")
                    }
                }
            } else {
                errorDialog.showErrorMessage(message)
            }
        }
        
        function onTrajectoryConverted() {
            console.log("Trajectory converted, updating map display")
            // Update map display with converted coordinates
            if (controller && typeof controller.getConvertedTrajectory === 'function') {
                var trajectory = controller.getConvertedTrajectory()
                if (trajectory && trajectory.length > 0) {
                    mapDisplay.updateTrajectoryCoordinates(trajectory)
                }
            }
        }
        
        function onErrorOccurred(error) {
            errorDialog.showErrorMessage(error)
        }
        
        function onVehiclePositionUpdated(plateNumber, position, direction, speed) {
            // Forward to map display for real-time position updates
            mapDisplay.updateVehiclePosition(plateNumber, position, direction, speed)
        }
        
        function onSelectedVehicleChanged() {
            // Clear map when vehicle selection changes
            if (!controller || typeof controller.selectedVehicle === 'undefined' || !controller.selectedVehicle) {
                mapDisplay.clearTrajectory()
            }
        }
        
        function onCurrentTimeChanged() {
            // Update UI elements that depend on current time
            if (controller && typeof controller.currentTime !== 'undefined') {
                console.log("Current time changed:", controller.currentTime)
            }
        }
        
        function onPlaybackStateChanged() {
            // Update UI elements that depend on playback state
            if (controller && typeof controller.isPlaying !== 'undefined') {
                console.log("Playback state changed, isPlaying:", controller.isPlaying)
            }
        }
        
    }

    // 地理搜索函数
    function doGeoSearch() {
        var keyword = geoKeywordField.text.trim()
        var region = adminRegionField.text.trim()
        if (keyword.length === 0 || region.length === 0) return

        // 清空上次结果和错误
        searchResultModel.clear()
        searchErrorText.text = ""

        geocoder.searchInAdminRegion(keyword, region)
    }

    // 连接 geocoder 信号
    Connections {
        target: (typeof geocoder !== 'undefined') ? geocoder : null

        function onGeocodeResultsReady(results) {
            searchResultModel.clear()
            searchErrorText.text = ""
            for (var i = 0; i < results.length; i++) {
                var r = results[i]
                searchResultModel.append({
                    name:      r.name,
                    address:   r.address,
                    latitude:  r.latitude,
                    longitude: r.longitude
                })
            }
            // 自动定位到第一条结果
            if (results.length > 0) {
                mapDisplay.showSearchResult(results[0].latitude, results[0].longitude, results[0].name)
            }
        }

        function onGeocodeFailed(errorMessage) {
            searchErrorText.text = errorMessage
            searchResultModel.clear()
        }
    }
}
