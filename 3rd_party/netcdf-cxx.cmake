cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

if (MSVC)  # unfortunately the lib name is a little bit 'tricky'..
  set(_LIB_NAME netcdf-cxx4)
 else()
  set(_LIB_NAME netcdf_cxx)
endif()

prepare_3rdparty(netcdf_cxx ${_LIB_NAME})
string(APPEND NETCDF_CXX_CMAKE_DIR  /netCDFCxx)

if (ON)  # _COMPLETE_INSTALL)

    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${NETCDF_CXX_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${NETCDF_CXX_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${NETCDF_CXX_INCLUDE_DIR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  
        "-DBUILD_SHARED_LIBS=OFF"
        "-DHDF5_DIR=${HDF5_CMAKE_DIR}"
        "-DnetCDF_DIR=${NETCDF_C_CMAKE_DIR}"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/Unidata/netcdf-cxx4.git"
        # git tag by libnetcdf_cxx, f.e. TAG='v4.3.1'!
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBNETCDF_CXX/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        CMAKE_ARGS ${CMAKE_ARGS}
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${HDF5_TARGET}  ${NETCDF_C_TARGET}
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
