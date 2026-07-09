import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Text {
    id: root

    required property var excelModel
    property string readyText: "（自动识别标红列）"
    property string missingText: "未检测到车牌列"

    Layout.fillWidth: true
    wrapMode: Text.WordWrap
    color: excelModel && excelModel.defaultPlateColumnNumber > 0 ? "#1e8449" : "#e74c3c"
    text: excelModel && excelModel.defaultPlateColumnNumber > 0
          ? "车牌列：列 " + excelModel.defaultPlateColumnNumber + root.readyText
          : root.missingText
}
