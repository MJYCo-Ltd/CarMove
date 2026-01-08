cmake_minimum_required(VERSION 3.16)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(QXlsxQt6_DIR ${CMAKE_SOURCE_DIR}/install/Qt-Debug/lib/cmake/QXlsxQt6)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(QXlsxQt6_DIR ${CMAKE_SOURCE_DIR}/install/Qt-Release/lib/cmake/QXlsxQt6)
endif()

project(CarMoveTracker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QXlsxQt6 REQUIRED)
# Find Qt6 components
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Quick 
    Location 
    Positioning
    Qml
    QuickControls2
)

# Enable automatic MOC, UIC, and RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)


# Include directories
include_directories(src)

# Source files
set(SOURCES
    src/main.cpp
    src/MainController.cpp
    src/FolderScanner.cpp
    src/ExcelDataReader.cpp
    src/CoordinateConverter.cpp
    src/VehicleManager.cpp
    src/VehicleDataModel.cpp
    src/VehicleAnimationEngine.cpp
    src/ErrorHandler.cpp
    src/ConfigManager.cpp
    src/FuelUnloadingDataLoader.cpp
)

# Header files
set(HEADERS
    src/MainController.h
    src/FolderScanner.h
    src/ExcelDataReader.h
    src/CoordinateConverter.h
    src/VehicleManager.h
    src/VehicleDataModel.h
    src/VehicleAnimationEngine.h
    src/ErrorHandler.h
    src/ConfigManager.h
    src/FuelUnloadingDataLoader.h
)

# QML resources
qt6_add_resources(QML_RESOURCES qml/qml.qrc)

# Create executable
qt6_add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${QML_RESOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    Qt6::Core
    Qt6::Quick
    Qt6::Location
    Qt6::Positioning
    Qt6::Qml
    Qt6::QuickControls2
    QXlsx::QXlsx
)

# Set target properties
set_target_properties(CarMoveTracker PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
