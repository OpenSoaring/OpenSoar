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

# wrong patch path name:
set(_PATCH_DIR ${PATCH_BASE}/libgeotiff)

if (_COMPLETE_INSTALL )
    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX:PATH=${GEOTIFF_DIR}"
        # not used     "-DCMAKE_INSTALL_LIBDIR:PATH=${GEOTIFF_LIB_DIR}"
        # not used    "-DCMAKE_INSTALL_INCLUDEDIR:PATH=${GEOTIFF_INCLUDE_DIR}"   # "include"
        "-DCMAKE_BUILD_TYPE=Release" # ${CMAKE_BUILD_TYPE}"

        # "-DWITH_UTILITIES:BOOL=OFF"
        "-DBUILD_SHARED_LIBS=OFF"
 
        "-DGEOTIFF_LIB_SUBDIR:PATH=${GEOTIFF_LIB_DIR}"
        "-DWITH_UTILITIES:BOOL=OFF"
        "-DBUILD_SHARED_LIBS:BOOL=OFF"
       
        "-DTIFF_DIR:PATH=${TIFF_CMAKE_DIR}"   # "/lib/${TOOLCHAIN}/cmake/tiff"
        "-DTiff_DIR:PATH=${TIFF_CMAKE_DIR}"   # "/lib/${TOOLCHAIN}/cmake/tiff"
        "-DTIFF_INCLUDE_DIR:PATH=${TIFF_INCLUDE_DIR}"

        # w/ Debug too?
        "-DSQLite3_LIBRARY:FILEPATH=${SQLITE3_LIBRARY}/lib/${TOOLCHAIN}/sqlite3.lib"
        "-DSQLite3_INCLUDE_DIR:PATH=${SQLITE3_INCLUDE_DIR}"
        
        "-DPROJ_DIR:PATH=${PROJ_CMAKE_DIR}"  # ${PROJ_DIR}"  # "/lib/${TOOLCHAIN}/cmake/proj"
        "-DPROJ_LIBRARY:PATH=${PROJ_LIBRARY}/lib/${TOOLCHAIN}/proj.lib"
        "-DPROJ_INCLUDE_DIR:PATH=${PROJ_DIR}/include"
        # 1.7.1 wrong?"-DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY"
        
        "-DLINK_LIBS:PATH=${LINK_LIBS}"
    )
    if({TARGET_CNAME}_VERSION VERSION_LESS 1.7.4)
        list(APPEND CMAKE_ARGS "-DCMAKE_POLICY_VERSION_MINIMUM=3.5")
    endif()

   ### set(_PATCH_DIR ${_PATCH_BASE}/geotiff)

    message(STATUS "geotiff: ${_COMPLETE_INSTALL} |||  ${GEOTIFF_DIR} ||| ${CURL_LIBRARY} ||| -> ${_TARGET_LIBS}")
    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/OSGeo/libgeotiff.git"
        GIT_TAG "${${TARGET_CNAME}_VERSION}"           # git tag by libgeotiff!
  
        # PREFIX  "${${TARGET_CNAME}_PREFIX}/libgeotiff"
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        # SOURCE_DIR ${${TARGET_CNAME}_PREFIX}/src/geotiff_build
        SOURCE_SUBDIR libgeotiff
        ${_BINARY_STEP}
        # DOWNLOAD_DIR "${GEOTIFF_SOURCE_DIR}"
        # SOURCE_DIR "${GEOTIFF_SOURCE_DIR}/libgeotiff/libgeotiff"
        INSTALL_DIR "${_INSTALL_DIR}"
        # CONFIGURE_COMMAND "${CMAKE_COMMAND} -H ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff -B ${GEOTIFF_PREFIX}/build"
        # CONFIGURE_COMMAND "${CMAKE_COMMAND} -G 'Visual Studio 17' ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff"
        # CONFIGURE_COMMAND "${CMAKE_COMMAND} -G \"Visual Studio 17 2022\" -S ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff -B ../test2"
        # WORKING_DIRECTORY ${GEOTIFF_SOURCE_DIR}/libgeotiff_build/libgeotiff

        #PATCH_COMMAND ${PYTHON_APP} ${_PATCH_BASE}/cmake_patch.py geotiff  # ${target_name}

  
   # PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECTGROUP_SOURCE_DIR}/3rd_party/geotiff_CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
   # PATCH_COMMAND git apply "${_PATCH_DIR}/patches/cmake-minimum.patch"
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBPNG/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        ## PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${GEOTIFF_SOURCE_DIR}/geotiff_build/libgeotiff/*" "${GEOTIFF_SOURCE_DIR}/geotiff_build/*"
        CMAKE_ARGS ${CMAKE_ARGS}
        # BUILD_COMMAND cmake --build -S ${GEOTIFF_SOURCE_DIR}/libgeotiff -B .

        INSTALL_COMMAND ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET} ${TIFF_TARGET} ${PROJ_TARGET}
     
 # 2024-22-19 ausgeblendet, 2026-01-08 wieder eingeblendet?
        BUILD_BYPRODUCTS  ${_TARGET_LIBS}
    )
    ## message(FATAL_ERROR "geotiff: ${_COMPLETE_INSTALL} |||  ${GEOTIFF_DIR} ||| ${CURL_LIBRARY} ||| -> ${_TARGET_LIBS}")
endif()
post_3rdparty()
