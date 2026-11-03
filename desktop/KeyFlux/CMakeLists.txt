cmake_minimum_required(VERSION 3.24)
project(KeyFlux VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Quick Qml QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)

# Use the same HIDAPI backend on Windows for both keyboards. Build it
# statically to avoid shipping an extra hidapi.dll with the executable.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Use static dependencies" FORCE)
set(HIDAPI_BUILD_HIDTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG hidapi-0.15.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(hidapi)

qt_add_executable(KeyFlux
    src/main.cpp
    src/DeviceBackend.h
    src/DeviceBackend.cpp
)
set_target_properties(KeyFlux PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
qt_add_qml_module(KeyFlux
    URI KeyFlux
    VERSION 1.0
    QML_FILES qml/Main.qml
)
target_link_libraries(KeyFlux PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::Qml Qt6::QuickControls2
    hidapi::hidapi
)
if(WIN32)
    target_compile_definitions(KeyFlux PRIVATE UNICODE _UNICODE)
endif()
