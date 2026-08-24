cmake_minimum_required(VERSION 3.15)
# FreeType - font rendering for the OpenGL/SDL flavor (USE_FREETYPE)
# upstream: build/freetype.mk (pkg-config freetype2), here built from source
set(INCLUDE_WITH_TOOLCHAIN 0)

# CMake install name: freetype.lib (Release), freetyped.lib (Debug/MSVC)
prepare_3rdparty(freetype freetype freetyped)

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_BINDIR=${_INSTALL_BIN_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=include"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DBUILD_SHARED_LIBS=OFF"
        # XCSoar only renders TrueType fonts: no optional deps at all.
        # FT_DISABLE_ZLIB=ON means 'use FreeType's *internal* zlib copy'
        # (the system-zlib path pulls our link_libs zconf.h into MSVC,
        # which then wants <unistd.h>); PNG (color bitmap glyphs) is
        # not needed either
        "-DFT_DISABLE_ZLIB=ON"
        "-DFT_DISABLE_PNG=ON"
        "-DFT_DISABLE_BZIP2=ON"
        "-DFT_DISABLE_HARFBUZZ=ON"
        "-DFT_DISABLE_BROTLI=ON"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/freetype/freetype.git"
        GIT_TAG "VER-${FREETYPE_VERSION_TAG}"

        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
        CMAKE_ARGS ${CMAKE_ARGS}

        INSTALL_COMMAND ${_INSTALL_COMMAND}
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        CONFIGURE_HANDLED_BY_BUILD  ON
        BUILD_BYPRODUCTS  ${_TARGET_BYPRODUCTS}
    )
endif()

post_3rdparty()
# freetype installs its headers to include/freetype2
set(FREETYPE_INCLUDE_DIR ${FREETYPE_INCLUDE_DIR}/freetype2 PARENT_SCOPE)
list(APPEND THIRDPARTY_INCLUDES ${FREETYPE_INCLUDE_DIR}/freetype2)
message(STATUS "---- FREETYPE-Target = ${FREETYPE_TARGET}")
