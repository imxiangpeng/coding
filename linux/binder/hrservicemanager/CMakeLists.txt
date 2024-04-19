cmake_minimum_required(VERSION 3.10)

PROJECT(odm_system_hrservicemanager C)

INCLUDE_DIRECTORIES($ENV{HI_HISI_BASIC_DIR}/include $ENV{HI_HISI_BASIC_DIR}/linux)

link_directories($ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib)

ADD_EXECUTABLE(hrservicemanager service_manager.c)
TARGET_LINK_LIBRARIES(hrservicemanager hrbinder)
#SET_TARGET_PROPERTIES(hrmgt PROPERTIES LINK_FLAGS "-Wl,-rpath-link,$ENV{HI_INSTALL_ROOTFS_DIR}/usr/lib")

# sample code, default do not install.
INSTALL(TARGETS hrservicemanager DESTINATION $ENV{HI_INSTALL_ROOTFS_DIR}/usr/bin)


ADD_EXECUTABLE(bctest bctest.c)
TARGET_LINK_LIBRARIES(bctest hrbinder)

