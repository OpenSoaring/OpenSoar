cmake_minimum_required(VERSION 3.15)
# - <TARGET>                = ${TARGET_CNAME}
#           - upper case name of the current target
# - <target>                = ${TARGET_NAME}
#           - lower case name of the current target
# - <TARGET>_NAME           = ${TARGET_CNAME}_NAME
#           - 
# - <TARGET>_DIR            = ${TARGET_CNAME}_DIR
#           - 
# - <TARGET>         = ${TARGET_CNAME}
#           - 
# - <TARGET>_LIBS           = ${TARGET_CNAME}_LIBS
#           - all needed libraries for target with 'optimized' or 'debug' label
# - <TARGET>_LIBRARY        = ${TARGET_CNAME}_LIBRARY 
#           - current library depend on Debug/Release
# - <TARGET>_LIBRARY_RELEASE= ${TARGET_CNAME}_LIBRARY_RELEASE 
#           - release library
# - <TARGET>_LIBRARY_DEBUG  = ${TARGET_CNAME}_LIBRARY_DEBUG 
#           - debug library 
# - <TARGET>_INCLUDE_DIR    = ${TARGET_CNAME}_INCLUDE_DIR 
#           -

  CMAKE_POLICY(SET CMP0135 NEW) # Or OLD

  if (SHOW_SUBPROJECTS)
    message(STATUS "+++ Start CMake ${CMAKE_CURRENT_SOURCE_DIR}!")
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CLANG ON)
  endif()

if (NOT LINK_LIBS)
  message(STATUS "+++ No LINK_LIBS!!!")
  if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    # fallback only - normally passed via -DLINK_LIBS (XCSOAR_LINK_LIBS)
    foreach(_ll D:/Projects/link_libs C:/Projects/link_libs)
      if (IS_DIRECTORY ${_ll})
        set (LINK_LIBS ${_ll})
        break()
      endif()
    endforeach()
    if (NOT LINK_LIBS)
      set (LINK_LIBS C:/Projects/link_libs)
    endif()
  else()
    set (LINK_LIBS /usr/lib/link_libs)  # Linux,...
    message(STATUS "+++ LINK_LIBS not Windows (${LINK_LIBS})!!!")
  endif()
endif()
message(STATUS "LINK_LIBS = ${LINK_LIBS}")

  include(3rd_party.cmake)
  
  # THIRD_PARTY comes from the command line (-DTHIRD_PARTY=..., fed by
  # CMakeXCSoar.py / env var XCSOAR_THIRD_PARTY) and must NOT be
  # overridden here - this is only the fallback if nothing was passed
  if (NOT THIRD_PARTY)
     message(STATUS "THIRD_PARTY is empty!")
     if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
       if (IS_DIRECTORY D:/Libs)
         set(THIRD_PARTY D:/Libs)  # classic Flaps6 layout
       else()
         set(THIRD_PARTY ${PROJECTGROUP_BINARY_DIR}/3rd_party_build)
       endif()
     endif()
  endif()
  message(STATUS "THIRD_PARTY = ${THIRD_PARTY}")

  set(EP_CMAKE "${THIRD_PARTY}")
  # ATTENTION: (ExtendedProject_Add needs the line 'PREFIX  "${${TARGET_CNAME}_PREFIX}"'
  # for using an other directory and not the internal one 

  option(USE_SYSTEM_${TARGET_CNAME} "Should we use the system ${TARGET_NAME}?" OFF)
  option(EP_BUILD_ALWAYS "Build 3rd party packages always?" OFF)
  option(EP_BUILD_IN_SOURCE "Build 3rd party in sources (Recommended: OFF)?" OFF)

include(CMakeSource.cmake)

set(EXTRA_BUILD 1)

# ---------------------------------------------------------------------------
# git for the ExternalProject GIT_REPOSITORY downloads.  Two traps:
#  * CMake 4.x refuses the submodule handling when it cannot determine
#    the git version ("Git_VERSION=''")
#  * the build scripts put the source dir on PATH, so a stray file
#    named 'git' in the repo root shadows the real git.exe
# Detect git ourselves, validate it by actually running --version, and
# throw a found-but-broken candidate away.
function(_xcsoar_git_version exe out_var)
    if (exe)
        execute_process(COMMAND "${exe}" --version
                        OUTPUT_VARIABLE _raw
                        OUTPUT_STRIP_TRAILING_WHITESPACE
                        ERROR_QUIET)
        string(REGEX MATCH "[0-9]+\\.[0-9]+(\\.[0-9]+)?" _v "${_raw}")
        set(${out_var} "${_v}" PARENT_SCOPE)
    else()
        set(${out_var} "" PARENT_SCOPE)
    endif()
