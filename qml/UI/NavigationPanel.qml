import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 驾车导航：起点 / 终点 / 途经点走天地图规划；
// 自定义点按添加顺序追加在规划路线之后；有自定义点时用特殊航线样式（蓝线+车辆）
Rectangle {
    id: navigationPanel
    color: "#f0f0f0"
    border.color: "#ccc"

    property var mapRef: null

    property bool originSet: false
    property double originLon: 0
    property double originLat: 0
    property string originName: ""

    property bool destSet: false
    property double destLon: 0
    property double destLat: 0
    property string destName: ""

    /// -1=未在地图选点；0 起点 / 1 终点 / 2 途经点 / 3 自定义点
    property int mapPickRole: -1

    ListModel { id: waypointModel }
    ListModel { id: customPointModel }

    function hasCustomPoints() {
        return customPointModel.count > 0
    }

    function clearWaypointModel() {
        while (waypointModel.count > 0)
            waypointModel.remove(0)
    }

    function clearCustomPointModel() {
        while (customPointModel.count > 0)
            customPointModel.remove(0)
    }

    function clearAllNavPoints() {
        cancelMapPick()
        originSet = false
        originLon = 0
        originLat = 0
        originName = ""
        destSet = false
        destLon = 0
        destLat = 0
        destName = ""
        navigationPanel.clearWaypointModel()
        navigationPanel.clearCustomPointModel()
        if (mapRef) {
            mapRef.clearNavigationEndpointMarkers()
            mapRef.clearNavigationRoute()
            mapRef.clearNavigationPickMarkers()
            mapRef.clearSearchResult()
        }
    }

    function buildMidString() {
        var parts = []
        for (var i = 0; i < waypointModel.count; i++) {
            var o = waypointModel.get(i)
            parts.push(o.longitude + "," + o.latitude)
        }
        return parts.join(";")
    }

    /// 天地图路线点之后，按添加顺序追加自定义点
    function appendCustomPointsToPath(pathPoints) {
        var result = []
        if (pathPoints) {
            for (var i = 0; i < pathPoints.length; i++)
                result.push(pathPoints[i])
        }
        for (var j = 0; j < customPointModel.count; j++) {
            var o = customPointModel.get(j)
            result.push({
                            "latitude": o.latitude,
                            "longitude": o.longitude
                        })
        }
        return result
    }

    function refreshEndpointMarkers() {
        if (!mapRef)
            return
        mapRef.clearNavigationEndpointMarkers()
        // 有自定义点：特殊航线模式，不画起终点钉
        if (hasCustomPoints())
            return
        if (originSet)
            mapRef.setNavigationStartMarker(originLat, originLon, originName, navPlateField.text.trim())
        if (destSet)
            mapRef.setNavigationEndMarker(destLat, destLon, destName)
    }

    function applyPickFromPopup(lat, lon, name, role) {
        navErrorStrip.text = ""
        if (role === 0) {
            navigationPanel.originSet = true
            navigationPanel.originLon = lon
            navigationPanel.originLat = lat
            navigationPanel.originName = name
        } else if (role === 1) {
            navigationPanel.destSet = true
            navigationPanel.destLon = lon
            navigationPanel.destLat = lat
            navigationPanel.destName = name
        } else if (role === 2) {
            waypointModel.append({
                                     "name": name,
                                     "latitude": lat,
                                     "longitude": lon
                                 })
        } else if (role === 3) {
            var wasSpecial = hasCustomPoints()
            customPointModel.append({
                                        "name": name,
                                        "latitude": lat,
                                        "longitude": lon
                                    })
            refreshEndpointMarkers()
            // 从无自定义点变为有：若已显示普通路线，清掉以便下次按特殊样式重规划
            if (!wasSpecial && mapRef)
                mapRef.clearNavigationRoute()
            return
        }
        refreshEndpointMarkers()
    }

    function readTargetAreaPoint() {
        if (!controller) {
            navErrorStrip.text = "控制器未就绪"
            return null
        }
        var lat = controller.targetAreaLatitude
        var lon = controller.targetAreaLongitude
        if (lat === undefined || lon === undefined
                || (Math.abs(lat) < 1e-9 && Math.abs(lon) < 1e-9)) {
            navErrorStrip.text = "请先在搜索页设置目标区域"
            return null
        }
        var name = (controller.targetAreaName && controller.targetAreaName.length > 0)
                   ? controller.targetAreaName : "目标区域"
        return { "lat": lat, "lon": lon, "name": name }
    }

    function useTargetAreaAsPoint(role) {
        navErrorStrip.text = ""
        var p = readTargetAreaPoint()
        if (!p)
            return
        cancelMapPick()
        applyPickFromPopup(p.lat, p.lon, p.name, role)
    }

    function beginMapPick(role) {
        navErrorStrip.text = ""
        if (!mapRef) {
            navErrorStrip.text = "地图未就绪"
            return
        }
        if (navGeoPickPopup.visible)
            navGeoPickPopup.close()
        mapPickRole = role
        mapRef.beginNavigationMapPick(role)
    }

    function cancelMapPick() {
        if (mapPickRole < 0 && !(mapRef && mapRef.navigationMapPickActive))
            return
        mapPickRole = -1
        if (mapRef)
            mapRef.cancelNavigationMapPick()
    }

    function onRouteReady(pathPoints) {
        navErrorStrip.text = ""
        if (!mapRef || !pathPoints || pathPoints.length < 2)
            return
        // 天地图路线末点 = 导航选择的终点（定位车辆停靠处，不含自定义点）
        var navDestIndex = pathPoints.length - 1
        var path = appendCustomPointsToPath(pathPoints)
        if (path.length < 2)
            return
        if (hasCustomPoints()) {
            mapRef.showCustomPointNavigationRoute(path,
                                                 navPlateField.text.trim(),
                                                 startTimeField.dateTimeText,
                                                 endTimeField.dateTimeText,
                                                 navDestIndex)
        } else {
            mapRef.showNavigationRoute(path)
        }
    }

    function removeCustomPointAt(index) {
        if (index < 0 || index >= customPointModel.count)
            return
        var wasSpecial = hasCustomPoints()
        customPointModel.remove(index)
        refreshEndpointMarkers()
        if (wasSpecial && !hasCustomPoints() && mapRef)
            mapRef.clearNavigationRoute()
    }

    onVisibleChanged: {
        if (!visible)
            cancelMapPick()
    }

    ScrollView {
        id: navScroll
        anchors.fill: parent
        anchors.margins: 10
        clip: true
        contentWidth: availableWidth
        focus: true

        ColumnLayout {
            width: navScroll.availableWidth
            spacing: 8

            PanelHeader {
                title: "驾车导航"
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: "车牌号"
                    font.pixelSize: 12
                    color: "#2c3e50"
                }
                TextField {
                    id: navPlateField
                    Layout.fillWidth: true
                    placeholderText: "例如 冀A·12345"
                    font.pixelSize: 12
                    maximumLength: 16
                    onTextChanged: {
                        if (!navigationPanel.hasCustomPoints()
                                && navigationPanel.originSet && mapRef)
                            mapRef.updateNavigationStartPlate(text.trim())
                    }
                }
            }

            GroupBox {
                title: "时间"
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
                    }

                    DateTimePickerField {
                        id: endTimeField
                        label: "结束"
                        defaultHour: 23
                        defaultMinute: 59
                        defaultSecond: 59
                        onDateTimeChanged: {
                            if (mapRef && navigationPanel.hasCustomPoints())
                                mapRef.updateComplementArrivalTime(endTimeField.dateTimeText, false)
                        }
                    }
                }
            }

            GroupBox {
                title: "已选路线点"
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    // —— 起点 ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                text: "起点"
                                font.pixelSize: 12
                                color: "#2c3e50"
                            }
                            Text {
                                Layout.fillWidth: true
                                text: navigationPanel.originSet ? navigationPanel.originName : ""
                                font.pixelSize: 12
                                color: "#34495e"
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 4
                            columnSpacing: 6
                            Button {
                                text: "目标区域"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: navigationPanel.useTargetAreaAsPoint(0)
                            }
                            Button {
                                text: navigationPanel.mapPickRole === 0 ? "选点中…" : "地图选点"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                highlighted: navigationPanel.mapPickRole === 0
                                onClicked: {
                                    if (navigationPanel.mapPickRole === 0)
                                        navigationPanel.cancelMapPick()
                                    else
                                        navigationPanel.beginMapPick(0)
                                }
                            }
                            Button {
                                text: "搜索"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: {
                                    navigationPanel.cancelMapPick()
                                    navGeoPickPopup.pickTargetRole = 0
                                    navGeoPickPopup.open()
                                }
                            }
                            Button {
                                text: "清除"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: {
                                    navigationPanel.originSet = false
                                    navigationPanel.originName = ""
                                    navigationPanel.refreshEndpointMarkers()
                                }
                            }
                        }
                    }

                    // —— 终点 ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Label {
                                text: "终点"
                                font.pixelSize: 12
                                color: "#2c3e50"
                            }
                            Text {
                                Layout.fillWidth: true
                                text: navigationPanel.destSet ? navigationPanel.destName : ""
                                font.pixelSize: 12
                                color: "#34495e"
                                elide: Text.ElideRight
                                wrapMode: Text.NoWrap
                            }
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 4
                            columnSpacing: 6
                            Button {
                                text: "目标区域"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: navigationPanel.useTargetAreaAsPoint(1)
                            }
                            Button {
                                text: navigationPanel.mapPickRole === 1 ? "选点中…" : "地图选点"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                highlighted: navigationPanel.mapPickRole === 1
                                onClicked: {
                                    if (navigationPanel.mapPickRole === 1)
                                        navigationPanel.cancelMapPick()
                                    else
                                        navigationPanel.beginMapPick(1)
                                }
                            }
                            Button {
                                text: "搜索"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: {
                                    navigationPanel.cancelMapPick()
                                    navGeoPickPopup.pickTargetRole = 1
                                    navGeoPickPopup.open()
                                }
                            }
                            Button {
                                text: "清除"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: {
                                    navigationPanel.destSet = false
                                    navigationPanel.destName = ""
                                    navigationPanel.refreshEndpointMarkers()
                                }
                            }
                        }
                    }

                    // —— 途经点 ——
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        Label {
                            text: "途经点（参与天地图规划）"
                            font.pixelSize: 12
                            color: "#2c3e50"
                            Layout.fillWidth: true
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            rowSpacing: 4
                            columnSpacing: 6
                            Button {
                                text: "目标区域"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                onClicked: navigationPanel.useTargetAreaAsPoint(2)
                            }
                            Button {
                                text: navigationPanel.mapPickRole === 2 ? "选点中…" : "地图选点"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                highlighted: navigationPanel.mapPickRole === 2
                                onClicked: {
                                    if (navigationPanel.mapPickRole === 2)
                                        navigationPanel.cancelMapPick()
                                    else
                                        navigationPanel.beginMapPick(2)
                                }
                            }
                            Button {
                                text: "搜索添加"
                                font.pixelSize: 11
                                Layout.fillWidth: true
                                Layout.columnSpan: 2
                                onClicked: {
                                    navigationPanel.cancelMapPick()
                                    navGeoPickPopup.pickTargetRole = 2
                                    navGeoPickPopup.open()
                                }
                            }
                        }
                    }

                    ListView {
                        id: wpList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(160, Math.max(44, waypointModel.count * 52 + 8))
                        model: waypointModel
                        clip: true
                        spacing: 4
                        visible: waypointModel.count > 0

                        delegate: Rectangle {
                            width: wpList.width
                            height: 48
                            color: "white"
                            border.color: "#dce0e8"
                            border.width: 1
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6
                                Label {
                                    text: (index + 1) + ". " + model.name
                                    font.pixelSize: 11
                                    color: "#2c3e50"
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: "移除"
                                    font.pixelSize: 11
                                    onClicked: waypointModel.remove(index)
                                }
                            }
                        }
                    }

                    Text {
                        text: waypointModel.count === 0 ? "（无途经点）" : ""
                        font.pixelSize: 11
                        color: "#95a5a6"
                        visible: waypointModel.count === 0
                    }

                    Button {
                        text: "清空全部途经点"
                        font.pixelSize: 11
                        enabled: waypointModel.count > 0
                        onClicked: navigationPanel.clearWaypointModel()
                    }
                }
            }

            // —— 自定义点（追加在规划路线之后，触发特殊航线样式）——
            GroupBox {
                title: "自定义点（接在规划路线后）"
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    Text {
                        Layout.fillWidth: true
                        text: "按添加顺序接到天地图路线末尾；有自定义点时使用特殊航线样式"
                        font.pixelSize: 11
                        color: "#7f8c8d"
                        wrapMode: Text.WordWrap
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 4
                        columnSpacing: 6
                        Button {
                            text: "目标区域"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            onClicked: navigationPanel.useTargetAreaAsPoint(3)
                        }
                        Button {
                            text: navigationPanel.mapPickRole === 3 ? "选点中…" : "地图选点"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            highlighted: navigationPanel.mapPickRole === 3
                            onClicked: {
                                if (navigationPanel.mapPickRole === 3)
                                    navigationPanel.cancelMapPick()
                                else
                                    navigationPanel.beginMapPick(3)
                            }
                        }
                        Button {
                            text: "搜索添加"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            Layout.columnSpan: 2
                            onClicked: {
                                navigationPanel.cancelMapPick()
                                navGeoPickPopup.pickTargetRole = 3
                                navGeoPickPopup.open()
                            }
                        }
                    }

                    ListView {
                        id: customList
                        Layout.fillWidth: true
                        Layout.preferredHeight: Math.min(160, Math.max(44, customPointModel.count * 52 + 8))
                        model: customPointModel
                        clip: true
                        spacing: 4
                        visible: customPointModel.count > 0

                        delegate: Rectangle {
                            width: customList.width
                            height: 48
                            color: "white"
                            border.color: "#3498db"
                            border.width: 1
                            radius: 4

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 6
                                Label {
                                    text: (index + 1) + ". " + model.name
                                    font.pixelSize: 11
                                    color: "#2c3e50"
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: "移除"
                                    font.pixelSize: 11
                                    onClicked: navigationPanel.removeCustomPointAt(index)
                                }
                            }
                        }
                    }

                    Text {
                        text: customPointModel.count === 0 ? "（无自定义点 → 普通导航）" : ""
                        font.pixelSize: 11
                        color: "#95a5a6"
                        visible: customPointModel.count === 0
                    }

                    Button {
                        text: "清空全部自定义点"
                        font.pixelSize: 11
                        enabled: customPointModel.count > 0
                        onClicked: {
                            var wasSpecial = navigationPanel.hasCustomPoints()
                            navigationPanel.clearCustomPointModel()
                            navigationPanel.refreshEndpointMarkers()
                            if (wasSpecial && mapRef)
                                mapRef.clearNavigationRoute()
                        }
                    }
                }
            }

            GroupBox {
                title: "策略"
                Layout.fillWidth: true

                ColumnLayout {
                    anchors.fill: parent
                    ComboBox {
                        id: styleCombo
                        Layout.fillWidth: true
                        model: ["最快 (0)", "最短 (1)", "避开高速 (2)", "步行 (3)"]
                        font.pixelSize: 12
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    text: (typeof routePlanner !== "undefined" && routePlanner && routePlanner.busy)
                          ? "规划中…" : "规划路线"
                    Layout.fillWidth: true
                    enabled: typeof routePlanner !== "undefined" && routePlanner && !routePlanner.busy
                    onClicked: {
                        navErrorStrip.text = ""
                        navigationPanel.cancelMapPick()
                        if (mapRef) {
                            mapRef.clearNavigationPickMarkers()
                            mapRef.clearSearchResult()
                        }
                        if (!navigationPanel.originSet || !navigationPanel.destSet) {
                            navErrorStrip.text = "请先设置起点与终点"
                            return
                        }
                        var mid = navigationPanel.buildMidString()
                        routePlanner.requestRoute(navigationPanel.originLon, navigationPanel.originLat,
                                                    navigationPanel.destLon, navigationPanel.destLat,
                                                    styleCombo.currentIndex, mid)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Button {
                        text: "清除路线"
                        Layout.fillWidth: true
                        onClicked: {
                            navErrorStrip.text = ""
                            if (mapRef)
                                mapRef.clearNavigationRoute()
                        }
                    }
                    Button {
                        text: "重置点"
                        Layout.fillWidth: true
                        onClicked: {
                            navErrorStrip.text = ""
                            navigationPanel.clearAllNavPoints()
                        }
                    }
                }
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: typeof routePlanner !== "undefined" && routePlanner && routePlanner.busy
                running: visible
                Layout.preferredHeight: 28
            }

            FormErrorStrip {
                id: navErrorStrip
            }
        }
    }

    Popup {
        id: navGeoPickPopup
        modal: true
        dim: true
        focus: true
        padding: 10
        closePolicy: Popup.CloseOnEscape

        property int pickTargetRole: 0

        readonly property string pickBanner: {
            if (pickTargetRole === 0)
                return "搜索起点"
            if (pickTargetRole === 1)
                return "搜索终点"
            if (pickTargetRole === 2)
                return "搜索途经点"
            return "搜索自定义点"
        }

        parent: navigationPanel.Window ? navigationPanel.Window.contentItem : navigationPanel
        x: 72
        y: parent ? Math.max(24, (parent.height - height) / 2) : 24
        width: Math.min(360, Math.max(280, parent.width * 0.32))
        height: navGeoPickContent.hasResults
                ? Math.min(520, parent.height - 48)
                : Math.min(260, parent.height - 48)
        onOpened: {
            if (parent)
                y = Math.max(24, (parent.height - height) / 2)
        }
        onHeightChanged: {
            if (visible && parent)
                y = Math.max(24, (parent.height - height) / 2)
        }
        onClosed: {
            if (mapRef)
                mapRef.clearSearchResult()
        }

        background: Rectangle {
            color: "#f0f0f0"
            border.color: "#bdc3c7"
            border.width: 1
            radius: 6
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: navGeoPickPopup.pickBanner
                    font.pixelSize: 14
                    font.bold: true
                    color: "#2c3e50"
                    Layout.fillWidth: true
                }
                Button {
                    text: "关闭"
                    font.pixelSize: 11
                    onClicked: navGeoPickPopup.close()
                }
            }

            GeoPickSearch {
                id: navGeoPickContent
                Layout.fillWidth: true
                Layout.fillHeight: true
                autoLocateFirstResult: true
                acceptGeocoderResults: navGeoPickPopup.visible
                assignRole: {
                    if (navGeoPickPopup.pickTargetRole === 0)
                        return "origin"
                    if (navGeoPickPopup.pickTargetRole === 1)
                        return "dest"
                    if (navGeoPickPopup.pickTargetRole === 2)
                        return "waypoint"
                    return "custom"
                }

                onLocateRequested: function (lat, lon, name) {
                    if (mapRef)
                        mapRef.locateToPlace(lat, lon)
                }
                onPlacePicked: function (lat, lon, name) {
                    navigationPanel.applyPickFromPopup(lat, lon, name, navGeoPickPopup.pickTargetRole)
                    navGeoPickPopup.close()
                }
            }
        }
    }

    Connections {
        target: (typeof routePlanner !== "undefined") ? routePlanner : null
        enabled: navigationPanel.visible

        function onRouteReady(pathPoints) {
            navigationPanel.onRouteReady(pathPoints)
        }

        function onRouteFailed(msg) {
            navErrorStrip.text = msg
        }
    }

    Connections {
        target: navigationPanel.mapRef
        enabled: navigationPanel.mapRef !== null

        function onNavigationPointPicked(lat, lon, name) {
            if (navigationPanel.mapPickRole < 0)
                return
            var role = navigationPanel.mapPickRole
            navigationPanel.mapPickRole = -1
            navigationPanel.applyPickFromPopup(lat, lon, name, role)
            // 保留地图选点 📍，规划时再清
        }

        function onNavigationMapPickCancelled() {
            navigationPanel.mapPickRole = -1
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: navigationPanel.visible && navigationPanel.mapPickRole >= 0
        onActivated: navigationPanel.cancelMapPick()
    }
}
