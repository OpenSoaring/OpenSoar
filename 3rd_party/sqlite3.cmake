cmake_minimum_required(VERSION 3.15)

set(INCLUDE_WITH_TOOLCHAIN 0)  # special include path for every toolchain!
set(_LIB_NAME sqlite3)

message(STATUS "---- _LIB_NAME (1)= ${_LIB_NAME}") 
prepare_3rdparty(${_LIB_NAME} ${_LIB_NAME})
message(STATUS "---- _LIB_NAME (2)= ${_LIB_NAME}") 

# needed from other 3rd party libs (f.e. proj)
set(SQLITE3_EXE        ${_INSTALL_DIR}/${_INSTALL_BIN_DIR}/${_LIB_NAME}${EXE_PREFIX})
set(SQLITE3_EXE        ${SQLITE3_EXE}               PARENT_SCOPE)

  if (${SQLITE3_VERSION} MATCHES "3.42.0")
     set(SQLITE3_YEAR         2023)
     ### # hash from url: "https://www.sqlite.org/2023/sqlite-autoconf-3420000.tar.gz"
     # set(SQLITE3_HASH         7abcfd161c6e2742ca5c6c0895d1f853c940f203304a0b49da4e1eca5d088ca6)
     set(SQLITE3_HASH         643898e9fcc8f6069bcd47b0e6057221c1ed17bbee57da20d2752c79d91274e8)
  elseif (${SQLITE3_VERSION} MATCHES "3.46.1")
    set(SQLITE3_YEAR         2024)
    set(SQLITE3_HASH            23075baddabe87d609251bbb66884f218dea9e185dea3178fe20eecc5337a6bb)
  elseif(${SQLITE3_VERSION} MATCHES "3.47.0")
    set(SQLITE3_YEAR         2024)
    set(SQLITE3_HASH         e35ee48efc24fe58d0e9102034ac0a41e3904641a745e94ab11a841fe9d7355e)
  elseif (${SQLITE3_VERSION} MATCHES "3.50.4")
     set(SQLITE3_YEAR         2025)
     ### # hash from url: "https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip"
     set(SQLITE3_HASH         934fafe96caa7f4c16e82e0c2b674441a715c038acc9780bf15e09411daba70c)
  elseif (${SQLITE3_VERSION} MATCHES "3.51.1")
     set(SQLITE3_YEAR         2025)
     ### # hash from url: "https://www.sqlite.org/2025/sqlite-amalgamation-3510100.zip"
     #set(SQLITE3_HASH         856b52ffe7383d779bb86a0ed1ddc19c41b0e5751fa14ce6312f27534e629b64)
     set(SQLITE3_HASH         9b2b1e73f577def1d5b75c5541555a7f42e6e073ad19f7a9118478389c9bbd9b)
  endif()
  #  a10d1469493c62c0667c63d889269f802bbb212d695533cd13611a713a9f77a7)


# if (1)
if (_COMPLETE_INSTALL)
  message(STATUS "---- _LIB_NAME (3)= ${_LIB_NAME}") 

  ### BUILD_CONIG -Setups:
  set(BUILD_CONIG NO_BUILD)
  set(BUILD_CONIG CMAKE_BUILD)
  # set(BUILD_CONIG MAKEFILE_BUILD)
  set(BUILD_CONIG ZIPFILE_BUILD)
  
  # build the version string for download (f.e 3.46.1 to 3460100)
  string(REPLACE "." "" _VERSION ${${TARGET_CNAME}_VERSION})
  string(SUBSTRING ${_VERSION} 0 3 _VERSION1)
  string(SUBSTRING ${_VERSION} 3 1 _VERSION2)
  string(CONCAT DOWNLOAD_VERSION ${_VERSION1} "0" ${_VERSION2} "00")
  
  message(STATUS "_VERSION         = ${_VERSION}")
  message(STATUS "_VERSION1        = ${_VERSION1}")
  message(STATUS "_VERSION2        = ${_VERSION2}")
  message(STATUS "DOWNLOAD_VERSION = ${DOWNLOAD_VERSION}")
  #message(FATAL_ERROR DOWNLOAD_VERSION = ${DOWNLOAD_VERSION})
  
  
  message(STATUS "---- BUILD_CONIG = ${BUILD_CONIG}") 
  # if(1)
  if (${BUILD_CONIG} MATCHES NO_BUILD)
    # set(SQLITE3_LIB ${LINK_LIBS}/sqlite3/test/lib/sqlite3.lib PARENT_SCOPE)
    set(SQLITE3_LIB ${SQLITE3_LIB} PARENT_SCOPE)
    set(SQLITE3_LIBRARY ${SQLITE3_LIBRARY} PARENT_SCOPE)

    message (STATUS "xxxx SQLITE3_LIBRARY: ${SQLITE3_LIBRARY}")
# 2024.11.26:   ## 
message (FATAL_ERROR "xxxx STOP!!!") 

    # set(SQLITE3_LIB ${LINK_LIBS}/sqlite3/test/lib/sqlite3.lib PARENT_SCOPE)
    #set(SQLITE3_LIB ${SQLITE3_LIB} PARENT_SCOPE)
    #set(SQLITE3_LIBRARY ${SQLITE3_LIBRARY} PARENT_SCOPE)
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
          DEPENDS zlib
    )
###    post_3rdparty()
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
###    post_3rdparty()
  elseif (${BUILD_CONIG} MATCHES ZIPFILE_BUILD)
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
#          URL "https://www.sqlite.org/${SQLITE3_YEAR}/sqlite-amalgamation-${DOWNLOAD_VERSION}.zip"
          URL "https://www.sqlite.org/${SQLITE3_YEAR}/sqlite-autoconf-${DOWNLOAD_VERSION}.tar.gz"
          URL_HASH SHA3_256=${SQLITE3_HASH}
       
       # no cmake: use Makefile(.msc)
       #  GIT_REPOSITORY "https://github.com/sqlite/sqlite.git"
       #  GIT_TAG "version-${${TARGET_CNAME}_VERSION}"           # git tag by libproj!

          PREFIX  "${${TARGET_CNAME}_PREFIX}"
  
          CMAKE_ARGS ${CMAKE_ARGS}
          ${_BINARY_STEP}
          PATCH_COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECTGROUP_SOURCE_DIR}/3rd_party/${LIB_TARGET_NAME}_CMakeLists.txt.in" <SOURCE_DIR>/CMakeLists.txt
          INSTALL_DIR "${_INSTALL_DIR}"
    )
    ### post_3rdparty()
  
    # set(SQLITE3_LIBRARY ${LINK_LIBS}/sqlite/sqlite-${SQLITE3_VERSION}/bin/${TOOLCHAIN}d/sqlite.lib PARENT_SCOPE)
    message(STATUS "---- BINARY_STEP = ${_BINARY_STEP}")
  endif (${BUILD_CONIG} MATCHES NO_BUILD)
endif(_COMPLETE_INSTALL)

post_3rdparty()
