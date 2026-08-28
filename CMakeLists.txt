cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VITASDK})
        set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "toolchain file")
    else()
        message(FATAL_ERROR "Please define VITASDK environment variable")
    endif()
endif()

project(VitaPNGGame C)

include("$ENV{VITASDK}/share/vita.cmake" REQUIRED)

set(VITA_APP_NAME "Vita PNG Game")
set(VITA_TITLEID "VPNG00001")
set(VITA_VERSION "01.00")

file(READ "${CMAKE_SOURCE_DIR}/assets/player_atlas.png.b64" PLAYER_ATLAS_B64)
string(REPLACE "\n" "" PLAYER_ATLAS_B64 "${PLAYER_ATLAS_B64}")
string(REPLACE "\r" "" PLAYER_ATLAS_B64 "${PLAYER_ATLAS_B64}")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/player_atlas.h.in"
    "${CMAKE_BINARY_DIR}/generated/player_atlas.h"
    @ONLY
)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_BINARY_DIR}/generated"
)

target_link_libraries(${PROJECT_NAME}
    vita2d
    SceDisplay_stub
    SceGxm_stub
    SceSysmodule_stub
    SceCtrl_stub
    ScePgf_stub
    ScePvf_stub
    SceCommonDialog_stub
    SceAppMgr_stub
    freetype
    png
    jpeg
    z
    m
)

vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME})

vita_create_vpk(${PROJECT_NAME}.vpk ${VITA_TITLEID} ${PROJECT_NAME}.self
    VERSION ${VITA_VERSION}
    NAME ${VITA_APP_NAME}
)
