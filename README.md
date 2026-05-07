# CarMove 车辆轨迹追踪系统

基于 **Qt 6** 与 **QML** 的桌面应用：从 Excel 读取车辆轨迹，在地图上播放与统计；并集成卸油记录、地点搜索、路线导航（天地图等插件能力依赖本地配置）。

可执行目标名称（CMake）：**CarMoveTracker**。

## 功能概览

| 模块 | 说明 |
|------|------|
| **轨迹** | 选择含 Excel 的文件夹，列表选车；地图轨迹线、车辆朝向/速度、时间轴播放与暂停 |
| **坐标** | WGS84 ↔ GCJ02 转换开关；转换后轨迹可刷新到地图 |
| **目标区域** | `MainController.targetAreaLatitude/Longitude` 存储中心点；地图「定位」与搜索「设为目标区域」写入该属性；经过次数统计在 C++ |
| **搜索** | 地名 + 行政区检索，结果卡片可定位到地图 |
| **导航** | 起终点/途经点选用、路线规划展示（依赖天地图路线与密钥配置） |
| **卸油** | 本地 JSON 数据加载，地图上卸油点与详情 |
| **其它** | 截图、地图类型与缩放/中心持久化（ConfigManager）、QXlsx 读表 |

## 技术栈

- **Qt 6**：Core、Quick、QuickControls2、Qml、Location、Positioning、Network  
- **C++17**、**CMake ≥ 3.16**  
- **QXlsx（Qt6）**：预编译包置于 `install/Qt-Debug` 与 `install/Qt-Release`（与 `CMAKE_BUILD_TYPE` 对应）

## 系统要求

- Windows 10/11（当前工程配置）
- 已安装 **Qt 6.2+** 与对应 MSVC 工具链
- CMake 3.16+

## 构建

```powershell
cd CarMove
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Debug 构建请将 `CMAKE_BUILD_TYPE` 设为 `Debug`，以便 CMake 使用 `install/Qt-Debug` 下的 QXlsx。

运行前请配置天地图等密钥（若使用相关插件）：见应用内配置或 `ConfigManager` 持久化项。

## 目录结构（摘要）

```
├── CMakeLists.txt
├── README.md
├── src/                      # C++：MainController、VehicleManager、Excel 解析、坐标转换、天地图 API 等
├── qml/                      # QML 界面与地图组件（MainWindow、MapDisplay、MapVehicleLayer、GeoPickSearch 等）
├── qml/qml.qrc               # QML 资源清单
├── install/                  # 预编译 QXlsx（Qt-Debug / Qt-Release）
├── carData/                  # 示例或实际 Excel 数据目录（可选）
└── data/                     # 卸油等 JSON 数据（若使用）
```

地图相关可复用 QML 组件示例：`MapLine.qml`（轨迹折线）、`MapPlateBadge.qml`、`MapGeoNameText.qml` 等。

## 业务与界面分工

- **运算与几何**（轨迹解析、路径折线、目标区域统计、播放速度档位、时间轴提示文案、车牌配色哈希、卸油吨位汇总等）集中在 **C++ `MainController`** 与 **VehicleAnimationEngine**（定时推演）中，通过 `controller` 暴露给 QML。  
- **QML** 主要负责布局、地图侧栏按钮（含坐标切换）、地图项生命周期与动画调用；播放条仅调用 `seekToProgress` / `seekProgressDelta` 等接口。

## 许可证

本项目仅供学习与研究使用。
