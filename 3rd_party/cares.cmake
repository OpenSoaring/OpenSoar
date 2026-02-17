cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!

prepare_3rdparty(cares cares)

if (_COMPLETE_INSTALL)
    set( CMAKE_ARGS
        "-DCMAKE_INSTALL_PREFIX=${_INSTALL_DIR}"
        "-DCMAKE_INSTALL_BINDIR=${_INSTALL_BIN_DIR}"
        "-DCMAKE_INSTALL_LIBDIR=${_INSTALL_LIB_DIR}"
        "-DCMAKE_INSTALL_INCLUDEDIR=${_INSTALL_INC_DIR}"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCARES_SHARED=OFF"
        "-DCARES_STATIC=ON"
        "-DCARES_STATIC_PIC=ON"
        "-DCARES_BUILD_TESTS=OFF"
        "-DCARES_BUILD_TOOLS=OFF"   # OFF"
    )

    if(${TARGET_CNAME}_VERSION VERSION_LESS 1.30.0)
       # GIT_TAG before 1.30.0 (f.e. "cares-1_29_0")
       # after 1.17.1 only 'cares', before c-ares!
       string(REPLACE "." "_" GIT_TAG cares-${${TARGET_CNAME}_VERSION})
    else()
       # GIT_TAG after 1.30.0 (f.e. "v1.30.0"):
       set(GIT_TAG v${${TARGET_CNAME}_VERSION})
    endif()
    
    ExternalProject_Add(
          ${_BUILD_TARGET}
       GIT_REPOSITORY "https://github.com/c-ares/c-ares.git"
       GIT_TAG  ${GIT_TAG}
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
        ${_BINARY_STEP}
        INSTALL_DIR "${_INSTALL_DIR}" 
        PATCH_COMMAND ${PYTHON_APP} ${_PATCH_BASE}/cmake_patch.py cares
        CMAKE_ARGS ${CMAKE_ARGS}
        INSTALL_COMMAND ${_INSTALL_COMMAND}
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
