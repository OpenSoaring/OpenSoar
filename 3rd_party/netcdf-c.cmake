cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME netcdf)
## if (MSVC)  # unfortunately the lib name is a little bit 'tricky' at libPng..
##   set(_LIB_NAME libnetcdf_c)
##   # message(FATAL_ERROR LibPng: MSVC')
##  else()
##   set(_LIB_NAME netcdf_c)
## endif()

set (HDF5_DIR ${LINK_LIBS}/hdf5/hdf5-${HDF5_VERSION})
# set (CURL_DIR ${LINK_LIBS}/curl/curl-${CURL_VERSION})
# set (ZLIB_DIR ${LINK_LIBS}/zlib/zlib-${ZLIB_VERSION})
prepare_3rdparty(netcdf_c ${_LIB_NAME})
if (_COMPLETE_INSTALL)

    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
             "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB}"
            "-DCMAKE_INSTALL_INCLUDEDIR=include"
            "-DCMAKE_BUILD_TYPE=Release"
  
        "-DWITH_UTILITIES=OFF"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_TESTING=OFF"
        "-DBUILD_TESTSETS=OFF"
        "-DBUILD_UTILITIES=OFF"
        "-DENABLE_FILTER_TESTING=OFF"

        "-DENABLE_EXAMPLES=OFF"
        "-DENABLE_MMAP=OFF"
        "-DENABLE_DAP=OFF"
        "-DENABLE_TESTS=OFF"

        # "-DHDF5_DIR=${HDF5_INSTALL_DIR}/cmake"
        "-DHDF5_DIR=${HDF5_DIR}/cmake"
        # "-DHAVE_HDF5_H=${HDF5_INSTALL_DIR}/include"
        "-DHAVE_HDF5_H=${HDF5_DIR}/include"
        "-DCURL_DIR=${CURL_INSTALL_DIR}/lib/${TOOLCHAIN}/cmake/CURL"
        "-DCURL_INCLUDE_DIR=${CURL_INSTALL_DIR}/include"
        "-DCURL_LIBRARY_DEBUG=${CURL_DIR}/lib/${TOOLCHAIN}/curl.lib"
        "-DCURL_LIBRARY_RELEASE=${CURL_DIR}/lib/${TOOLCHAIN}/curl.lib"
        "-DZLIB_INCLUDE_DIR=${ZLIB_DIR}/include"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/Unidata/netcdf-c.git"
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"           # git tag by libnetcdf_c!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBNETCDF_C/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        CMAKE_ARGS ${CMAKE_ARGS}
        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET} ${CURL_TARGET} ${HDF5_TARGET}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
