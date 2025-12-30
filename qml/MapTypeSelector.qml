import QtQuick
import QtQuick.Controls

// 地图类型选择器组件 - 从MapDisplay.qml提取
Item {
    id: mapTypeSelector
    
    // 公共属性
    property var availableMapTypes: []
    property int currentMapTypeIndex: 0
    property var mapTypeNames: []
    
    // 信号
    signal mapTypeSelected(int index)
    
    width: mapTypeSelectorMouseArea.containsMouse || mapTypeComboBox.popup.visible ? 180 : 40
    height: 40
    
    // 平滑的宽度动画
    Behavior on width {
        NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
    }
    
    // 背景容器
    Rectangle {
        id: mapTypeSelectorBackground
        anchors.fill: parent
        color: "#9b59b6"
        border.color: "white"
        border.width: 1
        radius: (mapTypeSelectorMouseArea.containsMouse || mapTypeComboBox.popup.visible) ? 6 : 20  // 圆形到圆角矩形
        
        // 平滑的圆角动画
        Behavior on radius {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }
    }
    
    // 圆形按钮状态的图标
    Text {
        id: mapTypeIcon
        text: "🗺️"
        font.pixelSize: 16
        anchors.centerIn: parent
        visible: !mapTypeSelectorMouseArea.containsMouse && !mapTypeComboBox.popup.visible
        opacity: (mapTypeSelectorMouseArea.containsMouse || mapTypeComboBox.popup.visible) ? 0 : 1
        
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }
    
    // 展开状态的下拉选择框
    ComboBox {
        id: mapTypeComboBox
        anchors.fill: parent
        anchors.margins: 1
        visible: mapTypeSelectorMouseArea.containsMouse || popup.visible
        opacity: (mapTypeSelectorMouseArea.containsMouse || popup.visible) ? 1 : 0
        
        model: mapTypeSelector.mapTypeNames
        currentIndex: mapTypeSelector.currentMapTypeIndex
        
        // 透明背景，因为外层已有背景
        background: Rectangle {
            color: "transparent"
            radius: 5
        }
        
        contentItem: Text {
            text: mapTypeComboBox.displayText
            font.pixelSize: 12
            color: "white"
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 30
            elide: Text.ElideRight
        }
        
        // 下拉箭头
        indicator: Text {
            text: "▼"
            color: "white"
            font.pixelSize: 10
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
        }
        
        // 下拉列表样式
        popup: Popup {
            y: mapTypeComboBox.height
            width: mapTypeComboBox.width
            height: Math.min(contentItem.implicitHeight, 200)
            padding: 1
            
            background: Rectangle {
                color: "#34495e"
                border.color: "#9b59b6"
                border.width: 1
                radius: 6
            }
            
            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: mapTypeComboBox.popup.visible ? mapTypeComboBox.delegateModel : null
                currentIndex: mapTypeComboBox.highlightedIndex
                
                ScrollIndicator.vertical: ScrollIndicator { }
            }
        }
        
        // 列表项样式
        delegate: ItemDelegate {
            width: mapTypeComboBox.width
            height: 35
            
            background: Rectangle {
                color: parent.hovered ? "#9b59b6" : "transparent"
                radius: 4
            }
            
            contentItem: Text {
                text: modelData
                color: "white"
                font.pixelSize: 12
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                elide: Text.ElideRight
            }
        }
        
        onActivated: function(index) {
            mapTypeSelector.mapTypeSelected(index)
        }
        
        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }
    
    // 鼠标区域控制悬停效果 - 只在ComboBox不可见时生效
    MouseArea {
        id: mapTypeSelectorMouseArea
        anchors.fill: parent
        hoverEnabled: true
        enabled: !mapTypeComboBox.popup.visible  // 当popup显示时禁用MouseArea
        
        onContainsMouseChanged: {
            if (!containsMouse && !mapTypeComboBox.popup.visible) {
                // 延迟收起，给用户时间移动到下拉列表
                collapseTimer.restart()
            } else if (containsMouse) {
                collapseTimer.stop()
            }
        }
    }
    
    // 延迟收起定时器
    Timer {
        id: collapseTimer
        interval: 500  // 增加到500ms延迟，给用户更多时间
        onTriggered: {
            if (!mapTypeSelectorMouseArea.containsMouse && !mapTypeComboBox.popup.visible) {
                // 确保鼠标不在组件上且下拉列表未打开时才收起
            }
        }
    }
    
    // 公共函数
    function updateMapTypes(mapTypes) {
        availableMapTypes = []
        var names = []
        
        for (var i = 0; i < mapTypes.length; i++) {
            var mapType = mapTypes[i]
            availableMapTypes.push(mapType)
            
            // 创建显示名称，优先使用 description，如果没有则使用 name
            var displayName = mapType.description || mapType.name || ("地图类型 " + (i + 1))
            names.push(displayName)
        }
        
        mapTypeNames = names
        mapTypeComboBox.model = names
        
        if (availableMapTypes.length > 0) {
            currentMapTypeIndex = 0
            mapTypeComboBox.currentIndex = 0
        }
    }
    
    function setCurrentIndex(index) {
        if (index >= 0 && index < availableMapTypes.length) {
            currentMapTypeIndex = index
            mapTypeComboBox.currentIndex = index
        }
    }
}