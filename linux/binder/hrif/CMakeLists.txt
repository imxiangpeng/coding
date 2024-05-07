cmake_minimum_required(VERSION 3.10)

PROJECT(HROUTER_HRIF C)

# RELEASE OR DEBUG
SET(CMAKE_BUILD_TYPE RELEASE)

INCLUDE_DIRECTORIES(include)

add_subdirectory(impl)


INCLUDE_DIRECTORIES($ENV{HI_HISI_BASIC_DIR}/include $ENV{HI_HISI_BASIC_DIR}/linux)

link_directories($ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib)

ADD_DEFINITIONS(-Wall -Wextra -Werror)
ADD_DEFINITIONS(-funwind-tables)

ADD_EXECUTABLE(hrifd hrifd/hrifd.c
            hrifd/hrifd_system.c
            hrifd/hrifd_network.c
            hrifd/hrifd_wireless.c
            hrifd/hrifd_easymesh.c)

TARGET_LINK_LIBRARIES(hrifd hr_base)
TARGET_LINK_LIBRARIES(hrifd hrbinder)
TARGET_LINK_LIBRARIES(hrifd hrif_impl)
TARGET_LINK_LIBRARIES(hrifd unwind unwind-generic)

add_dependencies(hrifd odm_thirdparty_libunwind_build)

TARGET_COMPILE_DEFINITIONS(hrifd PRIVATE LOG_TAG="hrifd")

SET_TARGET_PROPERTIES(hrifd PROPERTIES LINK_FLAGS "-Wl,-rpath-link,$ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib")

# sample code, default do not install.
INSTALL(TARGETS hrifd DESTINATION $ENV{HI_INSTALL_ROOTFS_DIR}/usr/bin)
INSTALL(FILES hrsvc.d/hrifd.json DESTINATION $ENV{HI_INSTALL_ROOTFS_DIR}/etc/hrsvc.d)
INSTALL(FILES hrsvc.d/diag.json DESTINATION $ENV{HI_INSTALL_ROOTFS_DIR}/etc/hrsvc.d)


SET(SOURCES hrif_network.c hrif_system.c hrif_wireless.c hrif_update.c hrif_easymesh.c hrif_diagnostics.c hrif_table.c)

ADD_LIBRARY(hrif SHARED hrif.c ${SOURCES})

TARGET_COMPILE_DEFINITIONS(hrif PRIVATE LOG_TAG="hrif")
TARGET_COMPILE_OPTIONS(hrif PRIVATE "-Wno-error=format-truncation")

TARGET_LINK_LIBRARIES(hrif hr_base)
TARGET_LINK_LIBRARIES(hrif hrbinder)

# notify depends include j2s
target_include_directories(hrif PUBLIC include)

SET_TARGET_PROPERTIES(hrif PROPERTIES LINK_FLAGS "-Wl,-rpath-link,$ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib")

if(EXISTS $ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib)
INSTALL(TARGETS hrif DESTINATION $ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib)
endif()
