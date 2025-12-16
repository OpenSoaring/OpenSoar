cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME proj)

# set (HDF5_DIR ${LINK_LIBS}/hdf5/hdf5-${HDF5_VERSION})
# set (CURL_DIR ${LINK_LIBS}/curl/curl-${CURL_VERSION})
# set (ZLIB_DIR ${LINK_LIBS}/zlib/zlib-${ZLIB_VERSION})

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
####
####        # "-DUSE_THREAD=OFF"
        "-DUSE_PKGCONFIG_REQUIRES=OFF"
####
        "-DENABLE_TIFF=ON" 
        "-DTiff_DIR:PATH=${TIFF_CMAKE_DIR}"    # /tiff"
####        # "-DTIFF_DIR:PATH=${LINK_LIBS}/tiff/tiff-4.6.0/lib/msvc2022/cmake/tiff"
####        "-DTIFF_DIR:PATH=${TIFF_CMAKE_DIR}/tiff"
####        # "-DTIFF_INCLUDE_DIR:PATH=${LINK_LIBS}/tiff/tiff-4.6.0/include"
####        "-DTIFF_INCLUDE_DIR:PATH=${TIFF_INCLUDE_DIR}"
####        # "-DTIFF_LIBRARY_DEBUG:FILEPATH=${LINK_LIBS}/tiff/tiff-4.6.0/lib/msvc2022d/tiff.lib"
####        # "-DTIFF_LIBRARY_RELEASE:FILEPATH=${LINK_LIBS}/tiff/tiff-4.6.0/lib/msvc2022/tiff.lib"
####        "-DTIFF_LIBRARY:FILEPATH=${TIFF_LIBRARY}"
####
        "-DEXE_SQLITE3=${LINK_LIBS}/sqlite/sqlite-${SQLITE3_VERSION}/bin/${TOOLCHAIN}/sqlite3.exe"

        "-DENABLE_CURL=OFF"
        # "-DCURL_DIR:PATH=${CURL_CMAKE_DIR}"
        "-DZLIB_LIBRARY:PATH=${ZLIB_LIBRARY}"
        "-DZLIB_INCLUDE_DIR:PATH=${ZLIB_INCLUDE_DIR}"

        # Manually-specified variables were not used by the project: "-DINSTALL_GTEST=OFF"
        # Manually-specified variables were not used by the project: "-Dgtest_force_shared_crt=OFF"
        # Manually-specified variables were not used by the project: "-DRUN_NETWORK_DEPENDENT_TESTS=OFF"
        # Manually-specified variables were not used by the project: "-DTESTING_USE_NETWORK=OFF"
        # "-DINSTALL_LEGACY_CMAKE_FILES=OFF"

        # CMake Error at generate_proj_db.cmake:76 (message):  Build of proj.db failed
    )
    # message (FATAL_ERROR "xxxx STOP PROJ!!!") 
    if (PROJ_VERSION VERSION_GREATER 9.4)
      list(APPEND CMAKE_ARGS
        # "-DSQLite3_LIBRARY:FILEPATH=${SQLITE3_LIBRARY}"
        "-DSQLite3_DIR:PATH=${${SQLITE3_CMAKE_DIR}}"
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
    ####  not possible? set(_PATCH_COMMAND )
    ####  not possible? if(EXISTS ${BUILD_SOURCE}/CMakeLists.txt)
    ####  not possible?   string(APPEND _PATCH_COMMAND "echo No Patch in '_PATCH_COMMAND' ") # TEST!!!!
    ####  not possible?   # string(APPEND _PATCH_COMMAND "PATCH_COMMAND git apply ${_PATCH_DIR}/patches/disable_db.patch")
    ####  not possible?   # string(APPEND _PATCH_COMMAND " dir && git apply ${_PATCH_DIR}/patches/disable_db.patch")
    ####  not possible?   # string(APPEND _PATCH_COMMAND echo Test)
    ####  not possible? 
    ####  not possible?   # message (FATAL_ERROR "xxxx STOP No Patch !!! -  ${BUILD_SOURCE}/CMakeLists.txt") 
    ####  not possible?   set(PATCH_ENABLED OFF)
    ####  not possible? else()
    ####  not possible?   string(APPEND _PATCH_COMMAND "git apply ${_PATCH_DIR}/patches/disable_db.patch")
    ####  not possible?   # string(APPEND _PATCH_COMMAND ${CMAKE_COMMAND} -E echo 'patch irgendwas')
    ####  not possible?   message (STATUS "_PATCH_COMMAND: ${_PATCH_COMMAND}")
    ####  not possible?   # message (FATAL_ERROR "xxxx STOP withPatch !!! -  ${BUILD_SOURCE}/CMakeLists.txt") 
    ####  not possible? endif()
    
     # message(FATAL_ERROR  "!!! _BINARY_STEP = '${_BINARY_STEP}'")

    set(_BINARY_DIR ${THIRD_PARTY}/proj/proj-${PROJ_VERSION}/build/${TOOLCHAIN})
    
    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/OSGeo/PROJ.git"
        GIT_TAG "${${TARGET_CNAME}_VERSION}"           # git tag by libproj!
        PREFIX  "${${TARGET_CNAME}_PREFIX}"

        # PREFIX  "${${TARGET_CNAME}_PREFIX}"
        # ${_BINARY_STEP}
        # BINARY_DIR D:/Libs/proj/proj-9.4.1/build/msvc2026
        BINARY_DIR ${_BINARY_DIR}
        INSTALL_DIR "${_INSTALL_DIR}"

        # PATCH_COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/lib/proj/cmake_patch.py 
        # PATCH_COMMAND ${PYTHON_APP} D:/Projects/OpenSoaring/OpenSoar/lib/proj/cmake_patch.py 
        PATCH_COMMAND ${PYTHON_APP} ${_PATCH_DIR}/cmake_patch.py
        # PATCH_COMMAND git apply ${_PATCH_DIR}/patches/disable_db.patch  || exit 0  # ${_PATCH_COMMAND}
        # PATCH_COMMAND git apply ${_PATCH_DIR}/patches/disable_db.patch  || exit 0  # ${_PATCH_COMMAND}
        
        # PATCH_COMMAND ${PYTHON_APP} ${_PATCH_DIR}/cmake_patch.py

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

# if (_COMPLETE_INSTALL)
#  add_dependencies(${_BUILD_TARGET}  ${SQLITE3_LIBRARY}) # ${SQLITE3_LIB_DIR})
#  target_link_libraries(${_BUILD_TARGET} PUBLIC ${SQLITE3_LIBRARY})

# endif()

# add_dependencies(${_BUILD_TARGET}  ${SQLITE3_TARGET} ${SQLITE3_LIB_DIR})

