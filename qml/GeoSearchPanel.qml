import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 地点搜索面板：关键词 + 行政区模糊匹配 + 结果列表
Rectangle {
    id: geoSearchPanel
    color: "#f0f0f0"
    border.color: "#ccc"

    signal locateRequested(double lat, double lon, string name)

    ListModel { id: searchResultModel }

    function doSearch() {
        var kw = keywordField.text.trim()
        var region = adminField.text.trim()
        if (kw.length === 0 || region.length === 0) return
        searchResultModel.clear()
        errorText.text = ""
        geocoder.searchInAdminRegion(kw, region)
    }

    Connections {
        target: (typeof geocoder !== 'undefined') ? geocoder : null

        function onGeocodeResultsReady(results) {
            searchResultModel.clear()
            errorText.text = ""
            for (var i = 0; i < results.length; i++)
                searchResultModel.append({ name: results[i].name, address: results[i].address,
                                           latitude: results[i].latitude, longitude: results[i].longitude })
            if (results.length > 0)
                geoSearchPanel.locateRequested(results[0].latitude, results[0].longitude, results[0].name)
        }

        function onGeocodeFailed(errorMessage) {
            errorText.text = errorMessage
            searchResultModel.clear()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Text { text: "地点搜索"; font.pixelSize: 14; font.bold: true; color: "#2c3e50"; Layout.fillWidth: true }

        GroupBox {
            title: "搜索条件"
            Layout.fillWidth: true

            ColumnLayout {
                anchors.fill: parent
                spacing: 6

                TextField {
                    id: keywordField
                    Layout.fillWidth: true
                    placeholderText: "输入地点名称，如：火车站"
                    Keys.onReturnPressed: doSearch()
                    Keys.onEnterPressed:  doSearch()
                }

                Text { text: "行政区范围"; font.pixelSize: 12; color: "#2c3e50" }

                // 行政区模糊匹配输入框 + 候选下拉
                Item {
                    Layout.fillWidth: true
                    height: adminField.height

                    TextField {
                        id: adminField
                        width: parent.width
                        placeholderText: "输入省/市名称，如：天津"
                        Keys.onReturnPressed: doSearch()
                        Keys.onEnterPressed:  doSearch()
                        Keys.onDownPressed: {
                            if (suggestList.count > 0) { suggestList.currentIndex = 0; suggestList.forceActiveFocus() }
                        }
                        onTextChanged: suggestPopup.updateSuggestions(text)
                    }

                    Rectangle {
                        id: suggestPopup
                        visible: (suggestList.count > 0 && adminField.activeFocus) || suggestList.activeFocus
                        width: adminField.width
                        height: Math.min(suggestList.count, 6) * 32
                        anchors.top: adminField.bottom
                        anchors.left: adminField.left
                        z: 999; color: "white"
                        border.color: "#bdc3c7"; border.width: 1; radius: 3; clip: true

                        property var allNames: []
                        Component.onCompleted: {
                            allNames = (typeof geocoder !== 'undefined' && geocoder) ? geocoder.adminRegionNames() : []
                        }

                        function updateSuggestions(input) {
                            suggestModel.clear()
                            var kw = input.trim()
                            if (kw.length === 0) return
                            var cnt = 0
                            for (var i = 0; i < allNames.length && cnt < 50; i++) {
                                if (allNames[i].indexOf(kw) >= 0) { suggestModel.append({ name: allNames[i] }); cnt++ }
                            }
                        }

                        ListModel { id: suggestModel }

                        ListView {
                            id: suggestList
                            anchors.fill: parent; model: suggestModel; clip: true; keyNavigationEnabled: true
                            Keys.onReturnPressed: selectCurrent()
                            Keys.onEnterPressed:  selectCurrent()
                            Keys.onEscapePressed: { suggestModel.clear(); adminField.forceActiveFocus() }

                            function selectCurrent() {
                                if (currentIndex >= 0 && currentIndex < count) {
                                    adminField.text = suggestModel.get(currentIndex).name
                                    suggestModel.clear(); adminField.forceActiveFocus()
                                }
                            }

                            delegate: Rectangle {
                                width: suggestList.width; height: 32
                                color: suggestList.currentIndex === index ? "#d6eaf8" : (ma.containsMouse ? "#eaf4fc" : "white")
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left; anchors.leftMargin: 8
                                    anchors.right: parent.right; anchors.rightMargin: 8
                                    text: model.name; font.pixelSize: 12; color: "#2c3e50"; elide: Text.ElideRight
                                }
                                MouseArea {
                                    id: ma; anchors.fill: parent; hoverEnabled: true
                                    onClicked: { adminField.text = model.name; suggestModel.clear(); adminField.forceActiveFocus() }
                                }
                            }
                        }
                    }
                }

                Button {
                    text: (typeof geocoder !== 'undefined' && geocoder && geocoder.busy) ? "搜索中..." : "搜索"
                    Layout.fillWidth: true
                    enabled: keywordField.text.trim().length > 0 && adminField.text.trim().length > 0 &&
                             !(typeof geocoder !== 'undefined' && geocoder && geocoder.busy)
                    onClicked: doSearch()
                }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: typeof geocoder !== 'undefined' && geocoder && geocoder.busy
            running: visible; Layout.preferredHeight: 32
        }

        GroupBox {
            title: searchResultModel.count > 0 ? ("搜索结果（" + searchResultModel.count + " 条）") : "搜索结果"
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: searchResultModel.count > 0

            ListView {
                anchors.fill: parent; model: searchResultModel; clip: true; spacing: 6

                delegate: Rectangle {
                    width: parent.width; height: col.implicitHeight + 20
                    color: "white"; border.color: "#dce0e8"; border.width: 1; radius: 4

                    Column {
                        id: col
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 8 }
                        spacing: 3
                        Text { width: parent.width; text: model.name; font.bold: true; font.pixelSize: 12; color: "#2c3e50"; wrapMode: Text.WordWrap }
                        Text { width: parent.width; text: model.address; font.pixelSize: 11; color: "#7f8c8d"; wrapMode: Text.WordWrap; visible: model.address.length > 0 }
                        Text { width: parent.width; text: model.latitude.toFixed(6) + ", " + model.longitude.toFixed(6); font.pixelSize: 10; color: "#95a5a6" }
                        Button {
                            text: "在地图上定位"; height: 26; font.pixelSize: 11
                            onClicked: geoSearchPanel.locateRequested(model.latitude, model.longitude, model.name)
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; height: errorText.implicitHeight + 16
            color: "#fdecea"; border.color: "#e74c3c"; border.width: 1; radius: 4
            visible: errorText.text.length > 0
            Text { id: errorText; anchors.fill: parent; anchors.margins: 8; text: ""; font.pixelSize: 11; color: "#c0392b"; wrapMode: Text.WordWrap }
        }

        Item { Layout.fillHeight: true }
    }
}
