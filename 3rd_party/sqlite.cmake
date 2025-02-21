cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
set(_LIB_NAME sqlite)
prepare_3rdparty(${_LIB_NAME} ${_LIB_NAME})

# needed from other 3rd party libs (f.e. proj)
set(SQLITE_EXE        ${_INSTALL_DIR}/${_INSTALL_BIN_DIR}/sqlite3${EXE_PREFIX})
set(SQLITE_EXE        ${SQLITE_EXE}               PARENT_SCOPE)

if (_COMPLETE_INSTALL)

### BUILD_CONIG -Setups:
set(BUILD_CONIG NO_BUILD)
set(BUILD_CONIG CMAKE_BUILD)
# set(BUILD_CONIG MAKEFILE_BUILD)
set(BUILD_CONIG BINARY_BUILD)

# build the version string for download (f.e 3.46.1 to 3460100)
string(REPLACE "." "" _VERSION ${${TARGET_CNAME}_VERSION})
string(SUBSTRING ${_VERSION} 0 3 _VERSION1)
string(SUBSTRING ${_VERSION} 3 1 _VERSION2)
string(CONCAT DOWNLOAD_VERSION ${_VERSION1} "0" ${_VERSION2} "00")

## message(STATUS "_VERSION         = ${_VERSION}")
## message(STATUS "_VERSION1        = ${_VERSION1}")
## message(STATUS "_VERSION2        = ${_VERSION2}")
## message(STATUS "DOWNLOAD_VERSION = ${DOWNLOAD_VERSION}")
#message(FATAL_ERROR DOWNLOAD_VERSION = ${DOWNLOAD_VERSION})


  message(STATUS "---- BUILD_CONIG = ${BUILD_CONIG}") 
if (${BUILD_CONIG} MATCHES NO_BUILD)
  set(SQLITE_LIB ${LINK_LIBS}/sqlite3/test/lib/sqlite3.lib PARENT_SCOPE)
  set(SQLITE_LIBRARY ${SQLITE_LIB} PARENT_SCOPE)
  message(STATUS "---- BINARY_STEP = ${_BINARY_STEP}")
elseif (${BUILD_CONIG} MATCHES CMAKE_BUILD)
  set(CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
    # "-DCMAKE_INSTALL_PREFIX=${${TARGET_CNAME}_PREFIX}"
    "-DCMAKE_INSTALL_LIBDIR:PATH=${_INSTALL_LIB_DIR}"
    "-DCMAKE_INSTALL_INCLUDEDIR:PATH=${_INSTALL_INCLUDE_DIR}"
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_INSTALL_BINDIR:PATH=${_INSTALL_BIN_DIR}"
    # n.u. "-DCMAKE_INSTALL_COMPONENT=bin/${TOOLCHAIN}"
    # "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
  )
  ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/sqlite/sqlite.git"
        GIT_TAG "version-${${TARGET_CNAME}_VERSION}"           # git tag by libproj!
        PREFIX  "${${TARGET_CNAME}_PREFIX}"

        CMAKE_ARGS ${CMAKE_ARGS}
        ${_BINARY_STEP}

        PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECTGROUP_SOURCE_DIR}/3rd_party/${LIB_TARGET_NAME}_CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        INSTALL_DIR "${_INSTALL_DIR}"
###     BUILD_ALWAYS ${EP_BUILD_ALWAYS}
        # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
        # DEPENDS zlib
  )
