cmake_minimum_required(VERSION 3.15)
# SDL2 - window/event/audio backend of the OpenGL flavor (ENABLE_SDL)
# upstream: build/sdl.mk (pkg-config sdl2), here built from source as a
# static library, like the upstream Windows cross build
# SDL generates SDL_config.h at build time and installs it with the
# headers - and that file is COMPILER-specific (a MinGW build sets
# HAVE_STRINGS_H, MSVC has no strings.h -> C1083).  A shared include
# dir lets the last-built toolchain poison all others, so SDL2 headers
# go to include/<toolchain>
set(INCLUDE_WITH_TOOLCHAIN 1)

# CMake install names: SDL2-static.lib / SDL2-staticd.lib
prepare_3rdparty(sdl2 SDL2-static SDL2-staticd)

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_BINDIR=${_INSTALL_BIN_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${_INSTALL_INC_DIR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DSDL_SHARED=OFF"
        "-DSDL_STATIC=ON"
        "-DSDL_TEST=OFF"
        "-DSDL_TESTS=OFF"
        "-DSDL2_DISABLE_INSTALL_DOCS=ON"
        # we bring our own GLES via ANGLE - no need for the OpenGL loader
        "-DSDL_OPENGLES=ON"
        "-DSDL_OPENGL=ON"
    )
    if (MSVC)
        # match XCSoar's runtime (/MDd, /MD)
        list(APPEND CMAKE_ARGS "-DSDL_FORCE_STATIC_VCRT=OFF")
    endif()

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "release-${SDL2_VERSION}"

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
# SDL2 installs its headers to include/SDL2
set(SDL2_INCLUDE_DIR ${SDL2_INCLUDE_DIR}/SDL2 PARENT_SCOPE)
list(APPEND THIRDPARTY_INCLUDES ${SDL2_INCLUDE_DIR}/SDL2)
# static SDL2 on Windows needs these system libraries
if (WIN32)
    set(SDL2_SYSTEM_LIBS winmm imm32 version setupapi ole32 oleaut32
        shell32 advapi32 user32 gdi32 PARENT_SCOPE)
endif()
message(STATUS "---- SDL2-Target = ${SDL2_TARGET}")
