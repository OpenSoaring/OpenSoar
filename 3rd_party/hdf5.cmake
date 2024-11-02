cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

# set(_LIB_NAME hdf5)
if (MSVC)  # unfortunately the lib name is a little bit 'tricky' at libPng..
  set(_LIB_NAME  libhdf5)
  # message(FATAL_ERROR LibPng: MSVC')
 else()
  set(_LIB_NAME hdf5)
endif()

prepare_3rdparty(hdf5 ${_LIB_NAME})

# set (ZLIB_DIR ${ZLIB_LIB})

if (_COMPLETE_INSTALL)

    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
             "-DHDF5_INSTALL_LIB_DIR=${_INSTALL_LIB}"
            "-DHDF5_EXTERNAL_LIB_PREFIX="
            "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY="  #  "${_INSTALL_LIB}"
            "-DCMAKE_INSTALL_INCLUDEDIR=include"
            "-DCMAKE_BUILD_TYPE=Release"
  
        "-DWITH_UTILITIES=OFF"
        "-DBUILD_SHARED_LIBS=OFF"
        "-DBUILD_TESTING=OFF"
        # "-DH5EX_BUILD_TESTING=OFF"
        # "-DH5EX_BUILD_EXAMPLES=OFF"
             "-DHDF5_ENABLE_Z_LIB_SUPPORT=ON"
             "-DZLIB_USE_EXTERNAL=OFF"  # ON?
             "-DZLIB_DIR=${ZLIB_LIB}"
             "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}"
             "-DZLIB_LIBRARY=${ZLIB_LIB}"
             "-DZLIB_LIBRARY_DEBUG=${ZLIB_LIB}"
             "-DZLIB_LIBRARY_RELEASE=${ZLIB_LIB}"

             "-DHDF5_TEST_CPP=OFF"
             "-DHDF5_TEST_EXAMPLES=OFF"
             "-DHDF5_TEST_FORTRAN=OFF"
             "-DHDF5_TEST_JAVA=OFF"
             "-DHDF5_TEST_PASSTHROUGH_VOL=OFF"
             "-DHDF5_TEST_PARALLEL=OFF"
             "-DHDF5_TEST_SERIAL=OFF"
             "-DHDF5_TEST_SWMR=OFF"
             "-DHDF5_TEST_TOOLS=OFF"
             "-DHDF5_TEST_VFD=OFF"

             "-DHDF5_BUILD_EXAMPLES=OFF"
             "-DHDF5_BUILD_STATIC_TOOLS=OFF"
             "-DHDF5_BUILD_UTILS=OFF"

             "-DHDF5_BUILD_HL_LIB=ON"
             "-DHDF5_BUILD_TOOLS=ON"

             "-DTEST_SHELL_SCRIPTS=OFF"
    )

    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/HDFGroup/hdf5.git"
        GIT_TAG "hdf5_${${TARGET_CNAME}_VERSION}"           # git tag by libhdf5!
  
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"
  
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/LIBPNG/CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        CMAKE_ARGS ${CMAKE_ARGS}
        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
  
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS ${ZLIB_TARGET}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()
