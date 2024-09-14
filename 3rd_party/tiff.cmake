cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

 set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
if (MSVC)  # unfortunately the lib name is a little bit 'tricky' at libPng..
  set(_LIB_NAME libtiff)
  set(_LIB_NAME tiff)
  # message(FATAL_ERROR LibPng: MSVC')
 else()
  set(_LIB_NAME tiff)
endif()

prepare_3rdparty(tiff ${_LIB_NAME})
if (_COMPLETE_INSTALL)

    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
             "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB}"
            "-DCMAKE_INSTALL_INCLUDEDIR=include"
            "-DCMAKE_BUILD_TYPE=Release"
  
        "-DBUILD_SHARED_LIBS=OFF"
        "-Dtiff-tools=OFF"
        "-Dtiff-tests=OFF"
        "-Dtiff-contrib=OFF"
        "-Dtiff-docs=OFF"
        "-Dtiff-deprecated=OFF"
        "-Dtiff-install=ON"
        "-Dld-version-script=OFF"
        "-Dccitt=OFF"
        "-Dpackbits=OFF"
        "-Dlzw=OFF"
        "-Dthunder=OFF"
        "-Dnext=OFF"
        "-Dlogluv=OFF"
        "-Dmdi=OFF"
        "-Dpixarlog=OFF"
        "-DCMAKE_DISABLE_FIND_PACKAGE_Deflate=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_JPEG=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_JBIG=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_liblzma=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_ZSTD=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_LERC=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_WebP=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_OpenGL=ON"
        "-DCMAKE_DISABLE_FIND_PACKAGE_GLUT=ON"
        "-Dcxx=OFF"
        "-Dstrip-chopping=OFF"
        "-Dextrasample-as-alpha=OFF"

        # workaround for build failure with -Dstrip-chopping=OFF
        "-DSTRIP_SIZE_DEFAULT=8192"

        "-DCMAKE_EXE_LINKER_FLAGS=-lm"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/libsdl-org/libtiff.git"
        GIT_TAG "v${${TARGET_CNAME}_VERSION}"           # git tag by libtiff!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBPNG/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        CMAKE_ARGS ${CMAKE_ARGS}
        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        # DEPENDS ${ZLIB_TARGET}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
