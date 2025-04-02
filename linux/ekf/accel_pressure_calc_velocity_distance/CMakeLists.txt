cmake_minimum_required(VERSION 3.12)

project(tinyekf_accel_pressure)

include_directories(".")

add_executable(tinyekf main.c)
target_link_libraries(tinyekf m)

add_executable(tinyekf_pressure pressure.c)
target_link_libraries(tinyekf_pressure m)



