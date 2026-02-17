cmake_minimum_required(VERSION 3.15)

# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

 set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
if (MSVC AND CLANG)  # unfortunately the lib name is a little bit 'tricky' at libPng..
  # at MSVC the LIB_PREFIX is empty normally
  set(_LIB_NAME libpng16_static)
 else()
  set(_LIB_NAME png16)
endif()

if (MINGW)
  prepare_3rdparty(png png16)  # png16d)
else()  # MSVC and CLANG
  prepare_3rdparty(png libpng16_static libpng16_staticd)
endif()

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
             "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
            "-DCMAKE_INSTALL_INCLUDEDIR=include"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  
            "-DPNG_ZLIB_BUILD=OFF"
            "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}"
            "-DZLIB_LIBRARY=${ZLIB_LIBRARY}"   # ZLIB_LIB is 'optimized;ReleaseLib;debug;DebugLib'--

            "-DPNG_SHARED=OFF"
            "-DPNG_STATIC=ON"
            "-DPNG_TESTS=OFF"
    )
    if(${TARGET_CNAME}_VERSION VERSION_LESS 1.6.43)
        list(APPEND CMAKE_ARGS "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
    endif()

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/glennrp/libpng.git"
        # GIT_REPOSITORY "https://github.com/libpng/libpng.git" # alternative?
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"           # git tag by libpng!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        PATCH_COMMAND ${PYTHON_APP} ${_PATCH_BASE}/cmake_patch.py png
        CMAKE_ARGS ${CMAKE_ARGS}

        INSTALL_COMMAND ${_INSTALL_COMMAND}

        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
