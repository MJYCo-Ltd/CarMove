import QtQuick

// 地图动画组件 - 从MapDisplay.qml提取
Item {
    id: mapAnimations
    
    // 公共属性
    property var mapTarget: null
    property bool animationsEnabled: true
    
    // 公共函数
    function animateToCenter(coordinate) {
        if (mapTarget) {
            centerAnimation.target = mapTarget
            centerAnimation.to = coordinate
            centerAnimation.start()
        }
    }
    
    function animateToZoom(zoomLevel) {
        if (mapTarget) {
            zoomAnimation.target = mapTarget
            zoomAnimation.to = zoomLevel
            zoomAnimation.start()
        }
    }
    
    function animateVehicleToLocation(vehicle, coordinate) {
        vehicleLocationAnimation.target = vehicle
        vehicleLocationAnimation.to = coordinate
        vehicleLocationAnimation.start()
    }
    
    function animateVehiclePosition(vehicle, coordinate) {
        if (positionAnimation.target !== vehicle) {
            positionAnimation.target = vehicle
        }
        positionAnimation.to = coordinate
        positionAnimation.start()
    }
    
    function animateVehicleRotation(vehicle, direction) {
        if (rotationAnimation.target !== vehicle) {
            rotationAnimation.target = vehicle
        }
        rotationAnimation.to = direction
        rotationAnimation.start()
    }
    
    // 位置和旋转动画
    PropertyAnimation {
        id: positionAnimation
        property: "coordinate"
        duration: mapAnimations.animationsEnabled ? 1000 : 0
        easing.type: Easing.InOutQuad
    }
    
    RotationAnimation {
        id: rotationAnimation
        property: "direction"
        duration: mapAnimations.animationsEnabled ? 500 : 0
        direction: RotationAnimation.Shortest
    }
    
    // 地图中心点动画
    PropertyAnimation {
        id: centerAnimation
        property: "center"
        duration: 1500
        easing.type: Easing.InOutQuad
    }
    
    // 地图缩放动画
    PropertyAnimation {
        id: zoomAnimation
        property: "zoomLevel"
        duration: 1500
        easing.type: Easing.InOutQuad
    }
    
    // 车辆位置移动动画
    PropertyAnimation {
        id: vehicleLocationAnimation
        property: "coordinate"
        duration: 2000
        easing.type: Easing.InOutQuad
    }
}