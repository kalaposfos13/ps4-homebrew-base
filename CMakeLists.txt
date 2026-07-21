cmake_minimum_required(VERSION 3.24)

project(MICE LANGUAGES C CXX)

# =========================================================
# USER CONFIG
# =========================================================

set(APP_TITLE "camera test")
set(APP_VERSION "0.01")
set(APP_TITLE_ID "KALA00014")
set(SDK_VERSION 0x02000001)

set(LIBS
    c c++ kernel
    SceSysUtil SceSystemService SceUserService ScePad
    SceVideoOut SceGnmDriver SceSysmodule SceFreeType
    SceCamera ScePadTracker
)

# =========================================================
# TOOLCHAIN
# =========================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)

set(OO_PS4_TOOLCHAIN $ENV{OO_PS4_TOOLCHAIN})

if(NOT OO_PS4_TOOLCHAIN)
    message(FATAL_ERROR "OO_PS4_TOOLCHAIN not set")
endif()

# =========================================================
# GLOBALS
# =========================================================

set(ROOT   ${CMAKE_SOURCE_DIR})
set(BUILD  ${CMAKE_BINARY_DIR})

set(EBUILD_ELF ${BUILD}/eboot.elf)
set(EBUILD_OELF ${BUILD}/eboot.oelf)
set(EBUILD_BIN ${BUILD}/eboot.bin)

set(SFO_BUILD ${BUILD}/sce_sys/param.sfo)
set(SFO_ROOT  ${ROOT}/sce_sys/param.sfo)

set(GP4_FILE  ${BUILD}/pkg.gp4)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    ${ROOT}/src/*.cpp
    ${ROOT}/src/*.c
)

file(GLOB_RECURSE ASSETS CONFIGURE_DEPENDS
    ${ROOT}/.is-app0-root
    ${ROOT}/assets/*
)

# =========================================================
# EXECUTABLE
# =========================================================

add_executable(eboot.elf ${SOURCES})

target_include_directories(eboot.elf PRIVATE
    ${ROOT}/src
    ${ROOT}/src/common
    ${OO_PS4_TOOLCHAIN}/include
    ${OO_PS4_TOOLCHAIN}/include/c++/v1
)

target_compile_options(eboot.elf PRIVATE
    -fPIC
    -fexceptions
    -funwind-tables
)

target_compile_options(eboot.elf PRIVATE
    -fmacro-prefix-map=${CMAKE_SOURCE_DIR}/src/=
    -fdebug-prefix-map=${CMAKE_SOURCE_DIR}/src/=
)

target_link_options(eboot.elf PRIVATE
    ${OO_PS4_TOOLCHAIN}/lib/crt1.o
)

target_link_directories(eboot.elf PRIVATE
    ${OO_PS4_TOOLCHAIN}/lib
)

target_link_libraries(eboot.elf PRIVATE
    ${LIBS}
)

# =========================================================
# EBOOT GENERATION
# =========================================================

add_custom_command(TARGET eboot.elf POST_BUILD
    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/create-fself
        -in ${EBUILD_ELF}
        -out ${EBUILD_OELF}
        --eboot ${EBUILD_BIN}
        --paid 0x3800000000000011
        --sdkver ${SDK_VERSION}

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${EBUILD_BIN}
        ${ROOT}/eboot.bin

    COMMENT "Generating + copying eboot.bin"
)

# =========================================================
# SFO GENERATION
# =========================================================

add_custom_target(sfo ALL DEPENDS ${SFO_BUILD})
set(CONTENT_ID "IV0000-${APP_TITLE_ID}_00-HOMEBREW00000000")

add_custom_command(
    OUTPUT ${SFO_BUILD}

    COMMAND ${CMAKE_COMMAND} -E make_directory ${BUILD}/sce_sys

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_new ${SFO_BUILD}

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        APP_TYPE --type Integer --maxsize 4 --value 1 > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        ATTRIBUTE --type Integer --maxsize 4 --value 0 > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        APP_VER --type Utf8 --maxsize 8 --value ${APP_VERSION} > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        VERSION --type Utf8 --maxsize 8 --value ${APP_VERSION} > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        SYSTEM_VER --type Integer --maxsize 4 --value 0x1000000 > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        CATEGORY --type Utf8 --maxsize 4 --value "gd" > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0 > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        TITLE_ID --type Utf8 --maxsize 12 --value ${APP_TITLE_ID} > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        CONTENT_ID --type Utf8 --maxsize 48 --value ${CONTENT_ID} > /dev/null

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core sfo_setentry ${SFO_BUILD}
        TITLE --type Utf8 --maxsize 128 --value ${APP_TITLE} > /dev/null

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SFO_BUILD}
        ${SFO_ROOT}
    
    DEPENDS ${CMAKE_SOURCE_DIR}/CMakeLists.txt
    COMMENT "Generating param.sfo"

    VERBATIM
)

# =========================================================
# GP4 FILE LIST (AUTO + MANUAL HYBRID)
# =========================================================

set(GP4_FILES
    eboot.bin
    sce_sys/param.sfo
    sce_sys/about/right.sprx
    sce_sys/icon0.png
    sce_module/libc.prx
    sce_module/libSceFios2.prx
)

# Append globbed assets safely (relative conversion)
foreach(a IN LISTS ASSETS)
    file(RELATIVE_PATH rel ${ROOT} ${a})
    list(APPEND GP4_FILES ${rel})
endforeach()

# =========================================================
# GP4 GENERATION
# =========================================================

add_custom_target(gp4
    DEPENDS eboot.elf sfo

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/create-gp4
        -out ${GP4_FILE}
        --content-id ${CONTENT_ID}
        --files \"${GP4_FILES}\" # "

    WORKING_DIRECTORY ${ROOT}

    COMMENT "Generating GP4"
)

add_custom_command(TARGET gp4 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${GP4_FILE}
        ${ROOT}/pkg.gp4
)

# =========================================================
# PKG BUILD (separate)
# =========================================================

add_custom_target(pkg
    DEPENDS gp4

    COMMAND ${OO_PS4_TOOLCHAIN}/bin/linux/PkgTool.Core
        pkg_build
        ${ROOT}/pkg.gp4
        ${BUILD}

    WORKING_DIRECTORY ${BUILD}

    COMMENT "Building PKG"
)