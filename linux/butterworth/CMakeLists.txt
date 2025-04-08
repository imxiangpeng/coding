cmake_minimum_required(VERSION 3.10)

# Set the project name
project(AccelCalculator C)

# Set the C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED True)

# Add the executable
add_executable(butter_worth main.c butter_worth.c)

# Link math library (needed for functions like sqrt, pow, etc. if used)
target_link_libraries(butter_worth m)
