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

  if (SHOW_SUBPROJECTS)
    message(STATUS "+++ Start CMake ${CMAKE_CURRENT_SOURCE_DIR}!")
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CLANG ON)
  endif()

if (NOT LINK_LIBS)
  message(STATUS "+++ No LINK_LIBS!!!")
  set (LINK_LIBS /usr/lib/link_libs)  # August2111: this Windows, Flaps6,....
endif (NOT LINK_LIBS)

  include(3rd_party.cmake)
  
  # set(EP_BASE "${THIRD_PARTY}/external")  # later remove 'External!' # not used up to now!
  set(EP_CMAKE "${THIRD_PARTY}")   ##/cmake")  # later remove 'cmake!'
  # set(EP_BINARIES "${BINARY_DIR}/3rd_party")  # Binaries dahin verlagern?
  option(USE_SYSTEM_${TARGET_CNAME} "Should we use the system ${TARGET_NAME}?" OFF)
  option(EP_BUILD_ALWAYS "Build 3rd party packages always?" OFF)
  option(EP_BUILD_IN_SOURCE "Build 3rd party in sources (Recommended: OFF)?" OFF)


# file(GLOB_RECURSE SCRIPT_FILES *.txt *.cmake *.in)
# file(GLOB_RECURSE SCRIPT_FILES *.txt *.in)

include(CMakeSource.cmake)

set(EXTRA_BUILD 1)

### macro(install_step arg1)
###    message(STATUS "========================== ${arg1} ===================================")
###    if(0)
###      set (_install_cmd  INSTALL_COMMAND  cmake --build . --target install --config Debug)
###    else()
###      set (_install_cmd  "INSTALL_COMMAND \"\"")
###    endif(1)
### 
### endmacro()

#macro(prepare_3rdparty target_name) #  lib_name)
macro(prepare_3rdparty target_name)
    set(params ${ARGN}) # ARGN isn't like a list
    list (GET params 0 lib_name)
    list (LENGTH params _LENGTH)

    ## if ("${debug_lib}" STREQUAL "NOTFOUND")
    if (_LENGTH LESS 2)
     #  message(FATAL_ERROR "Empty String (${_LENGTH}) !!!!!!!!!!!!!!!!!!!!!!!!!")
      set(debug_lib ${lib_name})
    else()
      list (GET params 1 debug_lib)
         # message(FATAL_ERROR "String debug_lib = ${debug_lib} / ${_LENGTH} !!!!!!!!!!!!!!!!!!!!!!!!!")
    endif()
if (${target_name} MATCHES "hdf5")
## message(STATUS "target_name (${target_name}) !!!!!!!!!!!!!!!!!!!!!!!!!")
## message(STATUS "lib_name    (${lib_name}) !!!!!!!!!!!!!!!!!!!!!!!!!")
## message(STATUS "debug_lib   (${debug_lib}) !!!!!!!!!!!!!!!!!!!!!!!!!")
# message(FATAL_ERROR "Stop !!!!!!!!!!!!!!!!!!!!!!!!!") 
endif()
    string(TOUPPER ${target_name} TARGET_CNAME)

    string(LENGTH ${TARGET_CNAME} _LENGTH)
    math(EXPR _LENGTH2 "15 - ${_LENGTH}")
    string(REPEAT " " ${_LENGTH2} _DIFF_STRING)
    set(_NEW_STRING "# ${TARGET_CNAME}${_DIFF_STRING}" )
    string(REPEAT ${_NEW_STRING} 6 _DISPLAY_STRING)
    message(STATUS "${_DISPLAY_STRING}")
    # message(STATUS "${_DISPLAY_STRING}  (${_LENGTH} vs ${_LENGTH2})")  # to check the length

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


    #-------------------
    if(INCLUDE_WITH_TOOLCHAIN)
        set(_INSTALL_INC_DIR        "include/${TOOLCHAIN}")
        # set(${TARGET_CNAME}_INCLUDE_DIR "${_INSTALL_DIR}/include/${TOOLCHAIN}")
    else()
        set(_INSTALL_INC_DIR        "include")
    endif()
    set(${TARGET_CNAME}_INCLUDE_DIR "${_INSTALL_DIR}/${_INSTALL_INC_DIR}")

    # set(_INSTALL_DIR "${LINK_LIBS}/${LIB_TARGET_NAME}/${XCSOAR_${TARGET_CNAME}_VERSION}")
    # set(INSTALL_BIN "${_INSTALL_DIR}/bin/${TOOLCHAIN}")
    # set(INSTALL_LIB "${_INSTALL_DIR}/lib/${TOOLCHAIN}")

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
if(${lib_name} MATCHES "hdf5")
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
  set( _BINARY_STEP BINARY_DIR ${${TARGET_CNAME}_PREFIX}/build/${TOOLCHAIN})
else()
  set( _BINARY_STEP   )
  string(REPLACE "\\" "/" _INSTALL_DIR ${${TARGET_CNAME}_INSTALL_DIR})
#  message(FATAL_ERROR "_INSTALL_DIR = ${_INSTALL_DIR}")
endif()

## message (FATAL_ERROR "Config = '$<CONFIG:Debug>', '$<CONFIG>', '$<CONFIGURATION>', '${CMAKE_BUILD_TYPE}' ")
## if($<CONFIG:Debug>) 
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    set(CMAKE_BUILD_TYPE Debug)
    # message(FATAL_ERROR "+++++ CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE} ++++++")
    if(VERBOSE_LEVEL GREATER 99)
      message(STATUS "+++++ CMAKE_BUILD_TYPE (1) = ${CMAKE_BUILD_TYPE} ++++++")
    endif(VERBOSE_LEVEL GREATER 99)
