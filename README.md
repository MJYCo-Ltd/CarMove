# CarMove 车辆轨迹追踪系统

基于Qt6的车辆轨迹可视化应用程序，能够从Excel文件中读取车辆位置数据，并在地图界面上实时显示车辆运动轨迹。

## 功能特性

- 从Excel文件读取车辆轨迹数据
- 交互式地图显示车辆位置和轨迹
- 时间序列播放控制
- GPS坐标系转换（WGS84 ↔ GCJ02）
- 车辆运动动画和方向显示

## 系统要求

- Qt 6.2 或更高版本
- CMake 3.16 或更高版本
- C++17 编译器
- Windows 10/11 (当前配置)

## 依赖库

- Qt6 Core, Widgets, Quick, Location, Positioning, Qml
- QXlsx (预编译库位于 install/Qt-Release 目录)

## 构建说明

1. 确保已安装Qt6和CMake
2. 克隆或下载项目代码
3. 在项目根目录创建build目录：
   ```bash
   mkdir build
   cd build
   ```
4. 运行CMake配置：
   ```bash
   cmake ..
   ```
5. 编译项目：
   ```bash
   cmake --build . --config Release
   ```

## 项目结构

```
CarMoveTracker/
├── CMakeLists.txt          # CMake构建配置
├── README.md              # 项目说明文档
├── src/                   # C++源代码
│   ├── main.cpp           # 应用程序入口
│   ├── MainController.*   # 主控制器
│   ├── FolderScanner.*    # 文件夹扫描器
│   ├── ExcelDataReader.*  # Excel数据读取器
│   ├── CoordinateConverter.* # 坐标转换器
│   ├── VehicleManager.*   # 车辆管理器
│   ├── VehicleDataModel.* # 车辆数据模型
│   └── VehicleAnimationEngine.* # 动画引擎
├── qml/                   # QML用户界面
│   ├── MainWindow.qml     # 主窗口
│   ├── MapDisplay.qml     # 地图显示组件
│   ├── PlaybackControls.qml # 播放控制面板
│   └── qml.qrc           # QML资源文件
├── install/               # 预编译库
│   └── Qt-Release/        # QXlsx库文件
└── carData/              # 车辆数据文件夹
    └── *.xlsx            # Excel数据文件
```

## 开发状态

当前项目处于基础架构搭建阶段，已完成：
- ✅ Qt6 CMake项目结构
- ✅ QXlsx库依赖配置
- ✅ 基础C++类框架
- ✅ QML用户界面框架

待实现功能：
- 🔄 坐标系转换算法
- 🔄 Excel数据读取功能
- 🔄 地图显示和车辆可视化
- 🔄 播放控制和动画
- 🔄 错误处理和用户反馈

## 使用说明

1. 启动应用程序
2. 点击"选择文件夹"按钮，选择包含Excel车辆数据的文件夹（如carData目录）
3. 从车辆列表中选择要查看的车辆
4. 使用播放控制面板控制轨迹播放
5. 可以切换GPS坐标系和火星坐标系显示

## 许可证

本项目仅供学习和研究使用。