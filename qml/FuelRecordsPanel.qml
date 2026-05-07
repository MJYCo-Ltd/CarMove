import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CarMove 1.0

Rectangle {
    id: fuelRecordsPanel
    color: "#f0f0f0"
    border.color: "#ccc"
    
    // 数据加载器
    FuelUnloadingDataLoader {
        id: dataLoader
        
        onDataLoaded: function(success, message) {
            if (success) {
                console.log("FuelRecordsPanel:", message)
            } else {
                console.error("FuelRecordsPanel: 数据加载失败:", message)
            }
        }
    }
    
    // 信号
    signal vehicleSelected(string plateNumber)
    signal showAllRecords()
    
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10
        
        // 标题和加载状态
        GroupBox {
            title: "卸油记录"
            Layout.fillWidth: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                // 加载状态指示
                Rectangle {
                    Layout.fillWidth: true
                    height: 30
                    color: dataLoader.isLoaded ? "#d5f4e6" : "#ffeaa7"
                    border.color: dataLoader.isLoaded ? "#00b894" : "#fdcb6e"
                    border.width: 1
                    radius: 4
                    
                    Text {
                        anchors.centerIn: parent
                        text: dataLoader.isLoaded ? "✓ 数据已加载" : "⚠ 数据加载中..."
                        color: dataLoader.isLoaded ? "#00b894" : "#e17055"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }
                
                // 错误信息显示
                Text {
                    Layout.fillWidth: true
                    text: dataLoader.errorMessage
                    color: "#e74c3c"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    visible: dataLoader.errorMessage.length > 0
                }
            }
        }
        
        // 统计信息
        GroupBox {
            title: "统计信息"
            Layout.fillWidth: true
            visible: dataLoader.isLoaded
            
            GridLayout {
                anchors.fill: parent
                columns: 2
                columnSpacing: 10
                rowSpacing: 5
                
                Text {
                    text: "总车辆数:"
                    font.pixelSize: 12
                    color: "#2c3e50"
                }
                
                Text {
                    text: dataLoader.isLoaded ? dataLoader.getStatistics().totalVehicles : "0"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#3498db"
                }
                
                Text {
                    text: "总卸油次数:"
                    font.pixelSize: 12
                    color: "#2c3e50"
                }
                
                Text {
                    text: dataLoader.isLoaded ? dataLoader.getStatistics().totalRecords : "0"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#3498db"
                }
                
                Text {
                    text: "汽油总量:"
                    font.pixelSize: 12
                    color: "#2c3e50"
                }
                
                Text {
                    text: (dataLoader.isLoaded ? dataLoader.getStatistics().totalGasoline.toFixed(1) : "0") + " 吨"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#e67e22"
                }
                
                Text {
                    text: "柴油总量:"
                    font.pixelSize: 12
                    color: "#2c3e50"
                }
                
                Text {
                    text: (dataLoader.isLoaded ? dataLoader.getStatistics().totalDiesel.toFixed(2) : "0") + " 吨"
                    font.pixelSize: 12
                    font.bold: true
                    color: "#f39c12"
                }
            }
        }
        
        // 车辆列表
        GroupBox {
            title: "车辆列表"
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            ColumnLayout {
                anchors.fill: parent
                spacing: 5
                
                // 显示所有记录按钮
                Button {
                    text: "显示所有卸油点"
                    Layout.fillWidth: true
                    enabled: dataLoader.isLoaded
                    
                    background: Rectangle {
                        color: parent.enabled ? "#3498db" : "#bdc3c7"
                        border.color: parent.enabled ? "#2980b9" : "#95a5a6"
                        border.width: 1
                        radius: 4
                    }
                    
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 12
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    
                    onClicked: {
                        fuelRecordsPanel.showAllRecords()
                    }
                }
                
                // 车辆列表
                ListView {
                    id: vehicleListView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: dataLoader.isLoaded ? dataLoader.vehicles : []
                    clip: true
                    
                    // 空状态提示
                    Text {
                        anchors.centerIn: parent
                        text: dataLoader.isLoaded ? "未找到卸油记录" : "请等待数据加载..."
                        color: "#7f8c8d"
                        font.pixelSize: 12
                        visible: vehicleListView.count === 0
                        horizontalAlignment: Text.AlignHCenter
                    }
                    
                    delegate: Rectangle {
                        width: vehicleListView.width
                        height: 80
                        color: mouseArea.containsMouse ? "#ecf0f1" : "transparent"
                        border.color: "#bdc3c7"
                        border.width: mouseArea.containsMouse ? 1 : 0
                        radius: 4
                        
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            hoverEnabled: true
                            
                            onClicked: {
                                fuelRecordsPanel.vehicleSelected(modelData.plateNumber)
                            }
                        }
                        
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10
                            
                            // 车辆图标
                            Rectangle {
                                width: 40
                                height: 40
                                color: (typeof controller !== 'undefined' && controller)
                                       ? controller.colorHexForPlate(modelData.plateNumber) : "#3498db"
                                radius: 20
                                
                                Text {
                                    anchors.centerIn: parent
                                    text: "🚛"
                                    font.pixelSize: 20
                                    color: "white"
                                }
                            }
                            
                            // 车辆信息
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                
                                Text {
                                    text: modelData.plateNumber
                                    font.pixelSize: 14
                                    font.bold: true
                                    color: "#2c3e50"
                                }
                                
                                Text {
                                    text: modelData.records.length + " 条卸油记录"
                                    font.pixelSize: 11
                                    color: "#7f8c8d"
                                }
                                
                                Text {
                                    text: (typeof controller !== 'undefined' && controller)
                                          ? (controller.formatRecordsTotalAmount(modelData.records) + " 吨")
                                          : "0.00 吨"
                                    font.pixelSize: 11
                                    color: "#e74c3c"
                                    font.bold: true
                                }
                            }
                            
                            // 箭头指示
                            Text {
                                text: "▶"
                                font.pixelSize: 12
                                color: "#bdc3c7"
                            }
                        }
                    }
                }
            }
        }
    }
    
}