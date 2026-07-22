import QtQuick
import QtQuick.Controls

Column {
    id: timeDisplay
    
    // Public properties for customization
    property var dateTime: null
    property string timeFormat: "hh:mm:ss"
    property string dateFormat: "yyyy/MM/dd"
    property bool showDate: false
    property bool showTime: true
    /// 为 true 时日期显示在时间上方（时间轴跨天场景）
    property bool dateAboveTime: false
    property color timeColor: "white"
    property color dateColor: "#bdc3c7"
    property int timeFontSize: 12
    property int dateFontSize: 9
    property bool timeFontBold: true
    property string emptyTimeText: "--:--:--"
    property string emptyDateText: "----/--/--"
    
    spacing: 2
    
    Text {
        text: formatDate(timeDisplay.dateTime)
        color: timeDisplay.dateColor
        font.pixelSize: timeDisplay.dateFontSize
        visible: timeDisplay.showDate && timeDisplay.dateAboveTime
    }
    
    // Time display
    Text {
        text: formatTime(timeDisplay.dateTime)
        color: timeDisplay.timeColor
        font.pixelSize: timeDisplay.timeFontSize
        font.bold: timeDisplay.timeFontBold
        visible: timeDisplay.showTime
    }
    
    // Date display (below time)
    Text {
        text: formatDate(timeDisplay.dateTime)
        color: timeDisplay.dateColor
        font.pixelSize: timeDisplay.dateFontSize
        visible: timeDisplay.showDate && !timeDisplay.dateAboveTime
    }
    
    // Time formatting function
    function formatTime(dateTime) {
        if (!dateTime || !dateTime.getTime || dateTime.getTime() === 0) {
            return emptyTimeText
        }
        return Qt.formatDateTime(dateTime, timeFormat)
    }
    
    // Date formatting function
    function formatDate(dateTime) {
        if (!dateTime || !dateTime.getTime || dateTime.getTime() === 0) {
            return emptyDateText
        }
        return Qt.formatDateTime(dateTime, dateFormat)
    }
}