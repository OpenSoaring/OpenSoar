cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

prepare_3rdparty(boost boost)

if (_COMPLETE_INSTALL)
    set( CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_BINDIR=${_INSTALL_BIN_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${_INSTALL_INC_DIR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCARES_SHARED=OFF"

        "-DBOOST_INCLUDE_LIBRARIES="
        # "-DBOOST_INCLUDE_LIBRARIES=system;regex;filesystem;chrono;date_time;json;algorithm"
        "-DBOOST_INSTALL_INCLUDE_SUBDIR=" # w/o /boost-1_90
        "-DBOOST_INSTALL_LAYOUT=versioned"   # possible values: versioned, tagged, system
        ### "-DCARES_STATIC=ON"
    )

    set(GIT_TAG boost-${${TARGET_CNAME}_VERSION})
    
    ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY        "https://github.com/boostorg/boost.git"
        GIT_TAG  ${GIT_TAG}  # boost-1.90.0
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}"  # ${LINK_LIBS}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}"
        # PATCH_COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/CURL_CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        CMAKE_ARGS ${CMAKE_ARGS}
        # INSTALL_COMMAND   cmake --build . --target install --config Release
        ${_INSTALL_COMMAND}
        BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        DEPENDS  ${ZLIB_TARGET}  # !!!!
        BUILD_BYPRODUCTS  ${_TARGET_LIBS}
    )
endif()

post_3rdparty()

if (_COMPLETE_INSTALL)
    add_dependencies(${_BUILD_TARGET}  ${ZLIB_TARGET})
endif()

### add_compile_definitions(BOOST_ASIO_SEPARATE_COMPILATION)
### add_compile_definitions(BOOST_MATH_DISABLE_DEPRECATED_03_WARNING=ON) 
### # following libs need this settings: Dialog, libOpenSoar, json, WeGlide, Weather, net, Repository, Task,  ...
### add_compile_definitions(BOOST_JSON_NO_LIB)
### add_compile_definitions(BOOST_CONTAINER_NO_LIB)
### #  ??
### # add_compile_definitions(BOOST_JSON_HEADER_ONLY)
### # add_compile_definitions(BOOST_JSON_STAND_ALONE)
