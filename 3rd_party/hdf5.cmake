cmake_minimum_required(VERSION 3.15)
# get_filename_component(LIB_TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

if (0)
set(_LIB_NAME hdf5)
else(0)
if (MSVC)  # unfortunately the lib name is a little bit 'tricky' at libPng..
  set(_LIB_NAME  libhdf5)
else(MSVC)
  set(_LIB_NAME hdf5)
endif(MSVC)
endif(0)

prepare_3rdparty(hdf5 ${_LIB_NAME} ${_LIB_NAME}_D)


set(HDF5_HL_LIBRARY  ${_INSTALL_DIR}/lib/${TOOLCHAIN}/${LIB_PREFIX}${_LIB_NAME}_hl${LIB_SUFFIX})  # finally

# string(APPEND HDF5_CMAKE_DIR  /???L)
if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  # set(HDF5_CMAKE_DIR  ${HDF5_DIR}/cmake)
  set(HDF5_CMAKE_DIR  ${HDF5_LIB_DIR}/cmake)
  set(HDF5_CMAKE_DIR  lib/${TOOLCHAIN}d/cmake)

else()
  # set(HDF5_CMAKE_DIR  ${HDF5_DIR}/cmake)
  # set(HDF5_CMAKE_DIR  ${HDF5_LIB_DIR}/cmake)
  set(HDF5_CMAKE_DIR  lib/${TOOLCHAIN}/cmake)
endif()

# NUR TEst: !!!!
  set(HDF5_CMAKE_DIR  ${HDF5_CMAKE_DIR} PARENT_SCOPE)
#   set(HDF5_DIR  ${HDF5_CMAKE_DIR} PARENT_SCOPE)
#   set(HDF5_DIR  ${HDF5_DIR} PARENT_SCOPE)
###############################################################################################

# set (ZLIB_DIR ${LINK_LIBS}/zlib/zlib-${ZLIB_VERSION})

### message(STATUS "======================================================== (in hdf5.cmake)")
### message(STATUS "++++++ ZLIB_DIR      =  ${ZLIB_DIR}")
### message(STATUS "++++++ ZLIB_LIB      =  ${ZLIB_LIB}")
### message(STATUS "++++++ ZLIB_TARGET   =  ${ZLIB_TARGET}")
### message(STATUS "++++++ ZLIB_LIBRARY  =  ${ZLIB_LIBRARY}")
# message(FATAL_ERROR Stop!)

# set (ZLIB_DIR ${ZLIB_LIB})
# set(HDF5_INSTALL_CMAKE_DIR )

