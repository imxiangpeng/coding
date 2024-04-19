cmake_minimum_required(VERSION 3.10)

PROJECT(hrbinder C)

# RELEASE OR DEBUG
SET(CMAKE_BUILD_TYPE RELEASE)

INCLUDE_DIRECTORIES(./)

link_directories($ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib)

SET(SOURCES hrbinder.c)

ADD_DEFINITIONS(-Wall -Wextra -Werror -Wno-error=unused-function)
ADD_LIBRARY(hrbinder STATIC ${SOURCES})

target_include_directories(hrbinder PUBLIC .)


