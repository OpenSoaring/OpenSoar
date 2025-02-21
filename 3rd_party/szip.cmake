cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
set(_LIB_NAME szip)
prepare_3rdparty(${_LIB_NAME} ${_LIB_NAME})

if (_COMPLETE_INSTALL)
  set(CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
    # "-DCMAKE_INSTALL_PREFIX=${${TARGET_CNAME}_PREFIX}"
    "-DCMAKE_INSTALL_LIBDIR:PATH=${_INSTALL_LIB_DIR}"
    # "-DCMAKE_INSTALL_INCLUDEDIR:PATH=${_INSTALL_INCLUDE_DIR}"
    "-DCMAKE_INSTALL_INCLUDEDIR:PATH=include"
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_INSTALL_BINDIR:PATH=${_INSTALL_BIN_DIR}"
    # n.u. "-DCMAKE_INSTALL_COMPONENT=bin/${TOOLCHAIN}"
  )

  ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/erdc/szip.git"
        # GIT_TAG "version-${${TARGET_CNAME}_VERSION}"
        GIT_TAG master    # no git tag available
        PREFIX  "${${TARGET_CNAME}_PREFIX}"

        CMAKE_ARGS ${CMAKE_ARGS}
        ${_BINARY_STEP}

        PATCH_COMMAND ${PYTHON_APP}
              ${PROJECTGROUP_SOURCE_DIR}/3rd_party/cmake_patch.py 
              ${PROJECTGROUP_SOURCE_DIR} <SOURCE_DIR>
        # SZConfig.h is 'stolen' from ./szip/szip-2.1/src/szip_build/windows/szipproj.zip/szip/src/SZConfig.h
        INSTALL_DIR "${_INSTALL_DIR}"
        # BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        # DEPENDS zlib
  )

post_3rdparty()

endif(_COMPLETE_INSTALL)