if (_COMPLETE_INSTALL)
#  endif()
#  if (ON)
    set(CMAKE_ARGS
             "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
             # "-DCMAKE_INSTALL_PREFIX=${HDF5_DIR}"
#             "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
             # "-DHDF5_INSTALL_BIN_DIR=${_INSTALL_BIN_DIR}" # 03.11.24 
             "-DHDF5_INSTALL_BIN_DIR=${HDF5_BIN_DIR}" # 03.11.24 
             # "-DHDF5_INSTALL_CMAKE_DIR=${_INSTALL_LIB_DIR}/cmake"
             "-DHDF5_INSTALL_CMAKE_DIR=${HDF5_CMAKE_DIR}"
 #         "-DHDF5_INSTALL_CMAKE_DIR=lib/msvc2022/cmake"
          #  "-DHDF5_INSTALL_LIB_DIR=lib/${TOOLCHAIN}"
          # "-DHDF5_INSTALL_LIB_DIR=${_INSTALL_LIB_DIR}"
          "-DHDF5_INSTALL_LIB_DIR=${HDF5_LIB_DIR}"
             "-DHDF5_INSTALL_INCLUDE_DIR=${HDF5_INCLUDE_DIR}"
            # "-DHDF5_INSTALL_LIB_DIR=${_INSTALL_LIB_DIR}" # 03.11.24 
            # "-DHDF5_EXTERNAL_LIB_PREFIX="
            # "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY="  #  "${_INSTALL_LIB_DIR}"
            # "-DCMAKE_INSTALL_INCLUDEDIR=include"
            "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  
        # "-DWITH_UTILITIES=OFF"
        # 05.11.24: "-DBUILD_SHARED_LIBS=ON"   # 03.11.24 "OFF"
        "-DBUILD_SHARED_LIBS=OFF"   # 05.11.24
        "-DBUILD_STATIC_LIBS=ON"   # 03.11.24 "OFF"
        "-DBUILD_TESTING=OFF"
        # "-DH5EX_BUILD_TESTING=OFF"
        # "-DH5EX_BUILD_EXAMPLES=OFF"
#             "-DHDF5_ENABLE_Z_LIB_SUPPORT=ON"
             "-DHDF5_ENABLE_Z_LIB_SUPPORT=OFF"
#             "-DZLIB_USE_EXTERNAL=ON"
             "-DZLIB_USE_EXTERNAL=OFF"

         "-DSZIP_USE_EXTERNAL=ON"
         ## 2024-12-03 "-DSZIP_DIR=${SZIP_LIB}"
         "-DSZIP_LIBRARY:FILEPATH=${SZIP_LIBRARY}"

         ## 2024-12-03 "-DZLIB_DIR=${ZLIB_LIB}"
        #     "-DZLIB_INCLUDE_DIR=${ZLIB_INCLUDE_DIR}"
        #     "-DZLIB_LIBRARY=${ZLIB_LIB}"
         "-DZLIB_LIBRARY:FILEPATH=${ZLIB_LIBRARY}"
            # "-DZLIB_LIBRARY_DEBUG=${ZLIB_LIB}"
             # "-DZLIB_LIBRARY_RELEASE=${ZLIB_LIB}"
             "-DHDF5_USE_ZLIB_STATIC:BOOL=ON" # see Release.txt, line 346 ('Configuration')
             "-DHDF5_ENABLE_SZIP_ENCODING:BOOL=ON"
             "-DHDF5_ENABLE_SZIP_SUPPORT:BOOL=ON"

             "-DHDF5_BUILD_TOOLS:BOOL=OFF"
             "-DHDF5_BUILD_STATIC_TOOLS:BOOL=OFF"

             # "-DHDF5_TEST_CPP=OFF"
             # "-DHDF5_TEST_EXAMPLES=OFF"
             # "-DHDF5_TEST_FORTRAN=OFF"
             # "-DHDF5_TEST_JAVA=OFF"
             # "-DHDF5_TEST_PASSTHROUGH_VOL=OFF"
             # "-DHDF5_TEST_PARALLEL=OFF"
             "-DHDF5_TEST_SERIAL=OFF"
             "-DHDF5_TEST_SWMR=OFF"
             # "-DHDF5_TEST_TOOLS=OFF"
             # "-DHDF5_TEST_VFD=OFF"

             "-DHDF5_BUILD_EXAMPLES=OFF"
             # 05.11.24: "-DHDF5_BUILD_STATIC_TOOLS=ON"   # 03.11.24 "
             "-DHDF5_BUILD_UTILS=OFF"

             "-DHDF5_BUILD_HL_LIB=ON"
             # 05.11.24: "-DHDF5_BUILD_TOOLS=ON"

             # "-DTEST_SHELL_SCRIPTS=OFF"
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

        DEPENDS ${ZLIB_TARGET} ${PROJ_TARGET} ${SZIP_TARGET}
       # DEPENDS ${ZLIB_LIB}
     
        BUILD_BYPRODUCTS  ${_TARGET_LIBS} # ${${TARGET_CNAME}_LIB}
    )
endif()
post_3rdparty()

set(HDF5_CMAKE_DIR  ${HDF5_LIB_DIR}/cmake)  # finally