post_3rdparty()
elseif (${BUILD_CONIG} MATCHES MAKEFILE_BUILD)
  # das ist noch nicht fertig!
  ExternalProject_Add(
        ${_BUILD_TARGET}
        GIT_REPOSITORY "https://github.com/sqlite/sqlite.git"
        GIT_TAG "version-${${TARGET_CNAME}_VERSION}"           # git tag by libproj!
        PREFIX  "${${TARGET_CNAME}_PREFIX}"
         CONFIGURE_COMMAND ""
         BUILD_COMMAND nmake /f Makefile.msc
         INSTALL_COMMAND ""
### fmt:      set(BUILD_COMMAND devenv ${_SOURCE_DIR}/libfmt.sln /Build Release|x64)
### fmt:      set(CONFIGURE_COMMAND  echo ${BUILD_COMMAND})
### fmt:      # set(INSTALL_COMMAND xcopy ${_SOURCE_DIR}\\Build\\Release\\x64\\*.lib ${_INSTALL_DIR}\\lib\\${TOOLCHAIN}\\* /Y /T)
### fmt:      set(INSTALL_COMMAND xcopy ${_SOURCE_DIR}/src/libfmt/include/*.h ${_INSTALL_DIR}/include/* /Y /I /S /E)

   # CUSTOMBUILD make
   # BUILD_COMMAND make
   
   # BUILD_COMMAND "${CMAKE_COMMAND} -E make"
#MinGW ??  BUILD_COMMAND make
# MSVC   BUILD_COMMAND nmake /f Makefile.msc sqlite3.exe
   ## BUILD_COMMAND "${CMAKE_COMMAND} -E make"
   ###      # HOST_CC=...   # See below
   ###      # CC=${CMAKE_C_COMPILER}
   ###      make # -C <SOURCE_DIR>
    

    #### CMAKE_ARGS ${CMAKE_ARGS}

###  direct build  with 'NMAKE'
###   CONFIGURE_COMMAND "./sqlite/configure --enable-all"
###    INSTALL_DIR "${_INSTALL_DIR}"   # ${LINK_LIBS}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}"
###    INSTALL_COMMAND make DESTDIR=<INSTALL_DIR> install
###   BUILD_IN_SOURCE TRUE

###    BUILD_ALWAYS ${EP_BUILD_ALWAYS}
    # BUILD_IN_SOURCE ${EP_BUILD_IN_SOURCE}
    # DEPENDS zlib
  )
  post_3rdparty()
elseif (${BUILD_CONIG} MATCHES BINARY_BUILD)
  set(CMAKE_ARGS
    "-DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>"
    # "-DCMAKE_INSTALL_PREFIX=${${TARGET_CNAME}_PREFIX}"
    "-DCMAKE_INSTALL_LIBDIR:PATH=${_INSTALL_LIB_DIR}"
    "-DCMAKE_INSTALL_INCLUDEDIR:PATH=${_INSTALL_INCLUDE_DIR}"
    "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
    "-DCMAKE_INSTALL_BINDIR:PATH=${_INSTALL_BIN_DIR}"
    # n.u. "-DCMAKE_INSTALL_COMPONENT=bin/${TOOLCHAIN}"
  )
  ExternalProject_Add(
        ${_BUILD_TARGET}
        # URL "https://www.sqlite.org/2024/sqlite-amalgamation-3470000.zip"
        URL "https://www.sqlite.org/${SQLITE_YEAR}/sqlite-amalgamation-${DOWNLOAD_VERSION}.zip"
        URL_HASH SHA3_256=${SQLITE_HASH}
        PREFIX  "${${TARGET_CNAME}_PREFIX}"

        CMAKE_ARGS ${CMAKE_ARGS}
        ${_BINARY_STEP}
        PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECTGROUP_SOURCE_DIR}/3rd_party/${LIB_TARGET_NAME}_CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
        INSTALL_DIR "${_INSTALL_DIR}"
  )
  post_3rdparty()

  # set(SQLITE_LIBRARY ${LINK_LIBS}/sqlite/sqlite-${SQLITE_VERSION}/bin/${TOOLCHAIN}d/sqlite.lib PARENT_SCOPE)
  message(STATUS "---- BINARY_STEP = ${_BINARY_STEP}")
endif (${BUILD_CONIG} MATCHES NO_BUILD)

endif(_COMPLETE_INSTALL)
