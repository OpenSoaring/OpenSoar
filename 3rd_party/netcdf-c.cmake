cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME netcdf)

prepare_3rdparty(netcdf_c ${_LIB_NAME})
string(APPEND NETCDF_C_CMAKE_DIR  /netCDF)

#  set(HDF5_CMAKE_DIR  ${HDF5_DIR}/cmake) # WRONG!!! ??????
#  set(HDF5_CMAKE_DIR  lib/${TOOLCHAIN}d/cmake)
message (STATUS "xxxx Test: ${HDF5_CMAKE_DIR}")
# string(REPLACE "${HDF5_DIR}//" "./" HDF5_CMAKE_DIR ${HDF5_CMAKE_DIR} )
message (STATUS "xxxx Test: ${HDF5_DIR}")
message (STATUS "xxxx Test: ${HDF5_CMAKE_DIR}")
message (STATUS "xxxx Test: ${HDF5_LIB_DIR}")
message (STATUS "xxxx Test: ${HDF5_INCLUDE_DIR}")
## message (FATAL_ERROR "xxxx STOP!!!")

if (_COMPLETE_INSTALL)

    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX=${NETCDF_C_DIR}"
             "-DCMAKE_INSTALL_LIBDIR=${NETCDF_C_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${NETCDF_C_INCLUDE_DIR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  
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

        "-DCURL_DIR:PATH=${CURL_CMAKE_DIR}"
        "-DCURL_INCLUDE_DIR:PATH=${CURL_INCLUDE_DIR}"
        "-DCURL_LIBRARY:FILEPATH=${CURL_LIBRARY}"
        # "-DCURL_LIBRARY_RELEASE=${CURL_DIR}/lib/${TOOLCHAIN}/curl.lib"
        "-DZLIB_LIBRARY:FILEPATH=${ZLIB_LIBRARY}"
        "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}"

        "-DNC_FIND_SHARED_LIBS=OFF"

        # "-DHDF5_DIR=${HDF5_DIR}"

        "-DHAVE_HDF5_H:PATH=${HDF5_INCLUDE_DIR}"
        # "-DHDF5_DIR:PATH=${HDF5_DIR}"
        # "-DHDF5_CMAKE_DIR:PATH=${HDF5_CMAKE_DIR}"
        "-DHDF5_DIR:PATH=${HDF5_CMAKE_DIR}"
        # "-DHDF5_BUILD_DIR:PATH=${HDF5_DIR}"
        # "-DHDF5_DIR:PATH=${HDF5_DIR}"
        "-DHDF5_PACKAGE_NAME=hdf5"
        "-DHDF5_LIBRARIES:PATH=${HDF5_LIB_DIR}"
        "-DHDF5_INCLUDE_DIRS:PATH=${HDF5_INCLUDE_DIR}"
        "-DHDF5_HL_LIBRARIES:PATH=${HDF5_LIB_DIR}"

        # funktionierte so bei ggeotiff - hier auch???
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
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
        DEPENDS ${ZLIB_TARGET} ${CURL_TARGET} ${HDF5_TARGET} ${PROJ_TARGET}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
