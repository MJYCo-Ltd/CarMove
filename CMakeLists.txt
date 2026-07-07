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

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Quick
    Location
    Positioning
    Qml
    QuickControls2
    Network
    GuiPrivate
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(src)

set(SOURCES
    src/main.cpp
    src/MainController.cpp
    src/MainControllerSlots.cpp
    src/MainControllerHelpers.cpp
    src/PlaybackControl.cpp
    src/FolderScanner.cpp
    src/ParseData/ExcelDataReader.cpp
    src/ParseData/ExcelRowParser.cpp
    src/ParseData/ExcelPreviewLoader.cpp
    src/ParseData/ExcelCellFormatter.cpp
    src/ParseData/ExcelSheetGridUtils.cpp
    src/ParseData/ExcelLoadUtils.cpp
    src/ParseData/ExcelPreviewModel.cpp
    src/ParseData/ExcelFilePath.cpp
    src/ParseData/LicensePlateDetector.cpp
    src/ParseData/DateColumnDetector.cpp
    src/ParseData/BusinessExcelExporter.cpp
    src/ParseData/OoxmlSaxExcelLoader.cpp
    ${QXLSX_SOURCE_DIR}/source/xlsxreadsax.cpp
    ${QXLSX_SOURCE_DIR}/source/xlsxzipreader.cpp
    src/ParseData/QXlsxExcelLoader.cpp
    src/ParseData/XlsxioExcelLoader.cpp
    src/CoordinateConverter.cpp
    src/VehicleManager.cpp
    src/VehicleDataModel.cpp
    src/VehicleAnimationEngine.cpp
    src/ErrorHandler.cpp
    src/ConfigManager.cpp
    src/FuelUnloadingDataLoader.cpp
    src/TianDiTu/TiandituGeocoder.cpp
    src/TianDiTu/TiandituRoutePlanner.cpp
)

set(HEADERS
    src/MainController.h
    src/PlaybackControl.h
    src/FolderScanner.h
    src/ParseData/ExcelDataReader.h
    src/ParseData/ExcelBackend.h
    src/ParseData/ExcelRowParser.h
    src/ParseData/ExcelPreviewLoader.h
    src/ParseData/ExcelCellFormatter.h
    src/ParseData/ExcelSheetGridUtils.h
    src/ParseData/ExcelLoadUtils.h
    src/ParseData/ExcelPreviewModel.h
    src/ParseData/ExcelFilePath.h
    src/ParseData/LicensePlateDetector.h
    src/ParseData/DateColumnDetector.h
    src/ParseData/BusinessExcelExporter.h
    src/ParseData/OoxmlSaxExcelLoader.h
    src/ParseData/QXlsxExcelLoader.h
    src/ParseData/XlsxioExcelLoader.h
    src/CoordinateConverter.h
    src/VehicleManager.h
    src/VehicleDataModel.h
    src/VehicleAnimationEngine.h
    src/ErrorHandler.h
    src/ConfigManager.h
    src/FuelUnloadingDataLoader.h
    src/TianDiTu/TiandituGeocoder.h
    src/TianDiTu/TiandituRoutePlanner.h
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
    QXlsx::QXlsx
    xlsxio::xlsxio_read_SHARED
)

target_include_directories(${PROJECT_NAME} PRIVATE "${QXLSX_HEADER_DIR}")

set_target_properties(CarMoveTracker PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
