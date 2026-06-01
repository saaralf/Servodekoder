set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(sx_monitor_qt_v3
    src/main_v3.cpp
    src/mainwindow_v3.cpp
    src/connection_panel.cpp
    src/dual_runtime_controller.cpp
    ../app-common/src/sx_runtime.cpp
    ../../sx-bus-core/sx_bus_core.c
)

target_include_directories(sx_monitor_qt_v3 PRIVATE
    src
    ../app-common/src
    ../../sx-bus-core
)
target_link_libraries(sx_monitor_qt_v3 PRIVATE Qt6::Widgets)

add_custom_command(TARGET sx_monitor_qt_v3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:sx_monitor_qt_v3>/../assets"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/servo_body_blue.png"
        "$<TARGET_FILE_DIR:sx_monitor_qt_v3>/../assets/servo_body_blue.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/servo_arm_new.png"
        "$<TARGET_FILE_DIR:sx_monitor_qt_v3>/../assets/servo_arm_new.png"
)
