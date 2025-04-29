cmake_minimum_required(VERSION 3.12)

project(fftw LANGUAGES C)

FIND_LIBRARY(libfftw3_library NAMES fftw3)

include_directories(".")

add_executable(fftw_test main.c)

target_link_libraries(fftw_test ${libfftw3_library})
target_link_libraries(fftw_test m)



