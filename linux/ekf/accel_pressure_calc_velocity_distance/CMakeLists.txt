cmake_minimum_required(VERSION 3.12)

project(tinyekf_accel_pressure)

include_directories(".")

add_executable(tinyekf main.c)
target_link_libraries(tinyekf m)

add_executable(tinyekf_pressure pressure.c)
target_link_libraries(tinyekf_pressure m)

add_executable(tinyekf_pressure_jump pressure_jump.c)
target_link_libraries(tinyekf_pressure_jump m)

add_executable(tinyekf_pressure_delta_accel pressure_delta_accel.c)
target_link_libraries(tinyekf_pressure_delta_accel m)

add_executable(jerk jerk_statemachine_normalize2.c)
target_link_libraries(jerk m)



