cmake_minimum_required(VERSION 3.16)
project(sx_monitor_qt LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)
add_executable(sx_monitor_qt_v2
    sx_monitor_qt_v2.cpp
    sx_bus_bridge.cpp
    sx_runtime.cpp
    ../sx-bus-core/sx_bus_core.c
)
target_include_directories(sx_monitor_qt_v2 PRIVATE ../sx-bus-core)
target_link_libraries(sx_monitor_qt_v2 PRIVATE Qt6::Widgets)
