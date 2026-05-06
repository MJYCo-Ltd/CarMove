import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 与「地点搜索」相同的地名检索，但不自动定位；由用户点击「选用」确认
Item {
    id: root

    /// 为 false 时忽略 geocoder 回调（避免与其他面板共用 geocoder 时串台）
    property bool acceptGeocoderResults: true
    /// 选用按钮旁提示，如「起点」；mapLocateMode 下用于副标题（可选）
    property string pickHintText: ""

    /// true：结果按钮为「在地图上定位」并发出 locateRequested；false：导航「选用」并发出 placePicked
    property bool mapLocateMode: false

    /// 地图搜索页：得到结果后自动定位第一条（与旧 GeoSearchPanel 行为一致）
    property bool autoLocateFirstResult: false

    signal placePicked(double lat, double lon, string name)
    signal locateRequested(double lat, double lon, string name)

    onAcceptGeocoderResultsChanged: {
        if (!acceptGeocoderResults) {
            pickResultModel.clear()
            geoErrorStrip.text = ""
        }
    }

    ListModel { id: pickResultModel }

    function doPickSearch() {
        var kw = pickKw.text.trim()
        var region = pickAdmin.text.trim()
        if (kw.length === 0 || region.length === 0)
            return
        pickResultModel.clear()
        geoErrorStrip.text = ""
        geocoder.searchInAdminRegion(kw, region)
    }

    Connections {
        target: (typeof geocoder !== "undefined") ? geocoder : null

        function onGeocodeResultsReady(results) {
            if (!root.acceptGeocoderResults)
                return
            pickResultModel.clear()
            geoErrorStrip.text = ""
            for (var i = 0; i < results.length; i++)
                pickResultModel.append({
                                           "name": results[i].name,
                                           "address": results[i].address,
                                           "latitude": results[i].latitude,
                                           "longitude": results[i].longitude
                                       })
            if (root.autoLocateFirstResult && results.length > 0) {
                var r0 = results[0]
                root.locateRequested(r0.latitude, r0.longitude, r0.name)
            }
        }

        function onGeocodeFailed(errorMessage) {
            if (!root.acceptGeocoderResults)
                return
            geoErrorStrip.text = errorMessage
            pickResultModel.clear()
        }
    }

    ColumnLayout {
        id: col
        anchors.fill: parent
        spacing: 8

        GroupBox {
            title: "地名与行政区"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                TextField {
                    id: pickKw
                    Layout.fillWidth: true
                    placeholderText: "输入地点名称，如：火车站"
                    font.pixelSize: 12
                    Keys.onReturnPressed: root.doPickSearch()
                    Keys.onEnterPressed: root.doPickSearch()
                }

                Text {
                    text: "行政区范围"
                    font.pixelSize: 12
                    color: "#2c3e50"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Item {
                        Layout.fillWidth: true
                        Layout.minimumHeight: pickAdmin.height
                        implicitHeight: pickAdmin.height

                        TextField {
                            id: pickAdmin
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            placeholderText: "输入省/市名称，如：天津"
                            font.pixelSize: 12
                            Keys.onReturnPressed: root.doPickSearch()
                            Keys.onEnterPressed: root.doPickSearch()
                            Keys.onDownPressed: {
                                if (pickSuggestList.count > 0) {
                                    pickSuggestList.currentIndex = 0
                                    pickSuggestList.forceActiveFocus()
                                }
                            }
                            onTextChanged: pickSuggestPopup.updateSuggestions(text)
                        }

                        Rectangle {
                            id: pickSuggestPopup
                            visible: (pickSuggestList.count > 0 && pickAdmin.activeFocus) || pickSuggestList.activeFocus
                            width: pickAdmin.width
                            height: Math.min(pickSuggestList.count, 6) * 32
                            anchors.top: pickAdmin.bottom
                            anchors.left: pickAdmin.left
                            z: 999
                            color: "white"
                            border.color: "#bdc3c7"
                            border.width: 1
                            radius: 3
                            clip: true

                            property var allNames: []

                            Component.onCompleted: {
                                allNames = (typeof geocoder !== "undefined" && geocoder) ? geocoder.adminRegionNames() : []
                            }

                            function updateSuggestions(input) {
                                pickSuggestModel.clear()
                                var kw = input.trim()
                                if (kw.length === 0)
                                    return
                                var cnt = 0
                                for (var i = 0; i < allNames.length && cnt < 50; i++) {
                                    if (allNames[i].indexOf(kw) >= 0) {
                                        pickSuggestModel.append({
                                                                    "name": allNames[i]
                                                                })
                                        cnt++
                                    }
                                }
                            }

                            ListModel {
                                id: pickSuggestModel
                            }

                            ListView {
                                id: pickSuggestList
                                anchors.fill: parent
                                model: pickSuggestModel
                                clip: true
                                keyNavigationEnabled: true
                                Keys.onReturnPressed: selectCurrent()
                                Keys.onEnterPressed: selectCurrent()
                                Keys.onEscapePressed: {
                                    pickSuggestModel.clear()
                                    pickAdmin.forceActiveFocus()
                                }

                                function selectCurrent() {
                                    if (currentIndex >= 0 && currentIndex < count) {
                                        pickAdmin.text = pickSuggestModel.get(currentIndex).name
                                        pickSuggestModel.clear()
                                        pickAdmin.forceActiveFocus()
                                    }
                                }

                                delegate: Rectangle {
                                    width: pickSuggestList.width
                                    height: 32
                                    color: pickSuggestList.currentIndex === index ? "#d6eaf8" : (pma.containsMouse ? "#eaf4fc" : "white")
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
                                        id: pma
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: {
                                            pickAdmin.text = model.name
                                            pickSuggestModel.clear()
                                            pickAdmin.forceActiveFocus()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    Button {
                        text: (typeof geocoder !== "undefined" && geocoder && geocoder.busy) ? "搜索中" : "搜索"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                        Layout.preferredWidth: implicitWidth
                        Layout.minimumWidth: 52
                        Layout.maximumWidth: 84
                        topPadding: 4
                        bottomPadding: 4
                        leftPadding: 10
                        rightPadding: 10
                        enabled: pickKw.text.trim().length > 0 && pickAdmin.text.trim().length > 0
                                 && !(typeof geocoder !== "undefined" && geocoder && geocoder.busy)
                        onClicked: root.doPickSearch()
                    }
                }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: typeof geocoder !== "undefined" && geocoder && geocoder.busy && root.acceptGeocoderResults
            running: visible
            Layout.preferredHeight: 28
        }

        GroupBox {
            title: pickResultModel.count > 0 ? ("搜索结果（" + pickResultModel.count + " 条）") : "搜索结果"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: pickResultModel.count > 0 ? 200 : 0
            visible: pickResultModel.count > 0

            // GroupBox 内用 ColumnLayout + ListView.fillHeight，避免 anchors.fill 在 contentItem 上高度为 0 导致无法点击
            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                ListView {
                    id: pickResultList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: pickResultModel
                    clip: true
                    spacing: 6
                    boundsBehavior: Flickable.StopAtBounds
                    interactive: true

                    delegate: GeocodeResultCard {
                        width: pickResultList.width
                        height: implicitHeight
                        resultName: model.name
                        resultAddress: model.address
                        resultLatitude: model.latitude
                        resultLongitude: model.longitude
                        actionButtonText: root.mapLocateMode ? "在地图上定位" : (root.pickHintText.length > 0 ? ("选用 — " + root.pickHintText) : "选用此地点")
                        onActionTriggered: {
                            if (root.mapLocateMode)
                                root.locateRequested(model.latitude, model.longitude, model.name)
                            else
                                root.placePicked(model.latitude, model.longitude, model.name)
                        }
                    }
                }
            }
        }

        FormErrorStrip {
            id: geoErrorStrip
        }
    }
}
