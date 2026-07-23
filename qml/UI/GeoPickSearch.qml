import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 地名检索：弹出搜索；结果含「在地图上定位」与「选为起点/终点/途经点/自定义点/目标」
Item {
    id: root

    /// 为 false 时忽略 geocoder 回调（避免与其他面板共用 geocoder 时串台）
    property bool acceptGeocoderResults: true

    /// 得到结果后自动在地图上定位第一条（仅预览，不回填）
    property bool autoLocateFirstResult: true

    /// "" | "origin" | "dest" | "waypoint" | "custom" | "targetArea"
    property string assignRole: ""

    readonly property string assignButtonText: {
        if (assignRole === "origin")
            return "选为起点"
        if (assignRole === "dest")
            return "选为终点"
        if (assignRole === "waypoint")
            return "选为途经点"
        if (assignRole === "custom")
            return "选为自定义点"
        if (assignRole === "targetArea")
            return "选为目标"
        return ""
    }

    readonly property bool hasResults: pickResultModel.count > 0

    signal placePicked(double lat, double lon, string name)
    signal locateRequested(double lat, double lon, string name)
    signal targetAreaRequested(double lat, double lon, string name)

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

                    AdminRegionField {
                        id: pickAdmin
                        Layout.fillWidth: true
                        placeholderText: "输入省/市名称，如：天津"
                        onAccepted: root.doPickSearch()
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
                        actionButtonText: "📍 在地图上定位"
                        secondaryButtonText: root.assignButtonText
                        onActionTriggered: {
                            root.locateRequested(model.latitude, model.longitude, model.name)
                        }
                        onSecondaryActionTriggered: {
                            if (root.assignRole === "targetArea")
                                root.targetAreaRequested(model.latitude, model.longitude, model.name)
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
