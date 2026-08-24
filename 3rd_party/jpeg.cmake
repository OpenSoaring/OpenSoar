cmake_minimum_required(VERSION 3.15)
# libjpeg-turbo - JPEG decoding for the SDL/OpenGL flavor
# (upstream build/sdl.mk: LIBJPEG=y; ui/canvas/custom/Bitmap.cpp calls
# LoadJPEG unconditionally); built static, no TurboJPEG API
set(INCLUDE_WITH_TOOLCHAIN 0)

# libjpeg-turbo installs jpeg-static.lib (MSVC) / libjpeg.a (MinGW),
# without a debug postfix
if (MSVC)
  prepare_3rdparty(jpeg jpeg-static jpeg-static)
else()
  prepare_3rdparty(jpeg jpeg jpeg)
endif()

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_BINDIR=${_INSTALL_BIN_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=include"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DENABLE_SHARED=OFF"
        "-DENABLE_STATIC=ON"
        "-DWITH_TURBOJPEG=OFF"
        "-DWITH_JPEG8=ON"
        "-DWITH_SIMD=OFF"     # no nasm/yasm dependency
    )
    if (MSVC)
        # libjpeg-turbo defaults to the *static* CRT (/MT) on MSVC, which
        # conflicts with everything else (/MD, LNK4098 LIBCMTD)
        list(APPEND CMAKE_ARGS "-DWITH_CRT_DLL=ON")
    endif()

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/libjpeg-turbo/libjpeg-turbo.git"
        GIT_TAG "${JPEG_VERSION}"

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
message(STATUS "---- JPEG-Target = ${JPEG_TARGET}")
