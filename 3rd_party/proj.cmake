cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME proj)

set (SQLITE3_LIBRARY ${LINK_LIBS}/sqlite/sqlite-${SQLITE3_VERSION})

message (STATUS "xxxx SQLITE3_LIBRARY: ${SQLITE3_LIBRARY}")
message (STATUS "xxxx TIFF_CMAKE_DIR: ${TIFF_CMAKE_DIR}")
# 2024.11.26: message (FATAL_ERROR "xxxx STOP!!!") 

prepare_3rdparty(proj ${_LIB_NAME} ${_LIB_NAME}_d)
string(APPEND PROJ_CMAKE_DIR  /proj)
# string(APPEND PROJ_CMAKE_DIR  /proj4)
if (_COMPLETE_INSTALL)  #  || 1)
    # set(SQLITE3_LIBRARY ${LINK_LIBS}/sqlite/sqlite-${SQLITE3_VERSION}/bin/${TOOLCHAIN}d/sqlite.lib) # PARENT_SCOPE)

    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX:PATH=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_LIBDIR:PATH=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR:PATH=include"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
 
        "-DBUILD_APPS=OFF"
        # Manually-specified variables were not used by the project: "-DBUILD_BENCHMARKS=OFF"
        "-DBUILD_CCT=OFF"
        "-DBUILD_CS2CS=OFF"
        "-DBUILD_GEOD=OFF"
        "-DBUILD_GIE=OFF"
        # Manually-specified variables were not used by the project: "-DBUILD_GMOCK=OFF"
        "-DBUILD_PROJ=OFF"
        "-DBUILD_PROJINFO=OFF"
        "-DBUILD_PROJSYNC=OFF"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_TESTING=OFF"
        "-DCPACK_BINARY_NSIS=OFF"
        "-DUSE_PKGCONFIG_REQUIRES=OFF"
        "-DENABLE_TIFF=ON" 
        "-DTiff_DIR:PATH=${TIFF_CMAKE_DIR}"    # /tiff"
        "-DENABLE_CURL=OFF"
        "-DZLIB_INCLUDE_DIR:PATH=${ZLIB_INCLUDE_DIR}"

        "-DEMBED_RESOURCE_FILES:BOOL=OFF" # start with 4.6.0
    )
    # message (FATAL_ERROR "xxxx STOP PROJ!!!") 
    if (PROJ_VERSION VERSION_GREATER 9.4)
      list(APPEND CMAKE_ARGS
        # "-DSQLite3_LIBRARY:FILEPATH=${SQLITE3_LIBRARY}"
 ##       "-DSQLite3_DIR:PATH=${${SQLITE3_CMAKE_DIR}}"
        "-DSQLite3_LIBRARY:FILEPATH=${SQLITE3_LIBRARY}/lib/${TOOLCHAIN}/"
        "-DSQLite3_INCLUDE_DIR:PATH=${SQLITE3_INCLUDE_DIR}"
      )
message (STATUS "xxxx PROJ_VERSION : ${PROJ_VERSION}" )
# 2026/01/06: message (FATAL_ERROR "xxxx STOP > 9.4!!!") 
    else()
      list(APPEND CMAKE_ARGS
        "-DSQLite3_DIR:PATH=${${SQLITE3_CMAKE_DIR}}"
        "-DSQLITE3_LIBRARY:FILEPATH=${SQLITE3_LIBRARY}"
        "-DSQLITE3_INCLUDE_DIR:PATH=${SQLITE3_INCLUDE_DIR}"
message (STATUS "xxxx PROJ_VERSION : ${PROJ_VERSION}")
# 2024.11.26: 
message (FATAL_ERROR "xxxx STOP << 9.4 !!!") 
      )
    endif()
    set(BUILD_SOURCE ${THIRD_PARTY}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}/src/${_BUILD_TARGET})
    set(_BINARY_DIR ${THIRD_PARTY}/proj/proj-${PROJ_VERSION}/build/${TOOLCHAIN})
    
    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/OSGeo/PROJ.git"
        GIT_TAG "${${TARGET_CNAME}_VERSION}"           # git tag by libproj!
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
      
        BINARY_DIR ${_BINARY_DIR}
        INSTALL_DIR "${_INSTALL_DIR}"

        PATCH_COMMAND ${PYTHON_APP} ${_PATCH_DIR}/cmake_patch.py proj # ${PROJ_VERSION}

        CMAKE_ARGS ${CMAKE_ARGS}
        
        INSTALL_COMMAND ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS OFF
        BUILD_IN_SOURCE OFF
        DEPENDS ${ZLIB_TARGET} ${TIFF_TARGET} ${SQLITE3_TARGET} 
        
        # next line needed from clang (not necessary in MinGW and MSVC):
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()

post_3rdparty()
