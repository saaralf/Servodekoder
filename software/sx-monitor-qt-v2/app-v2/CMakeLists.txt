set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(sx_monitor_qt_v2
    src/sx_monitor_qt_v2.cpp
    src/sx_bus_bridge.cpp
    ../app-common/src/sx_runtime.cpp
    ../../sx-bus-core/sx_bus_core.c
)

target_include_directories(sx_monitor_qt_v2 PRIVATE
    src
    ../app-common/src
    ../../sx-bus-core
)
target_link_libraries(sx_monitor_qt_v2 PRIVATE Qt6::Widgets)
