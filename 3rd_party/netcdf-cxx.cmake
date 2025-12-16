cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

if (MSVC)  # unfortunately the lib name is a little bit 'tricky'..
  set(_LIB_NAME netcdf-cxx4)
 else()
  set(_LIB_NAME netcdf-cxx4)
  # set(_LIB_NAME netcdf_cxx)
endif()

prepare_3rdparty(netcdf_cxx ${_LIB_NAME})
string(APPEND NETCDF_CXX_CMAKE_DIR  /netCDFCxx)

set(HAVE_HDF5 OFF)

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX:PATH=${NETCDF_CXX_DIR}"
        "-DCMAKE_INSTALL_LIBDIR:PATH=${NETCDF_CXX_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR:PATH=${NETCDF_CXX_INCLUDE_DIR}"
        "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
  
        "-DBUILD_TESTING:BOOL=OFF"
        "-DNCXX_ENABLE_TESTS:BOOL=OFF"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"
        "-DNETCDF_C_LIBRARY:FILEPATH=${NETCDF_C_LIBRARY}"
        "-DnetCDF_DIR=${NETCDF_C_CMAKE_DIR}"
        ## ??  "-DDLL_NETCDF:BOOL=OFF"
    )
    
    # message(FATAL_ERROR "### NETCDF_C_CMAKE_DIR  = ${NETCDF_C_CMAKE_DIR} ")
    # set(NETCDF_C_CMAKE_DIR ${LINK_LIBS}/netcdf_c/netcdf_c-4.9.3/lib/msvc2026/cmake/netCDF)
    
    list(APPEND CMAKE_ARGS
        "-DHDF5_DIR:PATH=" 
        "-DUSE_HDF5:BOOL=OFF"  # August2111: special flag because wrong usage of HDF5
    )

    list(APPEND CMAKE_ARGS
        "-DHDF5_DIR:PATH=" 
        "-DUSE_HDF5:BOOL=OFF"
    )
    if (${${TARGET_CNAME}_VERSION} STREQUAL "main")
      set(_GIT_TAG main)  # is not the version tag
      list(APPEND CMAKE_ARGS
        "-DNETCDF_C_INCLUDE_DIR:PATH=${NETCDF_C_INCLUDE_DIR}"   ## "/netcdf.h"
      )
    else()
      set(_GIT_TAG v${${TARGET_CNAME}_VERSION})
      if(${TARGET_CNAME}_VERSION VERSION_LESS 4.4)
        list(APPEND CMAKE_ARGS
          ### "-DNC_H_INCLUDE_DIR:PATH=${NETCDF_C_INCLUDE_DIR}"   ## "/netcdf.h"
          "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
        )
      else()
        list(APPEND CMAKE_ARGS
          "-DNETCDF_C_INCLUDE_DIR:PATH=${NETCDF_C_INCLUDE_DIR}"   ## "/netcdf.h"
        )
      endif()
    endif()


    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/Unidata/netcdf-cxx4.git"
        # git tag by libnetcdf_cxx, f.e. TAG='v4.3.1'!
        # GIT_TAG "v${${TARGET_CNAME}_VERSION}"
        GIT_TAG  ${_GIT_TAG}
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBNETCDF_CXX/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        # PATCH_COMMAND ${CMAKE_COMMAND} -E rm  -f "CMakeCache.txt" && dir
        PATCH_COMMAND ${CMAKE_COMMAND} -E echo  "Display Path: "  ||| '${${TARGET_CNAME}_BUILD_DIR}' && 
            ${CMAKE_COMMAND} -E rm  -f "${${TARGET_CNAME}_BUILD_DIR}/CMakeCache.txt" && ${CMAKE_COMMAND} -E echo  XXXXXXX
        
        CMAKE_ARGS ${CMAKE_ARGS}
        INSTALL_COMMAND ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        # DEPENDS ${HDF5_TARGET}  ${NETCDF_C_TARGET}
        DEPENDS ${NETCDF_C_TARGET}  # ${HDF5_TARGET}  
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()

if (_COMPLETE_INSTALL)
    add_dependencies(${_BUILD_TARGET}  ${NETCDF_C_TARGET})
# else()
    # set_target_properties(${_BUILD_TARGET} PROPERTIES FOLDER 3rdParty_2)
endif()



