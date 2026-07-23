import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/// 行政区输入框：输入时弹出建议列表，支持鼠标点击与键盘选择
Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: field.implicitHeight
    height: field.implicitHeight

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias font: field.font
    property alias fieldEnabled: field.enabled
    property int maxSuggestions: 50

    signal accepted()
    signal textEdited()

    function forceActiveFocus() {
        field.forceActiveFocus()
    }

    function clear() {
        field.text = ""
        suggestModel.clear()
        suggestPopup.close()
    }

    TextField {
        id: field
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        font.pixelSize: 12

        Keys.onReturnPressed: root.accepted()
        Keys.onEnterPressed: root.accepted()
        Keys.onDownPressed: {
            if (suggestModel.count > 0) {
                if (!suggestPopup.opened)
                    suggestPopup.open()
                suggestList.currentIndex = 0
                suggestList.forceActiveFocus()
            }
        }
        Keys.onEscapePressed: {
            suggestModel.clear()
            suggestPopup.close()
        }

        onTextChanged: {
            root.refreshSuggestions()
            root.textEdited()
        }
        onActiveFocusChanged: {
            if (activeFocus)
                root.refreshSuggestions()
            else if (!suggestPopup.opened)
                suggestPopup.close()
        }
    }

    ListModel {
        id: suggestModel
    }

    property var allNames: []

    Component.onCompleted: {
        allNames = (typeof geocoder !== "undefined" && geocoder)
                   ? geocoder.adminRegionNames() : []
    }

    function refreshSuggestions() {
        suggestModel.clear()
        var kw = field.text.trim()
        if (kw.length === 0) {
            suggestPopup.close()
            return
        }
        var cnt = 0
        for (var i = 0; i < allNames.length && cnt < root.maxSuggestions; i++) {
            if (allNames[i].indexOf(kw) >= 0) {
                suggestModel.append({ "name": allNames[i] })
                cnt++
            }
        }
        // 已精确选中完整行政区名时不再弹出
        if (suggestModel.count === 1 && suggestModel.get(0).name === kw) {
            suggestPopup.close()
            return
        }
        if (suggestModel.count > 0 && field.activeFocus)
            suggestPopup.open()
        else
            suggestPopup.close()
    }

    function applySuggestion(name) {
        suggestModel.clear()
        suggestPopup.close()
        field.text = name
        suggestModel.clear()
        suggestPopup.close()
        field.forceActiveFocus()
    }

    Popup {
        id: suggestPopup
        x: 0
        y: field.height + 2
        width: field.width
        padding: 0
        modal: false
        focus: false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        // 用 Overlay 保证盖在其它控件之上，鼠标可点选
        parent: Overlay.overlay
        property var _anchor: field
        function reposition() {
            if (!_anchor || !Overlay.overlay)
                return
            var p = _anchor.mapToItem(Overlay.overlay, 0, _anchor.height + 2)
            x = p.x
            y = p.y
            width = _anchor.width
        }
        onAboutToShow: reposition()
        onOpened: reposition()

        background: Rectangle {
            color: "white"
            border.color: "#bdc3c7"
            border.width: 1
            radius: 3
        }

        contentItem: ListView {
            id: suggestList
            clip: true
            implicitHeight: Math.min(count, 6) * 32
            model: suggestModel
            keyNavigationEnabled: true
            boundsBehavior: Flickable.StopAtBounds

            Keys.onReturnPressed: selectCurrent()
            Keys.onEnterPressed: selectCurrent()
            Keys.onEscapePressed: {
                suggestModel.clear()
                suggestPopup.close()
                field.forceActiveFocus()
            }

            function selectCurrent() {
                if (currentIndex >= 0 && currentIndex < count)
                    root.applySuggestion(suggestModel.get(currentIndex).name)
            }

            delegate: Item {
                width: suggestList.width
                height: 32

                Rectangle {
                    anchors.fill: parent
                    color: suggestList.currentIndex === index
                           ? "#d6eaf8"
                           : (rowHover.hovered ? "#eaf4fc" : "white")
                }

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

                HoverHandler {
                    id: rowHover
                }

                TapHandler {
                    onTapped: root.applySuggestion(model.name)
                }
            }
        }
    }

    Connections {
        target: field
        function onWidthChanged() {
            if (suggestPopup.opened)
                suggestPopup.reposition()
        }
    }
}
