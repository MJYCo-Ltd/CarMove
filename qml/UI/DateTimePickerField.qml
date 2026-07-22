import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// 只读展示 + 弹出日历/时间控件，输出 yyyy-MM-dd HH:mm:ss
Item {
    id: root
    Layout.fillWidth: true
    implicitHeight: fieldRow.implicitHeight

    property string label: ""
    property int labelWidth: 32
    property bool hasValue: false
    property int defaultHour: 0
    property int defaultMinute: 0
    property int defaultSecond: 0
    property alias enabled: openButton.enabled

    property var selectedDate: null
    property int hour: defaultHour
    property int minute: defaultMinute
    property int second: defaultSecond

    readonly property string dateTimeText: hasValue ? formatDateTime(combinedDateTime()) : ""

    signal dateTimeChanged()

    property var draftDate: null

    property string pickerError: ""

    readonly property var hourModel: {
        var items = []
        for (var i = 0; i < 24; ++i)
            items.push(pad2(i))
        return items
    }
    readonly property var minuteSecondModel: {
        var items = []
        for (var i = 0; i < 60; ++i)
            items.push(pad2(i))
        return items
    }
    readonly property int yearFrom: 2010
    readonly property int yearTo: (new Date()).getFullYear()
    readonly property var yearModel: {
        var items = []
        for (var y = yearFrom; y <= yearTo; ++y)
            items.push(y + "年")
        return items
    }
    readonly property var monthModel: {
        var items = []
        var maxMonth = 11
        if (monthGrid && monthGrid.year === (new Date()).getFullYear())
            maxMonth = (new Date()).getMonth()
        for (var m = 0; m <= maxMonth; ++m)
            items.push((m + 1) + "月")
        return items
    }

    function clear(notify) {
        hasValue = false
        selectedDate = null
        hour = defaultHour
        minute = defaultMinute
        second = defaultSecond
        draftDate = null
        pickerError = ""
        if (notify)
            dateTimeChanged()
    }

    function setFromDateTime(dateObj, notify) {
        if (!dateObj) {
            clear(notify)
            return
        }

        var d = (dateObj instanceof Date) ? new Date(dateObj.getTime()) : new Date(dateObj)
        if (isNaN(d.getTime())) {
            clear(notify)
            return
        }

        var now = new Date()
        if (d.getTime() > now.getTime())
            d = now

        selectedDate = startOfDay(d)
        hour = d.getHours()
        minute = d.getMinutes()
        second = d.getSeconds()
        hasValue = true
        pickerError = ""
        if (notify)
            dateTimeChanged()
    }

    function combinedDateTime() {
        if (!selectedDate)
            return null
        var d = new Date(selectedDate.getTime())
        d.setHours(hour, minute, second, 0)
        return d
    }

    function formatDateTime(dateObj) {
        if (!dateObj)
            return ""
        return Qt.formatDateTime(dateObj, "yyyy-MM-dd HH:mm:ss")
    }

    function pad2(value) {
        return value < 10 ? ("0" + value) : ("" + value)
    }

    function startOfDay(dateObj) {
        return new Date(dateObj.getFullYear(), dateObj.getMonth(), dateObj.getDate())
    }

    function isFutureDay(dateObj) {
        if (!dateObj)
            return false
        return startOfDay(dateObj).getTime() > startOfDay(new Date()).getTime()
    }

    function draftDateTime() {
        if (!draftDate)
            return null
        var d = new Date(draftDate.getTime())
        d.setHours(hourCombo.currentIndex, minuteCombo.currentIndex, secondCombo.currentIndex, 0)
        return d
    }

    function clampDraftToNowIfNeeded() {
        var now = new Date()
        if (!draftDate)
            return
        if (isFutureDay(draftDate)) {
            draftDate = startOfDay(now)
            monthGrid.month = draftDate.getMonth()
            monthGrid.year = draftDate.getFullYear()
        }
        var candidate = draftDateTime()
        if (candidate && candidate.getTime() > now.getTime())
            syncTimeDraft(now.getHours(), now.getMinutes(), now.getSeconds())
        syncCalendarHeader()
    }

    function syncTimeDraft(h, m, s) {
        hourCombo.currentIndex = h
        minuteCombo.currentIndex = m
        secondCombo.currentIndex = s
    }

    function syncCalendarHeader() {
        var yearIndex = monthGrid.year - yearFrom
        if (yearIndex < 0)
            yearIndex = 0
        if (yearIndex >= yearModel.length)
            yearIndex = yearModel.length - 1
        yearCombo.currentIndex = yearIndex

        var monthIndex = monthGrid.month
        if (monthIndex >= monthCombo.count)
            monthIndex = Math.max(0, monthCombo.count - 1)
        monthCombo.currentIndex = monthIndex
        if (monthGrid.month !== monthIndex)
            monthGrid.month = monthIndex
    }

    function applyCalendarHeader() {
        var year = yearFrom + yearCombo.currentIndex
        var month = monthCombo.currentIndex
        if (year < yearFrom || year > yearTo)
            return
        if (!canNavigateTo(year, month)) {
            pickerError = "不能选择大于当前时间的月份"
            syncCalendarHeader()
            return
        }
        pickerError = ""
        monthGrid.year = year
        monthGrid.month = month
        // 若已选日期落在非法月份，保持日历显示即可；点选日期时再校验
        if (draftDate
                && (draftDate.getFullYear() !== year || draftDate.getMonth() !== month)) {
            // 保留同日号（若该月有），否则落到当月 1 日
            var day = draftDate.getDate()
            var maxDay = new Date(year, month + 1, 0).getDate()
            if (day > maxDay)
                day = maxDay
            var candidate = new Date(year, month, day)
            if (!isFutureDay(candidate))
                draftDate = candidate
        }
        clampDraftToNowIfNeeded()
    }

    function openPicker() {
        if (!enabled)
            return
        pickerError = ""
        if (hasValue && selectedDate) {
            draftDate = new Date(selectedDate.getTime())
            syncTimeDraft(hour, minute, second)
        } else {
            var now = new Date()
            draftDate = startOfDay(now)
            syncTimeDraft(defaultHour, defaultMinute, defaultSecond)
        }
        monthGrid.month = draftDate.getMonth()
        monthGrid.year = Math.max(yearFrom, Math.min(yearTo, draftDate.getFullYear()))
        clampDraftToNowIfNeeded()
        pickerPopup.open()
    }

    function acceptDraft() {
        if (!draftDate)
            return

        pickerError = ""
        if (isFutureDay(draftDate)) {
            pickerError = "不能选择大于当前日期的日期"
            return
        }

        var selected = draftDateTime()
        if (!selected) {
            pickerError = "请选择有效的日期时间"
            return
        }

        var now = new Date()
        if (selected.getTime() > now.getTime()) {
            pickerError = "不能选择大于当前时间的时刻"
            return
        }

        selectedDate = new Date(draftDate.getTime())
        hour = hourCombo.currentIndex
        minute = minuteCombo.currentIndex
        second = secondCombo.currentIndex
        hasValue = true
        pickerPopup.close()
        dateTimeChanged()
    }

    function isSameDay(a, b) {
        if (!a || !b)
            return false
        return a.getFullYear() === b.getFullYear()
                && a.getMonth() === b.getMonth()
                && a.getDate() === b.getDate()
    }

    function canNavigateTo(year, month) {
        var now = new Date()
        if (year > now.getFullYear())
            return false
        if (year === now.getFullYear() && month > now.getMonth())
            return false
        if (year < yearFrom)
            return false
        return true
    }

    RowLayout {
        id: fieldRow
        anchors.fill: parent
        spacing: 8

        Text {
            text: root.label
            color: "#2c3e50"
            visible: root.label.length > 0
            Layout.preferredWidth: root.label.length > 0 ? root.labelWidth : 0
        }

        Button {
            id: openButton
            Layout.fillWidth: true
            flat: true
            padding: 6
            onClicked: root.openPicker()

            contentItem: Text {
                text: root.hasValue ? root.dateTimeText : "点击选择日期时间"
                color: root.hasValue ? "#2c3e50" : "#95a5a6"
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignLeft
                leftPadding: 8
            }

            background: Rectangle {
                implicitHeight: 32
                radius: 4
                color: openButton.down ? "#ecf0f1" : "#ffffff"
                border.color: openButton.enabled ? "#bdc3c7" : "#ecf0f1"
                border.width: 1
            }
        }
    }

    Popup {
        id: pickerPopup
        parent: Overlay.overlay
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: 320
        padding: 12

        x: Math.round((parent.width - width) / 2)
        y: Math.round((parent.height - height) / 2)

        background: Rectangle {
            color: "#ffffff"
            border.color: "#dcdde1"
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 10

            Text {
                Layout.fillWidth: true
                text: (root.label.length > 0 ? (root.label + "时间") : "选择时间")
                font.pixelSize: 14
                font.bold: true
                color: "#2c3e50"
            }

            Text {
                Layout.fillWidth: true
                visible: root.pickerError.length > 0
                wrapMode: Text.WordWrap
                text: root.pickerError
                color: "#e74c3c"
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ComboBox {
                    id: yearCombo
                    Layout.fillWidth: true
                    Layout.preferredWidth: 120
                    model: root.yearModel
                    onActivated: root.applyCalendarHeader()
                }

                ComboBox {
                    id: monthCombo
                    Layout.fillWidth: true
                    Layout.preferredWidth: 100
                    model: root.monthModel
                    onActivated: root.applyCalendarHeader()
                }
            }

            DayOfWeekRow {
                Layout.fillWidth: true
                locale: monthGrid.locale
            }

            MonthGrid {
                id: monthGrid
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                locale: Qt.locale("zh_CN")

                delegate: Item {
                    id: dayDelegate
                    required property var model

                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.min(parent.width, parent.height) - 2
                        height: width
                        radius: width / 2
                        color: root.isSameDay(dayDelegate.model.date, root.draftDate)
                               ? "#3498db" : "transparent"
                    }

                    Text {
                        anchors.centerIn: parent
                        text: dayDelegate.model.day
                        color: {
                            if (root.isSameDay(dayDelegate.model.date, root.draftDate))
                                return "#ffffff"
                            if (root.isFutureDay(dayDelegate.model.date)
                                    || dayDelegate.model.month !== monthGrid.month)
                                return "#bdc3c7"
                            return "#2c3e50"
                        }
                        font.pixelSize: 12
                    }
                }

                onClicked: function(date) {
                    if (root.isFutureDay(date)) {
                        root.pickerError = "不能选择大于当前日期的日期"
                        return
                    }
                    root.pickerError = ""
                    root.draftDate = new Date(date.getFullYear(), date.getMonth(), date.getDate())
                    root.clampDraftToNowIfNeeded()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#ecf0f1"
            }

            Text {
                text: "时间（时:分:秒）"
                color: "#7f8c8d"
                font.pixelSize: 11
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "时"
                        color: "#7f8c8d"
                        font.pixelSize: 11
                    }

                    ComboBox {
                        id: hourCombo
                        Layout.fillWidth: true
                        model: root.hourModel
                        onActivated: root.pickerError = ""
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: ":"
                    color: "#2c3e50"
                    font.pixelSize: 18
                    font.bold: true
                    topPadding: 14
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "分"
                        color: "#7f8c8d"
                        font.pixelSize: 11
                    }

                    ComboBox {
                        id: minuteCombo
                        Layout.fillWidth: true
                        model: root.minuteSecondModel
                        onActivated: root.pickerError = ""
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: ":"
                    color: "#2c3e50"
                    font.pixelSize: 18
                    font.bold: true
                    topPadding: 14
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "秒"
                        color: "#7f8c8d"
                        font.pixelSize: 11
                    }

                    ComboBox {
                        id: secondCombo
                        Layout.fillWidth: true
                        model: root.minuteSecondModel
                        onActivated: root.pickerError = ""
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Button {
                    text: "今天"
                    Layout.fillWidth: true
                    onClicked: {
                        var now = new Date()
                        root.pickerError = ""
                        root.draftDate = new Date(now.getFullYear(), now.getMonth(), now.getDate())
                        monthGrid.month = root.draftDate.getMonth()
                        monthGrid.year = root.draftDate.getFullYear()
                        root.syncTimeDraft(now.getHours(), now.getMinutes(), now.getSeconds())
                        root.syncCalendarHeader()
                    }
                }

                Button {
                    text: "清除"
                    Layout.fillWidth: true
                    onClicked: {
                        root.clear(true)
                        pickerPopup.close()
                    }
                }

                Button {
                    text: "确定"
                    Layout.fillWidth: true
                    highlighted: true
                    onClicked: root.acceptDraft()
                }
            }
        }
    }
}