endfunction()

find_package(Git QUIET)
_xcsoar_git_version("${GIT_EXECUTABLE}" _git_found_version)
if (NOT _git_found_version)
    if (GIT_EXECUTABLE)
        message(STATUS ">>> ignoring unusable git candidate: ${GIT_EXECUTABLE}")
    endif()
    # search again, strictly preferring 'git.exe' (a stray extension-less
    # 'git' file cannot win this round); NO_CMAKE_PATH etc. keep it to
    # the system search
    unset(GIT_EXECUTABLE CACHE)
    if (WIN32)
        find_program(GIT_EXECUTABLE NAMES git.exe)
    else()
        find_program(GIT_EXECUTABLE NAMES git)
    endif()
    _xcsoar_git_version("${GIT_EXECUTABLE}" _git_found_version)
endif()
if (NOT GIT_EXECUTABLE OR NOT _git_found_version)
    message(FATAL_ERROR "no usable git found (candidate: '${GIT_EXECUTABLE}') - "
        "check for a stray file named 'git' in the source dir and for git.exe on the PATH")
endif()
set(Git_VERSION "${_git_found_version}")
set(GIT_VERSION_STRING "${_git_found_version}")
message(STATUS ">>> git for ExternalProject: ${GIT_EXECUTABLE} (version '${Git_VERSION}')")
# ---------------------------------------------------------------------------


