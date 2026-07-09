# CarMove 车辆轨迹追踪系统

基于 **Qt 6** 与 **QML** 的桌面应用：从 Excel 读取车辆轨迹，在地图上播放与统计；支持业务 Excel 处理、PostGIS 数据库、地点搜索与路线导航。

可执行目标名称（CMake）：**CarMoveTracker**。当前版本：**2.0**。

## 功能概览

| 模块 | 说明 |
|------|------|
| **轨迹** | 从 Excel 文件夹或 PostGIS 加载车辆列表；地图轨迹线、车辆位置；分段时间轴拖动定位 |
| **业务** | 打开 Excel 自动识别并标出**车牌列**与**时间列**；导出 CSV、轨迹归类、导入数据库、批量截图 |
| **坐标** | WGS84 ↔ GCJ02 转换开关；转换后轨迹可刷新到地图 |
| **目标区域** | 地图定位与搜索可设目标区域；C++ 统计车辆经过次数 |
| **搜索** | 地名 + 行政区检索（天地图），结果卡片可定位到地图 |
| **导航** | 起终点/途经点选用、驾车路线规划与展示 |
| **其它** | 地图截图、地图类型与缩放/中心持久化、spdlog 日志 |

## 技术栈

- **Qt 6**：Core、Quick、QuickControls2、Qml、Location、Positioning、Network、Sql
- **C++17**、**CMake ≥ 3.16**
- **QXlsx（Qt6）**：小文件 Excel 读写（&lt; 100MB）
- **xlsxio**：大文件 Excel 流式读取（≥ 100MB）
- **spdlog**：日志
- **PostGIS / PostgreSQL**：可选轨迹数据源与批量导入
- **vcpkg**：第三方依赖管理（xlsxio、spdlog 等）

## 系统要求

- Windows 10/11（当前工程配置）
- Qt 6.2+ 与对应 MSVC 工具链
- CMake 3.16+
- vcpkg（含 `xlsxio`、`spdlog`）
- 预编译 QXlsx（见 CMake 中 `QXLSX_INSTALL_ROOT`）

## 构建

```powershell
cd CarMove
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

Debug 构建请将 `CMAKE_BUILD_TYPE` 设为 `Debug`，以便 CMake 使用 QXlsx 的 Debug 安装包。

首次配置前请确认 `CMakeLists.txt` 中的 `QXLSX_INSTALL_ROOT`、`VCPKG_ROOT` 等路径与本机一致。运行前请配置天地图 API 密钥（应用内或 `ConfigManager` 持久化项）。

## 目录结构

代码按职责拆分为独立模块（C++ 在 `src/`，界面在 `qml/`）：

```
CarMove/
├── CMakeLists.txt
├── README.md
├── AdminCode.csv              # 天地图行政区划国标码（打包进 qrc）
├── scripts/
│   └── postgis_schema.sql     # PostGIS 默认表结构
├── src/
│   ├── Core/                  # 入口、日志、错误处理、配置
│   ├── UI/                    # MainController
│   ├── Map/                   # 坐标转换、天地图 API
│   ├── Domain/                  # TrajectoryPoint、VehicleSummary、VehicleDayTrajectory
│   ├── DataManagement/          # TrajectoryDataManager、PostGisDataManager、TrajectoryTimelineManager
│   ├── DataParsing/             # Excel 行解析、轨迹文件命名
│   ├── ExcelDriver/             # ExcelParserManager、ExcelTrajectoryManager（内部 QXlsx/xlsxio）
│   └── Business/              # 业务预览、列识别、导出/归类/CSV
└── qml/
    ├── qml.qrc                # QML 资源（alias 保持 qrc:/ 路径不变）
    ├── UI/                    # 主窗口、侧边栏、轨迹/搜索/导航面板
    ├── Map/                   # 地图、车辆图层、标记、动画
    └── Business/              # 业务面板与相关对话框
