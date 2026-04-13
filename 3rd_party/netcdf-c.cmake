cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

# set(CMAKE_BUILD_TYPE Debug) # why ???

set(_LIB_NAME netcdf)

prepare_3rdparty(netcdf_c ${_LIB_NAME})
string(APPEND NETCDF_C_CMAKE_DIR  /netCDF)

if (_COMPLETE_INSTALL)
    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${NETCDF_C_DIR}"
        ## "-DCMAKE_INSTALL_LIBDIR=lib/${TOOLCHAIN}(d)" # ${NETCDF_C_LIB_DIR}"
        "-DCMAKE_INSTALL_LIBDIR:PATH=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=include"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"

        "-DBUILD_SHARED_LIBS:BOOL=OFF"
        "-DBUILD_TESTING:BOOL=OFF"

        # "-DHAVE_BASH:PATH=OFF"
        "-DHAVE_BLOSC:BOOL=OFF"
        "-DHAVE_BZ2:BOOL=OFF"
        "-DHAVE_SZIP:BOOL=OFF"

        ######### # "-DENABLE_DAP:BOOL=OFF" # see libs.py
        ######### "-DNETCDF_ENABLE_DAP:BOOL=OFF" # see libs.py
        ######### 
  # ?      "-DCURL_DIR:PATH=${CURL_CMAKE_DIR}"
        "-DZLIB_LIBRARY:FILEPATH=${ZLIB_LIBRARY}"
        "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}"
        ######### 
        ######### "-DNC_FIND_SHARED_LIBS:BOOL=OFF"
        ######### # funktionierte so bei geotiff - hier auch???
        ######### "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        ######### "-DENABLE_SHARED_LIBRARY_VERSION:BOOL=OFF"
        ######### 
        ######### "-DENABLE_LARGE_FILE_SUPPORT:BOOL=OFF"  # see libs.py
        ######### ### 2024.11.26: "-DENABLE_EXTREME_NUMBERS:BOOL=OFF"
        ######### "-DENABLE_DAP_REMOTE_TESTS:BOOL=OFF"
        ######### "-DENABLE_BASH_SCRIPT_TESTING:BOOL=OFF"
        ######### ### 2024.11.26: "-DENABLE_:BOOL=OFF"
        ######### 
        ######### "-DHAVE_GETRLIMIT:BOOL=OFF"
        ######### "-DHAVE_MKSTEMP:BOOL=OFF"
    )

    if (${${TARGET_CNAME}_VERSION} VERSION_LESS 4.8)   # -> "4.7.4")
      # Only a weak check, if version > 4.8! Now version should be >= 4.9.3
      message(FATAL_ERROR "### netcdf-Version is less 4.8!")

      list(APPEND CMAKE_ARGS
        ######## "-DBUILD_DLL:BOOL=OFF"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"
        "-DBUILD_TESTING:BOOL=OFF"
        "-DBUILD_UTILITIES:BOOL=OFF"
        ######## "-DBUILD_TESTSETS:BOOL=OFF"

        "-DUSE_HDF5:BOOL=OFF" # see libs.py
        "-DENABLE_NETCDF_4:BOOL=OFF" # see libs.py

        "-DENABLE_BASH_SCRIPT_TESTING:BOOL=OFF"
        "-DENABLE_CDF5:BOOL=OFF"
        "-DENABLE_DAP:BOOL=OFF"
        "-DENABLE_DAP2:BOOL=OFF"
        "-DENABLE_DAP4:BOOL=OFF"
        "-DENABLE_DAP_REMOTE_TESTS:BOOL=OFF"
        "-DENABLE_DLL:BOOL=OFF"
        "-DENABLE_EXAMPLES:BOOL=OFF"
        "-DENABLE_MMAP:BOOL=OFF" # see libs.py
        "-DENABLE_SET_LOG_LEVEL_FUNC:BOOL=OFF"
        "-DENABLE_SHARED_LIBRARY_VERSION:BOOL=OFF"
        "-DENABLE_TESTS:BOOL=OFF"
        
        "-DENABLE_V2_API:BOOL=OFF"
        "-DENABLE_XGETOPT:BOOL=OFF"
      )
    else()
      list(APPEND CMAKE_ARGS
        ######## "-DBUILD_DLL:BOOL=OFF"
        # "-DNETCDF_BUILD_SHARED_LIBS:BOOL=OFF"
        # "-DNETCDF_BUILD_TESTING:BOOL=OFF"
        # "-DBUILD_UTILITIES:BOOL=OFF"
        ######## "-DBUILD_TESTSETS:BOOL=OFF"
        "-DNETCDF_BUILD_UTILITIES:BOOL=OFF"

        "-DNETCDF_ENABLE_CDF5:BOOL=OFF"
        "-DNETCDF_ENABLE_DAP:BOOL=OFF"
        "-DNETCDF_ENABLE_DAP2:BOOL=OFF"
        "-DNETCDF_ENABLE_DAP4:BOOL=OFF"
        "-DNETCDF_ENABLE_DLL:BOOL=OFF"
   ## ?     "-DNETCDF_ENABLE_EXAMPLES:BOOL=OFF"
        
        "-DNETCDF_ENABLE_FILTER_BLOSC:BOOL=OFF"
        "-DNETCDF_ENABLE_FILTER_BZ2:BOOL=OFF"
        "-DNETCDF_ENABLE_FILTER_SZIP:BOOL=OFF"
        "-DNETCDF_ENABLE_FILTER_ZSTD:BOOL=OFF"
        
        "-DNETCDF_ENABLE_HDF5:BOOL=OFF"
    ## ?    "-DNETCDF_ENABLE_XML2:BOOL=OFF"
        
        "-DNETCDF_ENABLE_TESTS:BOOL=OFF"
        "-DNETCDF_ENABLE_FILTER_TESTING:BOOL=OFF"
        "-DNETCDF_ENABLE_PLUGINS:BOOL=OFF"
        "-DNETCDF_ENABLE_MMAP:BOOL=OFF" # see libs.py
        "-DHAVE_MMAP:BOOL=OFF"
      )
    endif()

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/Unidata/netcdf-c.git"
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"           # git tag by libnetcdf_c!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        PATCH_COMMAND ${PYTHON_APP} ${_PATCH_DIR}/cmake_patch.py netcdf

        CMAKE_ARGS ${CMAKE_ARGS}

        INSTALL_COMMAND ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET} ${CURL_TARGET} ${PROJ_TARGET} # ${HDF5_TARGET} 

        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()

# ???  target_link_libraries(${_BUILD_TARGET} PUBLIC ${HDF5_LIBRARY} ${HDF5_HL_LIBRARY})
