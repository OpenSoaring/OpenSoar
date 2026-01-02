cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

 set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
if (MSVC)  # unfortunately the lib name is a little bit 'tricky' at libPng..
  set(_LIB_NAME geotiff)
  # message(FATAL_ERROR LibPng: MSVC')
 else()
  set(_LIB_NAME geotiff)
endif()

prepare_3rdparty(geotiff ${_LIB_NAME} ${_LIB_NAME}_d)

#wrong patch path name:
set(_PATCH_DIR ${PROJECTGROUP_SOURCE_DIR}/lib/libgeotiff)

# D:/Projects/Binaries/OpenSoar/dev-branch/3rd_Party/geotiff/geotiff-1.7.1/src/geotiff/libgeotiff/libgeotiff
# D:\Projects\Binaries\OpenSoar\dev-branch\3rd_Party\geotiff\geotiff-1.7.1\src\geotiff\libgeotiff

##set(GEOTIFF_SOURCE_DIR  "${GEOTIFF_PREFIX}/src")

# set(TIFF_DIR  "${LINK_LIBS}/tiff/tiff-4.6.0")
# set(PROJ_DIR  "${LINK_LIBS}/proj/proj-9.3.1")

if (_COMPLETE_INSTALL )
    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX:PATH=${GEOTIFF_DIR}"
             "-DCMAKE_INSTALL_LIBDIR:PATH=${GEOTIFF_LIB_DIR}"
            "-DCMAKE_INSTALL_INCLUDEDIR:PATH=${GEOTIFF_INCLUDE_DIR}"   # "include"
            "-DCMAKE_BUILD_TYPE=Release" # ${CMAKE_BUILD_TYPE}"

        # "-DWITH_UTILITIES:BOOL=OFF",
        "-DBUILD_SHARED_LIBS=OFF",
 
        "-DGEOTIFF_LIB_SUBDIR:PATH=${GEOTIFF_LIB_DIR}"
        "-DWITH_UTILITIES:BOOL=OFF"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"

        # "-DCURL_DIR:PATH=${CURL_CMAKE_DIR}"
        "-DCURL_INCLUDE_DIR:PATH=${CURL_INCLUDE_DIR}"
        "-DCURL_LIBRARIES:PATH=${CURL_LIB_DIR}"
        "-DCURL_LIBRARY:FILEPATH=${CURL_LIBRARY}"
        "-DCURL_LIBRARY_DEBUG:FILEPATH=${CURL_LIBRARY}"
        "-DCURL_LIBRARY_RELEASE:FILEPATH=${CURL_LIBRARY}"
        
        "-DTIFF_DIR:PATH=${TIFF_CMAKE_DIR}"   # "/lib/${TOOLCHAIN}/cmake/tiff"
        "-DTiff_DIR:PATH=${TIFF_CMAKE_DIR}"   # "/lib/${TOOLCHAIN}/cmake/tiff"
        "-DTIFF_INCLUDE_DIR:PATH=${TIFF_INCLUDE_DIR}"
        "-DTIFF_LIBRARIES:PATH=${TIFF_LIB_DIR}"
        "-DTIFF_LIBRARY:FILEPATH=${TIFF_LIBRARY}"

        "-DSQLite3_LIBRARY:FILEPATH=${SQLITE3_LIBRARY}/lib/${TOOLCHAIN}/sqlite3.lib"
        "-DSQLite3_INCLUDE_DIR:PATH=${SQLITE3_INCLUDE_DIR}"
        
        "-DPROJ_DIR:PATH=${PROJ_CMAKE_DIR}"  # ${PROJ_DIR}"  # "/lib/${TOOLCHAIN}/cmake/proj"
        # "-DPROJ_INCLUDE_DIR:PATH=${PROJ_DIR}/include"
        "-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        
        "-DLINK_LIBS:PATH=${LINK_LIBS}"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/OSGeo/libgeotiff.git"
        GIT_TAG "${${TARGET_CNAME}_VERSION}"           # git tag by libgeotiff!
  
        # PREFIX  "${${TARGET_CNAME}_PREFIX}/libgeotiff"
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        # DOWNLOAD_DIR "${GEOTIFF_SOURCE_DIR}"
        # SOURCE_DIR "${GEOTIFF_SOURCE_DIR}/libgeotiff/libgeotiff"
        INSTALL_DIR "${_INSTALL_DIR}"
        # CONFIGURE_COMMAND "${CMAKE_COMMAND} -H ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff -B ${GEOTIFF_PREFIX}/build"
        # CONFIGURE_COMMAND "${CMAKE_COMMAND} -G 'Visual Studio 17' ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff"
        # CONFIGURE_COMMAND "${CMAKE_COMMAND} -G \"Visual Studio 17 2022\" -S ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff -B ../test2"
        # WORKING_DIRECTORY ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff
  
   PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECTGROUP_SOURCE_DIR}/3rd_party/geotiff_CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
   # PATCH_COMMAND git apply "${_PATCH_DIR}/patches/cmake-minimum.patch"
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBPNG/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        ## PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${GEOTIFF_SOURCE_DIR}/geotiff_build/libgeotiff/*" "${GEOTIFF_SOURCE_DIR}/geotiff_build/*"
        CMAKE_ARGS ${CMAKE_ARGS}
        # BUILD_COMMAND cmake --build -S ${GEOTIFF_SOURCE_DIR}/libgeotiff -B .


        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET} ${TIFF_TARGET} ${PROJ_TARGET} # ${HDF5_TARGET}
     
 # 2024-22-19       BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