```

### 模块职责

| 目录 | 职责 |
|------|------|
| **Core** | `main.cpp`、`AppLogger`、`ErrorHandler`、`ConfigManager`、**`FilePathManager`**、**`LocalFilePath`**（本地路径规范化） |
| **UI** | QML 与 C++ 的桥接：`MainController` |
| **Map** | **`MapServiceManager`**（天地图 API）、坐标系转换 |
| **Domain** | **`TrajectoryPoint` / `VehicleSummary` / `VehicleDayTrajectory`** |
| **DataManagement** | **`TrajectoryDataManager`**、**`PostGisDataManager`**、**`TrajectoryImportManager`**、**`TrajectoryTimeIndex`**、**`TrajectoryTimelineManager`** |
| **DataParsing** | Excel 行解析 → `TrajectoryPoint`、轨迹文件命名 |
| **ExcelDriver** | 对外仅 **`ExcelPreviewTypes.h`**、**`ExcelParserManager`**、**`ExcelTrajectoryManager`**；预览/解析实现（QXlsx、xlsxio、OOXML SAX）均为内部 `.cpp` |
| **Business** | **`BusinessDataManager`**（业务工作流）、列识别（车牌/时间）、导出 CSV、轨迹文件归类 |

### 模块间通信

各模块通过**管理者（Manager）**对外暴露能力，上层（`MainController` / QML）不直接依赖具体实现。

**领域数据结构**（`Domain/TrajectoryTypes.h`）：

| 类型 | 含义 |
|------|------|
| `TrajectoryPoint` | 单个 GPS 轨迹点 |
| `VehicleSummary` | 车辆目录项（Excel 文件夹或 PostGIS 均可） |
| `VehicleDayTrajectory` | 按日历日聚合的轨迹（对应 PostGIS `trajectory_days`） |

**数据读写分层**：

```
UI / VehicleManager
    └── TrajectoryDataManager          ← 切换 folder / database
            ├── ExcelTrajectoryManager ← Excel 扫描 + 轨迹读取
            │       └── ExcelParserManager（内部：QXlsx / xlsxio 自动切换）
            └── PostGisDataManager     ← PostGIS 车辆列表 + 轨迹读取 + Excel 导入写入
            └── PostGisDataManager（内部实现，无独立 Loader/Importer）

业务导入：BusinessDataManager → TrajectoryImportManager → PostGisDataManager
```

```
UI (MainController)
    ├── TrajectoryDataManager      ← 轨迹数据源（folder / database）
    │       ├── ExcelTrajectoryManager
    │       └── PostGisDataManager
    ├── VehicleManager             ← 车辆状态 + 坐标转换
    ├── TrajectoryTimelineManager  ← 轨迹时间轴（时刻索引、分段、拖动定位）
    ├── MapServiceManager          ← 天地图 geocoder / routePlanner
    ├── FilePathManager            ← 本地路径与截图文件名
    ├── ExcelParserManager         ← 业务 Excel 预览（经 BusinessDataManager）
    └── BusinessDataManager        ← 业务 Excel 工作流（经 ExcelPreviewModel 暴露）
            └── TrajectoryImportManager → PostGisDataManager
```

| 管理者 | 切换 / 调用方式 |
|--------|----------------|
| **TrajectoryDataManager** | `controller.setTrajectorySourceMode("folder" \| "database")` |
| **ExcelTrajectoryManager** | 经 `TrajectoryDataManager` 扫描文件夹、加载轨迹 |
| **PostGisDataManager** | 经 `TrajectoryDataManager` 读库；经 `TrajectoryImportManager` 写入 |
| **ExcelParserManager** | 业务 Excel 预览；轨迹读取经 `ExcelTrajectoryManager` |
| **TrajectoryTimelineManager** | `controller.trajectoryCurrentTime` / `seekTrajectorySegment`（时间轴拖动定位） |
| **MapServiceManager** | `mapService` / 兼容别名 `geocoder`、`routePlanner` |
| **FilePathManager** | `controller.paths` 或 `controller.screenshotFilePath(...)` |
| **BusinessDataManager** | `excelPreviewModel.businessData`（打开/导出/归类/截图/导入） |

- **切换数据源**：`controller.setTrajectorySourceMode("folder" | "database")`
- **加载轨迹**：统一返回 `QList<TrajectoryPoint>`，UI 不区分 Excel 文件或 PostGIS SQL
- **导入 PostGIS**：`TrajectoryImportManager` → `PostGisDataManager::importFolder()`，内部用 `ExcelParserManager` 解析再按日写入

## 业务 Excel 流程

1. **打开 Excel**：加载预览表格，自动识别并高亮车牌列（红）与时间列（绿）
2. **导出 / 归类 / 截图**：用户手动触发，基于已识别列处理数据
3. **导入数据库**：将轨迹文件夹批量写入 PostGIS

Excel 驱动层只负责读取与后端切换；列识别逻辑在 `Business/BusinessColumnIdentifier`。解析后端选择（小文件 QXlsx、大文件 xlsxio、兼容格式回退）封装在 `ExcelDriver/ExcelParserManager`，业务与轨迹模块均通过该入口访问。

## 架构说明

- **运算与几何**（轨迹解析、路径折线、目标区域统计、车牌配色等）集中在 C++ 各 Manager 与 `MainController` 中，通过 `controller`、`mapService` 等上下文属性暴露给 QML
- **QML** 负责布局、地图交互与动画；时间轴条调用 `seekTrajectoryToProgress` / `seekTrajectorySegment` 等接口
- QML 类型在 `main.cpp` 中注册为 `CarMove 1.0` 模块

## 许可证

本项目仅供学习与研究使用。