else()
    if(VERBOSE_LEVEL GREATER 99)
      message(STATUS "!!!!!! CMAKE_BUILD_TYPE (2) = ${CMAKE_BUILD_TYPE} !!!!!!")
    endif(VERBOSE_LEVEL GREATER 99)
endif()
# message(FATAL_ERROR "Stop !!!!!!!!!!!!!!!!!!!!!!!!!")

set(_INSTALL_COMMAND INSTALL_COMMAND cmake --build . --target install)
### ??? install_step(irgendwas)
### ??? # set(_INSTALL_COMMAND  _install_cmd  )

set(_INSTALL_BIN_DIR "bin/${TOOLCHAIN}")
set(_INSTALL_LIB_DIR "lib/${TOOLCHAIN}")
if(MSVC AND "${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    set(_TARGET_LIBRARY     ${_TARGET_LIB_DEBUG})
    list(APPEND _INSTALL_COMMAND --config Debug)
    string(APPEND _INSTALL_BIN_DIR "d")
    string(APPEND _INSTALL_LIB_DIR "d")
else()
    set(_TARGET_LIBRARY    ${_TARGET_LIB_RELEASE})
    list(APPEND _INSTALL_COMMAND --config Release)
endif()
    set(${TARGET_CNAME}_LIBRARY  ${_TARGET_LIBRARY})
    set(${TARGET_CNAME}_LIBRARY  ${_TARGET_LIBRARY} PARENT_SCOPE)

# if(${VERBOSE_LEVEL} LESS 100)
if(VERBOSE_LEVEL GREATER 99)
  message(STATUS "++++++ 3rd party  = ${lib_name} =================")
  ## message(STATUS "++++++ _TARGET_LIBS         =  ${_TARGET_LIBS}")
  message(STATUS "++++++ _TARGET_LIB_DEBUG   =  ${_TARGET_LIB_DEBUG}")
  message(STATUS "++++++ _TARGET_LIB_RELEASE =  ${_TARGET_LIB_RELEASE}")
endif(VERBOSE_LEVEL GREATER 99)

# -------------------------------------------------
if(EXISTS ${_TARGET_LIBRARY}) 
    set(_COMPLETE_INSTALL OFF)
else()
    set(_COMPLETE_INSTALL ON)
    ##   message(FATAL_ERROR "### PNG: _TARGET_LIBRARY = ${_TARGET_LIBRARY} ")
endif()
#### set(_COMPLETE_INSTALL ON) ## test only (03.11.2024)!!!!!!!!
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
        set_target_properties(${${TARGET_CNAME}_TARGET} PROPERTIES IMPORTED_LOCATION  ${_TARGET_LIBRARY}  GENERATED TRUE)
        set_target_properties(${${TARGET_CNAME}_TARGET} PROPERTIES FOLDER 3rdParty)
        # set_target_properties(${_BUILD_TARGET} PROPERTIES FOLDER 3rdParty)
        add_dependencies(${${TARGET_CNAME}_TARGET}  ${_BUILD_TARGET})
    endif()

if (_COMPLETE_INSTALL)
    set_target_properties(${_BUILD_TARGET} PROPERTIES FOLDER 3rdParty) # either zlib_build or zlib_3rd
endif()
    list(APPEND THIRDPARTY_INCLUDES  ${${TARGET_CNAME}_INCLUDE_DIR})

    list(APPEND 3RDPARTY_TARGETS ${${TARGET_CNAME}_TARGET})

### -> Public: ... PARENT_SCOPE
  set(${TARGET_CNAME}_TARGET_LIBS  ${_TARGET_LIBS} PARENT_SCOPE)
  set(${TARGET_CNAME}_INCLUDE_DIR  ${${TARGET_CNAME}_INCLUDE_DIR} PARENT_SCOPE)
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

    if (DISPLAY_TARGETS)
        foreach(_target ${3RDPARTY_TARGETS} ) 
           message(STATUS "### 3RDPARTY_TARGETS = ${_target}")
        endforeach()
    # message(FATAL_ERROR "SCRIPT_FILES = ${SCRIPT_FILES}")
    endif()
    
set_target_properties(ThirdParty PROPERTIES FOLDER 3rdParty)


if(VERBOSE_LEVEL GREATER 80)
message(STATUS      "!!!! HDF5_LIB_DIR: ${HDF5_LIB_DIR}")
message(STATUS      "!!!! CURL_LIB_DIR: ${CURL_LIB_DIR}")
message(STATUS      "!!!! ------------------------------------------  !!")
message(STATUS      "!!!! HDF5_DIR:     ${HDF5_DIR}")
message(STATUS      "!!!! CURL_DIR:     ${CURL_DIR}")
message(STATUS      "!!!! ZLIB_DIR:     ${ZLIB_DIR}")
message(STATUS      "!!!! ------------------------------------------  !!")
message(STATUS      "!!!! HDF5F_CXX_CMAKE_DIR: ${HDF5_CMAKE_DIR}")
message(STATUS      "!!!! NETCDF_CXX_CMAKE_DIR: ${NETCDF_CXX_CMAKE_DIR}")
endif(VERBOSE_LEVEL GREATER 80)
# message(FATAL_ERROR "!!!! HDF5_DIR: ${LINK_LIBS}/hdf5/hdf5-${HDF5_VERSION}")

