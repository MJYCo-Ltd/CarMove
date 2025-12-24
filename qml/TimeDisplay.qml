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
    property color timeColor: "white"
    property color dateColor: "#bdc3c7"
    property int timeFontSize: 12
    property int dateFontSize: 9
    property bool timeFontBold: true
    property string emptyTimeText: "--:--:--"
    property string emptyDateText: "----/--/--"
    
    spacing: 2
    
    // Time display
    Text {
        text: formatTime(timeDisplay.dateTime)
        color: timeDisplay.timeColor
        font.pixelSize: timeDisplay.timeFontSize
        font.bold: timeDisplay.timeFontBold
        visible: timeDisplay.showTime
    }
    
    // Date display
    Text {
        text: formatDate(timeDisplay.dateTime)
        color: timeDisplay.dateColor
        font.pixelSize: timeDisplay.dateFontSize
        visible: timeDisplay.showDate
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
    
    // Utility function to check if data spans multiple days
    function isLongTermData(startTime, endTime) {
        if (!startTime || !endTime || 
            !startTime.getTime || !endTime.getTime) return false
        
        var totalMs = endTime.getTime() - startTime.getTime()
        var totalDays = totalMs / (1000 * 60 * 60 * 24)
        
        return totalDays > 7  // Consider data spanning more than 7 days as long-term
    }
    
    // Get time range information
    function getTimeRangeInfo(startTime, endTime) {
        if (!startTime || !endTime || 
            !startTime.getTime || !endTime.getTime) return ""
        
        var totalMs = endTime.getTime() - startTime.getTime()
        var totalDays = Math.floor(totalMs / (1000 * 60 * 60 * 24))
        var totalHours = Math.floor(totalMs / (1000 * 60 * 60))
        
        if (totalDays > 365) {
            var years = Math.floor(totalDays / 365)
            var remainingDays = totalDays % 365
            return years + "年" + remainingDays + "天"
        } else if (totalDays > 30) {
            var months = Math.floor(totalDays / 30)
            var remainingDays = totalDays % 30
            return months + "月" + remainingDays + "天"
        } else if (totalDays > 0) {
            return totalDays + "天"
        } else {
            return totalHours + "小时"
        }
    }
}