# param 1: target_name
# param 2: lib_name  (w/o LIB_PREFIX)
# param 3: debug_name (optional - if different from lib_name)
macro(prepare_3rdparty target_name)
    set(params ${ARGN}) # ARGN isn't like a list
    list (GET params 0 lib_name)
    list (LENGTH params _LENGTH)
    get_filename_component(_SOURCE_BASE ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
    string(REGEX REPLACE "/3rd_party" "" _SOURCE_BASE  ${_SOURCE_BASE})

    set(_PATCH_DIR ${_SOURCE_BASE}/lib)

    if (_LENGTH LESS 2)
     #  message(FATAL_ERROR "Empty String (${_LENGTH}) !!!!!!!!!!!!!!!!!!!!!!!!!")
      set(debug_lib ${lib_name})
    else()
      list (GET params 1 debug_lib)
         # message(FATAL_ERROR "String debug_lib = ${debug_lib} / ${_LENGTH} !!!!!!!!!!!!!!!!!!!!!!!!!")
    endif()

    string(TOUPPER ${target_name} TARGET_CNAME)

    string(LENGTH ${TARGET_CNAME} _LENGTH)
    math(EXPR _LENGTH2 "15 - ${_LENGTH}")
    string(REPEAT " " ${_LENGTH2} _DIFF_STRING)
    set(_NEW_STRING "# ${TARGET_CNAME}${_DIFF_STRING}" )
    string(REPEAT ${_NEW_STRING} 6 _DISPLAY_STRING)
    message(STATUS "${_DISPLAY_STRING}")
    # message(STATUS "${_DISPLAY_STRING}  (${_LENGTH} vs ${_LENGTH2})")  # to check the length
    set (${TARGET_CNAME}_VERSION ${${TARGET_CNAME}_VERSION}  PARENT_SCOPE)

    set(LIB_TARGET_NAME ${target_name})
    set(${TARGET_CNAME}_TARGET ${LIB_TARGET_NAME}_3rd)

    if(${EXTRA_BUILD})
       set(_BUILD_TARGET ${LIB_TARGET_NAME}_build)
    else()
       set(_BUILD_TARGET ${${TARGET_CNAME}_TARGET})
    endif()
    # ---------------------------------------------------------------------------
    option(USE_SYSTEM_${TARGET_CNAME} "Should we use the system ${LIB_TARGET_NAME}?" OFF)

    set(XCSOAR_${TARGET_CNAME}_VERSION "${LIB_TARGET_NAME}-${${TARGET_CNAME}_VERSION}")
    set(_INSTALL_DIR "${LINK_LIBS}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}")
    set(${TARGET_CNAME}_INSTALL_DIR "${_INSTALL_DIR}")
    # set(${TARGET_CNAME}_INSTALL_DIR "${LINK_LIBS}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}")
    set(${TARGET_CNAME}_PREFIX "${EP_CMAKE}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}")
    set(${TARGET_CNAME}_SOURCE_DIR  "${${TARGET_CNAME}_PREFIX}/src/${LIB_TARGET_NAME}")

    # per-configuration sub-build for the multi-config MSVC world: each
    # configuration gets its own build/stamp/tmp directories.  Sharing
    # them (the old behaviour) leaves 'done' stamps from a Debug run
    # behind, so a later Release configure adds the project but MSBuild
    # finds nothing to do and the release libs never get built.
    if (MSVC)
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            set(_EP_CFG_DIR ${TOOLCHAIN}-dbg)
        else()
            set(_EP_CFG_DIR ${TOOLCHAIN}-rel)
        endif()
    else()
        set(_EP_CFG_DIR ${TOOLCHAIN})
    endif()
    set(${TARGET_CNAME}_BUILD_DIR  "${${TARGET_CNAME}_PREFIX}/build/${_EP_CFG_DIR}")


    #-------------------
    if(INCLUDE_WITH_TOOLCHAIN)
        set(_INSTALL_INC_DIR        "include/${TOOLCHAIN}")
        # set(${TARGET_CNAME}_INCLUDE_DIR "${_INSTALL_DIR}/include/${TOOLCHAIN}")
    else()
        set(_INSTALL_INC_DIR        "include")
    endif()
    set(${TARGET_CNAME}_INCLUDE_DIR "${_INSTALL_DIR}/${_INSTALL_INC_DIR}")

    if(${EXTRA_BUILD})
       set(_BUILD_TARGET ${LIB_TARGET_NAME}_build)
    else()
       set(_BUILD_TARGET ${${TARGET_CNAME}_TARGET})
       # string(REPLACE "\\" "/" _BUILD_TARGET  ${${TARGET_CNAME}_TARGET})
    endif()

    set(_TARGET_LIB_RELEASE   ${_INSTALL_DIR}/lib/${TOOLCHAIN}/${LIB_PREFIX}${lib_name}${LIB_SUFFIX})
    set(_TARGET_LIB_DEBUG     ${_INSTALL_DIR}/lib/${TOOLCHAIN}d/${LIB_PREFIX}${debug_lib}${LIB_SUFFIX})
    if (MSVC)
      set(_TARGET_LIBS          optimized ${_TARGET_LIB_RELEASE}  debug ${_TARGET_LIB_DEBUG}) 
    else (MSVC)
      set(_TARGET_LIBS          ${_TARGET_LIB_RELEASE})   # ... ${_TARGET_LIB_DEBUG}) 
    endif (MSVC)
   
##aug##    set(_TARGET_LIBRARY           "${_TARGET_LIB_RELEASE}")
    string(REPLACE "\\" "/" _PREFIX_DIR  ${${TARGET_CNAME}_PREFIX})

    set(${TARGET_CNAME}_LIBS  ${_TARGET_LIBS})  ## for internal use
if(${lib_name} MATCHES "boost")

elseif(${lib_name} MATCHES "hdf5")
    if (MSVC)
      list(APPEND _TARGET_LIBS        
        ## optimized ${_TARGET_LIB_RELEASE} 
        optimized ${_INSTALL_DIR}/lib/${TOOLCHAIN}/${LIB_PREFIX}${lib_name}_hl${LIB_SUFFIX}
        ## debug     ${_TARGET_LIB_DEBUG} 
        debug ${_INSTALL_DIR}/lib/${TOOLCHAIN}d/${LIB_PREFIX}${lib_name}_hl_D${LIB_SUFFIX}
      )
    else (MSVC)
      list(APPEND _TARGET_LIBS        
        ${_INSTALL_DIR}/lib/${TOOLCHAIN}/${LIB_PREFIX}${lib_name}_hl${LIB_SUFFIX}
        ## debug: ${_INSTALL_DIR}/lib/${TOOLCHAIN}d/${LIB_PREFIX}${lib_name}_hl_D${LIB_SUFFIX}
      )
    endif (MSVC)
    if(WIN32)
      list(APPEND _TARGET_LIBS    shlwapi )
    endif(WIN32)
endif()
 # set(${TARGET_CNAME}_LIBS  ${_TARGET_LIBS})  ## for internal use
  set(${TARGET_CNAME}_DIR  ${_INSTALL_DIR})  ## D:/Project/link_libs...

if (MSVC OR MINGW OR CLANG)
  set( _BINARY_STEP
       BINARY_DIR ${${TARGET_CNAME}_PREFIX}/build/${_EP_CFG_DIR}
       STAMP_DIR  ${${TARGET_CNAME}_PREFIX}/stamp/${_EP_CFG_DIR}
       TMP_DIR    ${${TARGET_CNAME}_PREFIX}/tmp/${_EP_CFG_DIR})
else()
  set( _BINARY_STEP   )
  string(REPLACE "\\" "/" _INSTALL_DIR ${${TARGET_CNAME}_INSTALL_DIR})
#  message(FATAL_ERROR "_INSTALL_DIR = ${_INSTALL_DIR}")
endif()

if (NOT CMAKE_BUILD_TYPE)
    message(FATAL_ERROR "+++++ CMAKE_BUILD_TYPE = empty ++++++")
endif()

# the running cmake, not a bare 'cmake' - MSBuild's environment does not
# necessarily have it on PATH ('Der Befehl "cmake" ... nicht gefunden')
set(_INSTALL_COMMAND ${CMAKE_COMMAND} --build . --target install)
set(_INSTALL_BIN_DIR "bin/${TOOLCHAIN}")
set(_INSTALL_LIB_DIR "lib/${TOOLCHAIN}")
if(MSVC AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(_TARGET_LIBRARY     ${_TARGET_LIB_DEBUG})
    list(APPEND _INSTALL_COMMAND --config Debug)
    string(APPEND _INSTALL_BIN_DIR "d")
    string(APPEND _INSTALL_LIB_DIR "d")
else()
    set(_TARGET_LIBRARY    ${_TARGET_LIB_RELEASE})
    # mgw152? 
    list(APPEND _INSTALL_COMMAND --config Release)
endif()
    set(${TARGET_CNAME}_LIBRARY  ${_TARGET_LIBRARY})
    set(${TARGET_CNAME}_LIBRARY  ${_TARGET_LIBRARY} PARENT_SCOPE)

# BUILD_BYPRODUCTS for the ExternalProjects: ONLY the library this run
# actually installs (the CMAKE_BUILD_TYPE config).  Never pass
# _TARGET_LIBS here - it contains the literal keywords 'optimized' /
# 'debug' plus the other config's path, i.e. "outputs" that never get
# created, which makes MSBuild consider the step out of date and re-run
# build+install on EVERY build.
set(_TARGET_BYPRODUCTS ${_TARGET_LIBRARY})

# if(${VERBOSE_LEVEL} LESS 100) 
if(VERBOSE_LEVEL GREATER 99)
  message(STATUS "++++++ 3rd party  = ${lib_name} =================")
  ## message(STATUS "++++++ _TARGET_LIBS         =  ${_TARGET_LIBS}")
  message(STATUS "++++++ _TARGET_LIB_DEBUG   =  ${_TARGET_LIB_DEBUG}")
  message(STATUS "++++++ _TARGET_LIB_RELEASE =  ${_TARGET_LIB_RELEASE}")
endif(VERBOSE_LEVEL GREATER 99)

# -------------------------------------------------
if(${lib_name} MATCHES "boost")
##  file(GLOB SCRIPT_FILES ${LINK_LIBS}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}\LINK_LIBS*.lib)
  set(_GLOB_DIR ${_INSTALL_DIR}/lib/${TOOLCHAIN})
  if(MSVC AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(_GLOB_DIR ${_GLOB_DIR}d)
  endif()
  message(STATUS "### boost: _INSTALL_DIR = ${_GLOB_DIR} ")
  file(GLOB BOOST_LIB_FILES ${_GLOB_DIR}/libboost_*.lib)
  list(LENGTH BOOST_LIB_FILES BOOST_LIB_COUNT)
  if (BOOST_LIB_COUNT  GREATER 1)
    message(STATUS "### boost: BOOST_LIB_FILES = ${BOOST_LIB_FILES} -> ${BOOST_LIB_COUNT} ")
  else()
    message(STATUS "### boost: _COMPLETE_INSTALL = ON} -> ${BOOST_LIB_COUNT} ")
    set(_COMPLETE_INSTALL ON)  # No boost library found
  endif()
  # message(FATAL_ERROR "### boost: BOOST_LIB_FILES = ${BOOST_LIB_FILES} -> ${BOOST_LIB_COUNT} ")

elseif(NOT EXISTS ${${TARGET_CNAME}_INCLUDE_DIR})
   # library found, but no headers at the expected include path -
   # e.g. after a lib switched to a per-toolchain include dir (sdl2:
   # the generated SDL_config.h is compiler-specific) -> reinstall
   set(_COMPLETE_INSTALL ON)
elseif(EXISTS ${_TARGET_LIBRARY})
   set(_COMPLETE_INSTALL OFF)
     # only a check if some libraries have to be compiled
   if(
        # ${lib_name} STREQUAL "sodium" OR 
        # ${lib_name} STREQUAL "curl" OR 
        # ${lib_name} STREQUAL "sqlite3"  OR 
        # ${lib_name} STREQUAL "proj" OR 
        # ${lib_name} STREQUAL "geotiff" OR 
        # ${lib_name} STREQUAL "netcdf" OR
        # ${lib_name} STREQUAL "netcdf-cxx4" OR
        OFF  # finish entry :)
     )
     set(_COMPLETE_INSTALL ON)
   endif()
else()
    set(_COMPLETE_INSTALL ON)
    ##   message(FATAL_ERROR "### PNG: _TARGET_LIBRARY = ${_TARGET_LIBRARY} ")
endif()

# one decision line per library - this is THE line to look for when a
# lib does not get built: it shows the exact path checked, the build
# type and the verdict
if (_COMPLETE_INSTALL)
    message(STATUS ">>> ${lib_name} [${CMAKE_BUILD_TYPE}]: MISSING ${_TARGET_LIBRARY} -> ExternalProject will build+install")
else()
    message(STATUS ">>> ${lib_name} [${CMAKE_BUILD_TYPE}]: found ${_TARGET_LIBRARY} -> nothing to do")
endif()
### Only test w/ complete build of 3rd party libs:
### set(_COMPLETE_INSTALL ON) ## test only (15.12.2025)!!!!!!!!
# -------------------------------------------------

get_filename_component(${TARGET_CNAME}_LIB_DIR  ${${TARGET_CNAME}_LIBRARY} PATH)

# this <TARGET>_CMAKE_DIR could be changed in target.cmake!
set(_CMAKE_LIB "${_INSTALL_LIB_DIR}/cmake")
set(${TARGET_CNAME}_CMAKE_DIR ${${TARGET_CNAME}_LIB_DIR}/cmake)

### -> Public: ... PARENT_SCOPE
  set(${TARGET_CNAME}_LIBS  ${_TARGET_LIBS} PARENT_SCOPE)  ## D:/Project/link_libs...
  set(${TARGET_CNAME}_TARGET  ${${TARGET_CNAME}_TARGET} PARENT_SCOPE) # f.e. zlib_3rd, sodium_3rd
  # set(_TARGET_LIBS    ${_TARGET_LIBS} PARENT_SCOPE)
  set(${TARGET_CNAME}_LIB_DIR ${${TARGET_CNAME}_LIB_DIR} PARENT_SCOPE)
endmacro()

macro(post_3rdparty)
    if(${EXTRA_BUILD})
        # ExternalProject_Get_Property(${_BUILD_TARGET} _INSTALL_DIR)
            add_library(${${TARGET_CNAME}_TARGET} STATIC IMPORTED GLOBAL)
        # set_source_files_properties(${${TARGET_CNAME}_LIB} PROPERTIES GENERATED TRUE)
        # set_target_properties(${${TARGET_CNAME}_TARGET} PROPERTIES IMPORTED_LOCATION  ${${TARGET_CNAME}_LIB}  GENERATED TRUE)
        if (MSVC)
            # multi-config generator (Visual Studio): resolve the library
            # PER CONFIGURATION, so switching Debug/Release inside the IDE
            # links the matching lib from  lib/<toolchain>  or
            # lib/<toolchain>d  - IMPORTED_LOCATION alone would freeze the
            # choice at configure time
            set_target_properties(${${TARGET_CNAME}_TARGET} PROPERTIES
                IMPORTED_LOCATION_RELEASE        "${_TARGET_LIB_RELEASE}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${_TARGET_LIB_RELEASE}"
                IMPORTED_LOCATION_MINSIZEREL     "${_TARGET_LIB_RELEASE}"
                IMPORTED_LOCATION_DEBUG          "${_TARGET_LIB_DEBUG}"
                IMPORTED_LOCATION                "${_TARGET_LIBRARY}"
                GENERATED TRUE)
        else()
            set_target_properties(${${TARGET_CNAME}_TARGET} PROPERTIES IMPORTED_LOCATION  ${_TARGET_LIBRARY}  GENERATED TRUE)
        endif()
        set_target_properties(${${TARGET_CNAME}_TARGET} PROPERTIES FOLDER 3rdParty)
        # set_target_properties(${_BUILD_TARGET} PROPERTIES FOLDER 3rdParty)
        add_dependencies(${${TARGET_CNAME}_TARGET}  ${_BUILD_TARGET})

  if(${lib_name} MATCHES "sqlite")
    message(STATUS "!!!!! sqlite3 = ${TARGET_CNAME}_TARGET ||| ${${TARGET_CNAME}_TARGET}")
    # message(FATAL_ERROR "!!!!! Stop !!!!")
  endif()
    endif()

if (_COMPLETE_INSTALL)
    set_target_properties(${_BUILD_TARGET} PROPERTIES FOLDER 3rdParty) # either zlib_build or zlib_3rd
endif()
    list(APPEND THIRDPARTY_INCLUDES  ${${TARGET_CNAME}_INCLUDE_DIR})

    list(APPEND 3RDPARTY_TARGETS ${${TARGET_CNAME}_TARGET})

    # collect the ExternalProject targets that were really created this
    # configure (missing/changed libs only) - the application adds an
    # explicit add_dependencies() on them, so MSBuild finishes building
    # and INSTALLING the libraries before the app links (with --parallel,
    # relying on the imported-target edge alone is not enough)
    if (_COMPLETE_INSTALL)
        list(APPEND 3RDPARTY_BUILD_TARGETS ${_BUILD_TARGET})
    endif()

### -> Public: ... PARENT_SCOPE
  set(${TARGET_CNAME}_TARGET_LIBS  ${_TARGET_LIBS} PARENT_SCOPE)
  set(${TARGET_CNAME}_INCLUDE_DIR  ${${TARGET_CNAME}_INCLUDE_DIR} PARENT_SCOPE)
  set(${TARGET_CNAME}_ROOT  ${_INSTALL_DIR} PARENT_SCOPE)
endmacro()

  if(${lib_name} MATCHES "sqlite")
    message(FATAL_ERROR "!!!!! CMAKE_FILES = ${CMAKE_FILES}")
  endif()
    foreach(cmake_file ${CMAKE_FILES})
      message(STATUS "!!!!! cmake_file = ${cmake_file}")
      include(${cmake_file})
    endforeach()
  
    #this makes the THIRDPARTY_INCLUDES visible to outside in general:
    set(THIRDPARTY_INCLUDES ${THIRDPARTY_INCLUDES} PARENT_SCOPE)

    source_group("Scripts" FILES  ${SCRIPT_FILES} )
    source_group("Targets" FILES  ${CMAKE_FILES})

    ## message(FATAL_ERROR "SCRIPT_FILES = ${SCRIPT_FILES}")
    add_custom_target(ThirdParty
        # EXCLUDE_FROM_ALL
        SOURCES
        ${CMAKE_FILES}
        ${SCRIPT_FILES}
    )

    set(3RDPARTY_TARGETS ${3RDPARTY_TARGETS} PARENT_SCOPE)
    set(3RDPARTY_BUILD_TARGETS ${3RDPARTY_BUILD_TARGETS} PARENT_SCOPE)

    if (DISPLAY_TARGETS)
        foreach(_target ${3RDPARTY_TARGETS} ) 
           message(STATUS "### 3RDPARTY_TARGETS = ${_target}")
        endforeach()
    # message(FATAL_ERROR "SCRIPT_FILES = ${SCRIPT_FILES}")
    endif()
    
set_target_properties(ThirdParty PROPERTIES FOLDER 3rdParty)


if(VERBOSE_LEVEL GREATER 80)
# message(STATUS      "!!!! HDF5_LIB_DIR: ${HDF5_LIB_DIR}")
message(STATUS      "!!!! CURL_LIB_DIR: ${CURL_LIB_DIR}")
message(STATUS      "!!!! ------------------------------------------  !!")
# message(STATUS      "!!!! HDF5_DIR:     ${HDF5_DIR}")
message(STATUS      "!!!! CURL_DIR:     ${CURL_DIR}")
message(STATUS      "!!!! ZLIB_DIR:     ${ZLIB_DIR}")
message(STATUS      "!!!! ------------------------------------------  !!")
# message(STATUS      "!!!! HDF5F_CXX_CMAKE_DIR: ${HDF5_CMAKE_DIR}")
message(STATUS      "!!!! NETCDF_CXX_CMAKE_DIR: ${NETCDF_CXX_CMAKE_DIR}")
endif(VERBOSE_LEVEL GREATER 80)
