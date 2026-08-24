cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
  set(_LIB_NAME zs) # since 1.3.2
else()
  set(_LIB_NAME z)
endif()

# prepare_3rdparty(zlib ${_LIB_NAME} ${_LIB_NAME}d)
prepare_3rdparty(zlib ${_LIB_NAME} ${_LIB_NAME}d)

if (_COMPLETE_INSTALL)
    #----------------------
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_LIBDIR:PATH=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_BINDIR:PATH=${_INSTALL_BIN_DIR}"
        # ??? "-DCMAKE_CONFIGURATION_TYPES=Release"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"

        # no <unistd.h> on MSVC. zlib <= 1.3.1 checks Z_HAVE_UNISTD_H,
        # zlib >= 1.3.2 checks HAVE_UNISTD_H (and #defines it in the
        # generated zconf.h, where it then pulls unistd.h into every user)
        "-DZ_HAVE_UNISTD_H=OFF"
        "-DHAVE_UNISTD_H=OFF"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/madler/zlib.git"
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"

        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
        PATCH_COMMAND ${PYTHON_APP} ${_PATCH_DIR}/cmake_patch.py zlib
        CMAKE_ARGS ${CMAKE_ARGS}
 
        INSTALL_COMMAND ${_INSTALL_COMMAND}
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        CONFIGURE_HANDLED_BY_BUILD  ON
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        BUILD_BYPRODUCTS  ${_TARGET_BYPRODUCTS} # important for ninja!
    )
endif()

post_3rdparty()
  message(STATUS "---- ZLIB-Target(ZLIB_TARGET) = ${ZLIB_TARGET}") 

# guard: an already installed zconf.h that was generated with unistd.h
# detected breaks every zlib user on MSVC (C1083 unistd.h from zconf.h)
if(MSVC AND EXISTS ${ZLIB_INCLUDE_DIR}/zconf.h)
  file(STRINGS ${ZLIB_INCLUDE_DIR}/zconf.h _zconf_unistd
       REGEX "^#define (Z_)?HAVE_UNISTD_H")
  if(_zconf_unistd)
    message(FATAL_ERROR "zlib: '${ZLIB_INCLUDE_DIR}/zconf.h' defines "
      "HAVE_UNISTD_H (${_zconf_unistd}) - this zconf.h was generated with "
      "unistd.h detected and cannot be used with MSVC. Delete "
      "'${ZLIB_INSTALL_DIR}' (and the zlib build dir under THIRD_PARTY) so "
      "zlib is rebuilt with -DHAVE_UNISTD_H=OFF.")
  endif()
endif()
