cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

set(_LIB_NAME proj)

# set (HDF5_DIR ${LINK_LIBS}/hdf5/hdf5-${HDF5_VERSION})
# set (CURL_DIR ${LINK_LIBS}/curl/curl-${CURL_VERSION})
# set (ZLIB_DIR ${LINK_LIBS}/zlib/zlib-${ZLIB_VERSION})

message (STATUS "xxxx SQLITE_LIBRARY: ${SQLITE_LIBRARY}")
# 2024.11.26:   ## message (FATAL_ERROR "xxxx STOP!!!") 

prepare_3rdparty(proj ${_LIB_NAME} ${_LIB_NAME}_d)
string(APPEND PROJ_CMAKE_DIR  /proj)
# string(APPEND PROJ_CMAKE_DIR  /proj4)
if (_COMPLETE_INSTALL)
  
    # set(SQLITE_LIBRARY ${LINK_LIBS}/sqlite/sqlite-${SQLITE_VERSION}/bin/${TOOLCHAIN}d/sqlite.lib) # PARENT_SCOPE)

    set(CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=include"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
 
        "-DBUILD_APPS=OFF"

        "-DBUILD_TESTING=OFF"
        "-DENABLE_TIFF=OFF"  # ON" ??
        "-DENABLE_CURL=OFF"
        "-DBUILD_CCT=OFF"
        "-DBUILD_CS2CS=OFF"
        "-DBUILD_GEOD=OFF"
        "-DBUILD_GIE=OFF"
        "-DBUILD_PROJ=OFF"
        "-DBUILD_PROJINFO=OFF"
        "-DBUILD_PROJSYNC=OFF"
        "-DBUILD_SHARED_LIBS=OFF"

        # "-DUSE_THREAD=OFF"

        # "-DTIFF_DIR:PATH=${LINK_LIBS}/tiff/tiff-4.6.0/lib/msvc2022/cmake/tiff"
        "-DTIFF_DIR:PATH=${TIFF_CMAKE_DIR}/tiff"
        # "-DTIFF_INCLUDE_DIR:PATH=${LINK_LIBS}/tiff/tiff-4.6.0/include"
        "-DTIFF_INCLUDE_DIR:PATH=${TIFF_INCLUDE_DIR}"
        # "-DTIFF_LIBRARY_DEBUG:FILEPATH=${LINK_LIBS}/tiff/tiff-4.6.0/lib/msvc2022d/tiff.lib"
        # "-DTIFF_LIBRARY_RELEASE:FILEPATH=${LINK_LIBS}/tiff/tiff-4.6.0/lib/msvc2022/tiff.lib"
        "-DTIFF_LIBRARY:FILEPATH=${TIFF_LIBRARY}"

#        "-DEXE_SQLITE3=${LINK_LIBS}/sqlite/sqlite-${SQLITE_VERSION}/bin/${TOOLCHAIN}/sqlite3.exe"
        "-DEXE_SQLITE3:FILEPATH=${SQLITE_EXE}"  ##"${LINK_LIBS}/sqlite/sqlite-${SQLITE_VERSION}/bin/${TOOLCHAIN}/sqlite3"
        "-DSQLITE3_EXE:FILEPATH=${SQLITE_EXE}"  ##${LINK_LIBS}/sqlite/sqlite-${SQLITE_VERSION}/bin/${TOOLCHAIN}/sqlite3"
        "-DSQLITE3_LIBRARY:FILEPATH=${SQLITE_LIBRARY}"
        "-DSQLITE3_INCLUDE_DIR:PATH=${SQLITE_INCLUDE_DIR}"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/OSGeo/PROJ.git"
        GIT_TAG "${${TARGET_CNAME}_VERSION}"           # git tag by libproj!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBPROJ/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        CMAKE_ARGS ${CMAKE_ARGS}
        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET} ${SQLITE_TARGET} ${TIFF_TARGET}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()

# if (_COMPLETE_INSTALL)
#  add_dependencies(${_BUILD_TARGET}  ${SQLITE_LIBRARY}) # ${SQLITE_LIB_DIR})
#  target_link_libraries(${_BUILD_TARGET} PUBLIC ${SQLITE_LIBRARY})

# endif()

# add_dependencies(${_BUILD_TARGET}  ${SQLITE_TARGET} ${SQLITE_LIB_DIR})

