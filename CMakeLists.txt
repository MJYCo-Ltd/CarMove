cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "E:/Code/Git/OpenSouce/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(CarMoveTracker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QXLSX_INSTALL_ROOT "E:/Code/Git/OpenSouce/QXlsx/QXlsx/out/install" CACHE PATH "QXlsx install root")
set(QXLSX_SOURCE_DIR "${QXLSX_INSTALL_ROOT}/../.." CACHE PATH "QXlsx source root")
set(QXLSX_HEADER_DIR "${QXLSX_SOURCE_DIR}/header" CACHE PATH "QXlsx source headers")
set(VCPKG_ROOT "E:/Code/Git/OpenSouce/vcpkg" CACHE PATH "vcpkg root")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "vcpkg target triplet")

list(APPEND CMAKE_PREFIX_PATH "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(QXlsxQt6_DIR "${QXLSX_INSTALL_ROOT}/Qt-Debug/lib/cmake/QXlsxQt6")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(QXlsxQt6_DIR "${QXLSX_INSTALL_ROOT}/Qt-Release/lib/cmake/QXlsxQt6")
else()
    list(APPEND CMAKE_PREFIX_PATH
        "${QXLSX_INSTALL_ROOT}/Qt-Debug"
        "${QXLSX_INSTALL_ROOT}/Qt-Release")
endif()

find_package(QXlsxQt6 REQUIRED)
find_package(xlsxio CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Quick
    Location
    Positioning
    Qml
    QuickControls2
    Network
    GuiPrivate
    Sql
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(src)

set(SOURCES
    # Core
    src/Core/main.cpp
    src/Core/AppLogger.cpp
    src/Core/ErrorHandler.cpp
    src/Core/ConfigManager.cpp
    # UI
    src/UI/MainController.cpp
    src/UI/MainControllerSlots.cpp
    src/UI/MainControllerHelpers.cpp
    src/UI/PlaybackControl.cpp
    # Map
    src/Map/VehicleAnimationEngine.cpp
    src/Map/CoordinateConverter.cpp
    src/Map/TianDiTu/TiandituGeocoder.cpp
    src/Map/TianDiTu/TiandituRoutePlanner.cpp
    # DataManagement
    src/DataManagement/FolderScanner.cpp
    src/DataManagement/VehicleManager.cpp
    src/DataManagement/VehicleDataModel.cpp
    src/DataManagement/PostGisDatabaseConfig.cpp
    src/DataManagement/PostGisConnection.cpp
    src/DataManagement/PostGisTrajectoryLoader.cpp
    src/DataManagement/PostGisTrajectoryImporter.cpp
    src/DataManagement/FuelUnloadingDataLoader.cpp
    # DataParsing
    src/DataParsing/ExcelDataReader.cpp
    src/DataParsing/ExcelRowParser.cpp
    src/DataParsing/ExcelLoadUtils.cpp
    src/DataParsing/TrajectoryFileNaming.cpp
    # ExcelDriver
    src/ExcelDriver/ExcelPreviewLoader.cpp
    src/ExcelDriver/ExcelCellFormatter.cpp
    src/ExcelDriver/ExcelSheetGridUtils.cpp
    src/ExcelDriver/ExcelFilePath.cpp
    src/ExcelDriver/OoxmlSaxExcelLoader.cpp
    src/ExcelDriver/QXlsxExcelLoader.cpp
    src/ExcelDriver/XlsxioExcelLoader.cpp
    ${QXLSX_SOURCE_DIR}/source/xlsxreadsax.cpp
    ${QXLSX_SOURCE_DIR}/source/xlsxzipreader.cpp
    # Business
    src/Business/BusinessColumnIdentifier.cpp
    src/Business/LicensePlateDetector.cpp
    src/Business/DateColumnDetector.cpp
    src/Business/BusinessExcelExporter.cpp
    src/Business/BusinessWorkbookResolver.cpp
    src/Business/BusinessCsvWriter.cpp
    src/Business/ExcelPreviewModel.cpp
)

set(HEADERS
    # Core
    src/Core/AppLogger.h
    src/Core/ErrorHandler.h
    src/Core/ConfigManager.h
    # UI
    src/UI/MainController.h
    src/UI/PlaybackControl.h
    # Map
    src/Map/VehicleAnimationEngine.h
    src/Map/CoordinateConverter.h
    src/Map/TianDiTu/TiandituGeocoder.h
    src/Map/TianDiTu/TiandituRoutePlanner.h
    # DataManagement
    src/DataManagement/FolderScanner.h
    src/DataManagement/VehicleManager.h
    src/DataManagement/VehicleDataModel.h
    src/DataManagement/PostGisDatabaseConfig.h
    src/DataManagement/PostGisConnection.h
    src/DataManagement/PostGisSqlHelpers.h
    src/DataManagement/PostGisTrajectoryLoader.h
    src/DataManagement/PostGisTrajectoryImporter.h
    src/DataManagement/FuelUnloadingDataLoader.h
    # DataParsing
    src/DataParsing/ExcelDataReader.h
    src/DataParsing/ExcelRowParser.h
    src/DataParsing/ExcelLoadUtils.h
    src/DataParsing/TrajectoryFileNaming.h
    # ExcelDriver
    src/ExcelDriver/ExcelBackend.h
    src/ExcelDriver/ExcelPreviewLoader.h
    src/ExcelDriver/ExcelCellFormatter.h
    src/ExcelDriver/ExcelSheetGridUtils.h
    src/ExcelDriver/ExcelFilePath.h
    src/ExcelDriver/ExcelPreviewColumnUtils.h
    src/ExcelDriver/OoxmlSaxExcelLoader.h
    src/ExcelDriver/QXlsxExcelLoader.h
    src/ExcelDriver/XlsxioExcelLoader.h
    # Business
    src/Business/BusinessColumnIdentifier.h
    src/Business/BusinessWorkbookTypes.h
    src/Business/LicensePlateDetector.h
    src/Business/DateColumnDetector.h
    src/Business/BusinessWorkbookResolver.h
    src/Business/BusinessExcelExporter.h
    src/Business/BusinessCsvWriter.h
    src/Business/ExcelPreviewModel.h
)

qt6_add_resources(QML_RESOURCES qml/qml.qrc)

qt6_add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${QML_RESOURCES}
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
    Qt6::Core
    Qt6::Quick
    Qt6::Location
    Qt6::Positioning
    Qt6::Qml
    Qt6::QuickControls2
    Qt6::Network
    Qt6::GuiPrivate
    Qt6::Sql
    QXlsx::QXlsx
    xlsxio::xlsxio_read_SHARED
    spdlog::spdlog
)

target_include_directories(${PROJECT_NAME} PRIVATE "${QXLSX_HEADER_DIR}")

target_compile_definitions(${PROJECT_NAME} PRIVATE QT_MESSAGELOGCONTEXT)

set_target_properties(CarMoveTracker PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
