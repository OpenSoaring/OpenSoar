cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME netcdf)

prepare_3rdparty(netcdf_c ${_LIB_NAME})
string(APPEND NETCDF_C_CMAKE_DIR  /netCDF)

# 2024.11.26:   # set(HDF5_CMAKE_DIR  ${HDF5_DIR}/${HDF5_CMAKE_DIR}) # WRONG!!! ??????
# 2024.11.26:   # set(HDF5_CMAKE_DIR  ${HDF5_LIB_DIR}/cmake) # WRONG!!! ??????
# 2024.11.26:   #  set(HDF5_CMAKE_DIR  lib/${TOOLCHAIN}d/cmake)
# 2024.11.26:   message (STATUS "xxxx Test: ${HDF5_CMAKE_DIR}")
# 2024.11.26:   # string(REPLACE "${HDF5_DIR}//" "./" HDF5_CMAKE_DIR ${HDF5_CMAKE_DIR} )
# 2024.11.26:   message (STATUS "xxxx Test: ${HDF5_DIR}")
# 2024.11.26:   message (STATUS "xxxx Test: ${HDF5_CMAKE_DIR}")
# 2024.11.26:   message (STATUS "xxxx Test: ${HDF5_LIB_DIR}")
# 2024.11.26:   message (STATUS "xxxx Test: ${HDF5_INCLUDE_DIR}")
# 2024.11.26:   ## message (FATAL_ERROR "xxxx STOP!!!") 

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

    if(HAVE_HDF5)
      list(APPEND CMAKE_ARGS

        "-DUSE_HDF5:BOOL=ON"
        "-DENABLE_NETCDF_4:BOOL=ON"

#### w/o or with NETCDF4:
        "-DSZIP:FILEPATH=${SZIP_LIBRARY}"
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
        DEPENDS ${ZLIB_TARGET} ${CURL_TARGET} ${PROJ_TARGET} ${HDF5_TARGET} 

        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()

# ???  target_link_libraries(${_BUILD_TARGET} PUBLIC ${HDF5_LIBRARY} ${HDF5_HL_LIBRARY})
