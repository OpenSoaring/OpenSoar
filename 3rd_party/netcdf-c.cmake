cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME netcdf)

prepare_3rdparty(netcdf_c ${_LIB_NAME})
string(APPEND NETCDF_C_CMAKE_DIR  /netCDF)

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${NETCDF_C_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${NETCDF_C_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${NETCDF_C_INCLUDE_DIR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  
        "-DBUILD_UTILITIES:BOOL=OFF"
        "-DWITH_UTILITIES:BOOL=OFF"

        "-DBUILD_SHARED_LIBS:BOOL=OFF"

        # Tests:
        "-DENABLE_TESTS:BOOL=OFF"
        "-DBUILD_TESTING:BOOL=OFF"
        "-DBUILD_TESTSETS:BOOL=OFF"
        "-DENABLE_FILTER_TESTING:BOOL=OFF"

        "-DENABLE_EXAMPLES:BOOL=OFF" # see libs.py
        "-DENABLE_MMAP:BOOL=OFF" # see libs.py
        "-DENABLE_DAP:BOOL=OFF" # see libs.py

        "-DCURL_DIR:PATH=${CURL_CMAKE_DIR}"
        # 2024.11.26: "-DCURL_INCLUDE_DIR:PATH=${CURL_INCLUDE_DIR}"
        # 2024.11.26:"-DCURL_LIBRARY:FILEPATH=${CURL_LIBRARY}"
        # "-DCURL_LIBRARY_RELEASE=${CURL_DIR}/lib/${TOOLCHAIN}/curl.lib"
        # "-DZLIB_LIBRARY:FILEPATH=\"debug;${ZLIB_LIBRARY_DEBUG};optimized;${ZLIB_LIBRARY_RELEASED}\""
        # "-DZLIB_LIBRARY:FILEPATH=${ZLIB_LIBRARY_DEBUG}"
        "-DZLIB_LIBRARY:FILEPATH=${ZLIB_LIBRARY}"
        "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}"

        "-DNC_FIND_SHARED_LIBS:BOOL=OFF"
        # funktionierte so bei geotiff - hier auch???
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        "-DENABLE_SHARED_LIBRARY_VERSION:BOOL=OFF"

        "-DENABLE_LARGE_FILE_SUPPORT:BOOL=OFF"  # see libs.py
        ### 2024.11.26: "-DENABLE_EXTREME_NUMBERS:BOOL=OFF"
        "-DENABLE_DAP_REMOTE_TESTS:BOOL=OFF"
        "-DENABLE_BASH_SCRIPT_TESTING:BOOL=OFF"
        ### 2024.11.26: "-DENABLE_:BOOL=OFF"

        "-DHAVE_GETRLIMIT:BOOL=OFF"
        "-DHAVE_MKSTEMP:BOOL=OFF"
    )

    if(HAVE_HDF5)  # defined in 3rd_party/CMakeSource.cmake
      list(APPEND CMAKE_ARGS

        "-DUSE_HDF5:BOOL=ON"
        "-DENABLE_NETCDF_4:BOOL=ON"

#### w/o or with NETCDF4:
        "-DSZIP:FILEPATH=${SZIP_LIBRARY}"
        # "-DHDF5_DIR=${HDF5_DIR}"
   # 22.12.2025     "-DHAVE_HDF5_H:PATH=${HDF5_INCLUDE_DIR}"
        "-DUSE_HDF5:BOOL=ON"
        # "-DHDF5_DIR:PATH=${HDF5_DIR}"
        # "-DHDF5_CMAKE_DIR:PATH=${HDF5_CMAKE_DIR}"
        "-DHDF5_DIR:PATH=${HDF5_CMAKE_DIR}"
        # "-DHDF5_BUILD_DIR:PATH=${HDF5_DIR}"
        # "-DHDF5_DIR:PATH=${HDF5_DIR}"
        "-DHDF5_PACKAGE_NAME=hdf5"
        "-DHDF5_LIBRARIES:PATH=${HDF5_LIB_DIR}"
        "-DHDF5_INCLUDE_DIRS:PATH=${HDF5_INCLUDE_DIR}"
        "-DHDF5_HL_LIBRARIES:PATH=${HDF5_LIB_DIR}"

        "-DHDF5_hdf5_LIBRARY:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_LIBRARY_DEBUG:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_LIBRARY_RELEASE:FILEPATH=${HDF5_LIBRARY}"
        "-DHDF5_hdf5_hl_LIBRARY:FILEPATH=${HDF5_HL_LIBRARY}"
        "-DHDF5_hdf5_hl_LIBRARY_DEBUG:FILEPATH=${HDF5_HL_LIBRARY}"
        "-DHDF5_hdf5_hl_LIBRARY_RELEASE:FILEPATH=${HDF5_HL_LIBRARY}"
        "-DHDF5_PARALLEL:BOOL=OFF"
        "-DHDF5_IS_PARALLEL:BOOL=OFF"

        # with this 3 ARGS no FIND_PACKAGE necessary (with MSVC not needed ;-) )
        "-DHDF5_LIBRARY:PATH=${HDF5_LIBRARY}"
        "-DHDF5_HL_LIBRARY:PATH=${HDF5_HL_LIBRARY}"
        "-DHDF5_INCLUDE_DIR:PATH=${HDF5_INCLUDE_DIR}"

        "-DUSE_PARALLEL:BOOL=OFF"
        "-DUSE_PARALLEL4:BOOL=OFF"
        "-DENABLE_PARALLEL4:BOOL=OFF"
        "-DENABLE_PARALLEL_TESTS:BOOL=OFF"
    )
    else (HAVE_HDF5)
      list(APPEND CMAKE_ARGS
        "-DUSE_HDF5:BOOL=OFF" # see libs.py
        "-DENABLE_NETCDF_4:BOOL=OFF" # see libs.py
      )

    endif (HAVE_HDF5)
    list(APPEND CMAKE_ARGS
        "-DnetCDF_DIR:PATH=${NETCDF_CMAKE_DIR}"
    )

    #wrong patch path name:
    # set(_PATCH_DIR ${PROJECTGROUP_SOURCE_DIR}/lib/netcdf/patches)
    set(_PATCH_DIR ${PROJECTGROUP_SOURCE_DIR}/lib/netcdf)
    # set(_PATCH_COMMAND "${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/LIBNETCDF_C/CMakeLists.txt.in <SOURCE_DIR>/CMakeLists.txt")
    if (${${TARGET_CNAME}_VERSION} VERSION_EQUAL "4.7.4")
      set(_PATCH_COMMAND "${PYTHON_APP} ${_PATCH_DIR}/patch.py")  ## fix_dutil_4_7_4.patch")
      string(REPLACE "/" "\\" _PATCH_COMMAND "${_PATCH_COMMAND}")
      string(APPEND _PATCH_COMMAND " ${${TARGET_CNAME}_PREFIX}/src/netcdf_c_build ${_PATCH_DIR}/patches/fix_dutil_4_7_4.patch")  ## fix_dutil_4_7_4.patch")
      
      # string(REPLACE "\"" "" _PATCH_COMMAND "${_PATCH_COMMAND}")
      set(_PATCH_TEST ${_PATCH_COMMAND}) # TEST!!!!
      set(_PATCH_COMMAND echo Patch: '${_PATCH_TEST}') # TEST!!!!
      string(APPEND _PATCH_COMMAND " && ${_PATCH_TEST}")  ## fix_dutil_4_7_4.patch")
      
      # set(_PATCH_COMMAND "${PYTHON_APP} ${_PATCH_DIR}/patch.py D:/Libs/netcdf_c/netcdf_c-4.7.4/src/netcdf_c_build ${_PATCH_DIR}/patches/fix_dutil_4_7_4.patch")  ## fix_dutil_4_7_4.patch")
      # set(_PATCH_COMMAND "git apply ${_PATCH_DIR}/fix_dutil_4_7_4.patch")
      message(STATUS "### netcdf-Patch:  '${_PATCH_COMMAND}' ")
      message(STATUS "### netcdf-Source:  '${${TARGET_CNAME}_PREFIX}/src/netcdf_c_build' ")

      # message(FATAL_ERROR "Stop! ++++++++++++++++++++++++++")
    else()
       set(_PATCH_COMMAND )
       message(FATAL_ERROR "### netcdf: No_PATCH!!!: ${${TARGET_CNAME}_VERSION} ")
    endif()

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/Unidata/netcdf-c.git"
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"           # git tag by libnetcdf_c!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        PATCH_COMMAND ${_PATCH_COMMAND}
        CMAKE_ARGS ${CMAKE_ARGS}
        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET} ${CURL_TARGET} ${PROJ_TARGET} # ${HDF5_TARGET} 

        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()

# ???  target_link_libraries(${_BUILD_TARGET} PUBLIC ${HDF5_LIBRARY} ${HDF5_HL_LIBRARY})
