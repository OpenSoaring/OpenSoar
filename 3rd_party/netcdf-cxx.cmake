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
        ## ?? "-DNETCDF_INCLUDE_DIR:PATH=${NETCDF_C_INCLUDE_DIR}"
        ## ?? 
        "-DNC_H_INCLUDE_DIR:PATH=${NETCDF_C_INCLUDE_DIR}"   ## "/netcdf.h"
        "-DnetCDF_DIR=${NETCDF_C_CMAKE_DIR}"
        ## ??  "-DDLL_NETCDF:BOOL=OFF"
    )

    if  (HAVE_HDF5)
      list(APPEND CMAKE_ARGS
        "-DHDF5_DIR:PATH=${HDF5_CMAKE_DIR}"
        # "-DHDF5_DIR:FILEPATH=${HDF5_LIBRARY}" # "${HDF5_LIB_DIR}/libhdf5.a"
        "-DUSE_HDF5:BOOL=ON"  # August2111: special flag because wrong usage of HDF5

        "-DHDF5_C_INCLUDE_DIR:PATH=${HDF5_INCLUDE_DIR}"
        "-DHDF5_INCLUDE_DIRS:PATH=${HDF5_INCLUDE_DIR}"

        "-DHDF5_C_LIBRARY_hdf5:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_LIBRARY:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_LIBRARY_DEBUG:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_LIBRARY_RELEASE:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_hl_LIBRARY:FILEPATH=${HDF5_HL_LIBRARY}"
        "-DHDF5_hdf5_hl_LIBRARY_DEBUG:FILEPATH=${HDF5_HL_LIBRARY}"
        "-DHDF5_hdf5_hl_LIBRARY_RELEASE:FILEPATH=${HDF5_HL_LIBRARY}"
      )
    else (HAVE_HDF5)
      list(APPEND CMAKE_ARGS
        "-DHDF5_DIR:PATH=" 
        "-DUSE_HDF5:BOOL=OFF"  # August2111: special flag because wrong usage of HDF5
      )
    endif (HAVE_HDF5)


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



