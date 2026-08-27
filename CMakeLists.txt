cmake_minimum_required(VERSION 3.18)

## ----------------------------------------------------------
# command line options
set (VERBOSE_LEVEL 10)
# Features
# set (HAVE_SKYSIGHT ON) # look WinMSVC.cmd, WinMinGW.cmd, ...
set (SHOW_SUBPROJECTS OFF)

if (NOT LINK_LIBS)
  message(STATUS "+++ No LINK_LIBS!!!")
  set (LINK_LIBS /usr/lib/link_libs)  # August2111: this Windows, Flaps6,....
endif (NOT LINK_LIBS)

## ----------------------------------------------------------

message(STATUS "+++ Start CMake ${CMAKE_CURRENT_SOURCE_DIR}!")
message(STATUS "+++ CMAKE_HOST_SYSTEM_NAME:     ${CMAKE_HOST_SYSTEM_NAME}")
message(STATUS "+++ CMAKE_SYSTEM_NAME:          ${CMAKE_SYSTEM_NAME}")
message(STATUS "+++ TOOLCHAIN:                  ${TOOLCHAIN}")
message(STATUS "+++ VERBOSE_LEVEL:             ${VERBOSE_LEVEL}")

get_filename_component(PROJECTGROUP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
get_filename_component(PROJECTGROUP_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR} ABSOLUTE)
message(STATUS "*** PROJECTGROUP_SOURCE_DIR = '${PROJECTGROUP_SOURCE_DIR}'")
message(STATUS "*** PROJECTGROUP_BINARY_DIR = '${PROJECTGROUP_BINARY_DIR}'")

# get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_BINARY_DIR} NAME)
# brand-neutral default; the OpenSoar branding topic switches this to OpenSoar
set(TARGET_NAME XCSoar)
set(APP_NAME ${TARGET_NAME})
set(IS_OPENSOAR OFF)  # ON in the OpenSoar branding topic
set (PROJECT_NAME "${APP_NAME}-${TOOLCHAIN}")
project(${PROJECT_NAME})
set(PROGRAM_NAME ${APP_NAME})
# in test programs PROGRAM_NAME and APP_NAME different:
set(PROGRAM_NAME_CC ${APP_NAME})              # CamelCase
string(TOLOWER  ${PROGRAM_NAME} PROGRAM_NAME_LC) # lowercase
#==========================================
if (NOT CMAKE_BUILD_TYPE) # variable is only set AFTER the project cmd!!!
  if (WIN32 AND MSVC)
    # with MultiConfig at MSVC and empty CMAKE_BUILD_TYPE create the Debug case
    # set(CMAKE_BUILD_TYPE Debug)
    # set(CMAKE_BUILD_TYPE Release)
  else()
    if (MSVC)
        message(FATAL_ERROR "${WIN32} AND '${CMAKE_CXX_COMPILER_ID}', '${MSVC}', '${CMAKE_BUILD_TYPE}'  ")
    endif(MSVC)
    # set(CMAKE_BUILD_TYPE Release)
  endif()
endif()

#==========================================
if (0)
# August2111: Why that????
if (MSVC)    # VisualStudio:
    set(LIB_PREFIX "" )
    set(LIB_SUFFIX ".lib")
    set(EXE_PREFIX ".exe")
elseif(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".a")
    set(EXE_PREFIX ".exe")
elseif (MINGW) # MinGW
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".a")
    set(EXE_PREFIX ".exe")
else()
#     message(FATAL_ERROR "+++ unknown (3rd party-)System: ${CMAKE_SYSTEM}, Compiler = ${CMAKE_CXX_COMPILER_ID} !")
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".a")
    set(EXE_PREFIX "")
endif()
endif() # : 1 or 0
#=========================

# Program name/version: read <APP_NAME>.config if present (branding topic),
# otherwise fall back to the upstream VERSION.txt
if(EXISTS ${PROJECTGROUP_SOURCE_DIR}/${APP_NAME}.config)
  file(READ ${APP_NAME}.config _config_file)
  # put the complete string in a list of lines
  string(REPLACE "\n" ";" _config_file ${_config_file})
  foreach(_line ${_config_file})
    string(REPLACE "=" ";" _line ${_line} )
    list(GET _line 0 _param0)
    list(GET _line 1 _param1)
    string(STRIP ${_param0} _param0)
    string(STRIP ${_param1} _param1)
    set(${_param0} ${_param1})
    # message(STATUS "+++++ '${_param0}' = '${_param1}'")
  endforeach()
else()
  file(READ "VERSION.txt" PROGRAM_VERSION)
  string(STRIP "${PROGRAM_VERSION}" PROGRAM_VERSION)
  set(PROGRAM_NAME ${APP_NAME})
endif()
# CONFIG_FILE is a build input (title graphics, ProgramVersion.h):
# use the source config if present, otherwise generate a neutral one
if(EXISTS ${PROJECTGROUP_SOURCE_DIR}/${APP_NAME}.config)
  set(CONFIG_FILE ${PROJECTGROUP_SOURCE_DIR}/${APP_NAME}.config)
else()
  set(CONFIG_FILE ${PROJECTGROUP_BINARY_DIR}/${APP_NAME}.config)
  file(WRITE ${CONFIG_FILE} "PROGRAM_NAME=${PROGRAM_NAME}\nPROGRAM_VERSION=${PROGRAM_VERSION}\n")
endif()# ---------------------------------------------------------------

message(STATUS "+++++ PROGRAM_NAME = '${PROGRAM_NAME}'")
message(STATUS "+++++ PROGRAM_VERSION = '${PROGRAM_VERSION}'")

add_compile_definitions("PROGRAM_VERSION=\"${PROGRAM_VERSION}\"")
# mirror build/version.mk (VERSION_CPPFLAGS/PRODUCT_NAME_CPPFLAGS):
# src/Version.cpp requires XCSOAR_VERSION, and src/ProductName.hpp can be
# driven by PRODUCT_NAME - upstream's official rebranding hook, which the
# branding topic simply feeds with "OpenSoar" via PROGRAM_NAME
add_compile_definitions("XCSOAR_VERSION=\"${PROGRAM_VERSION}\"")
add_compile_definitions("PRODUCT_NAME=\"${PROGRAM_NAME}\"")
string(TOLOWER "${PROGRAM_NAME}" _PROGRAM_NAME_LC)
add_compile_definitions("PRODUCT_NAME_LC=\"${_PROGRAM_NAME_LC}\"")
file(WRITE "${PROJECTGROUP_BINARY_DIR}/VERSION.txt" ${PROGRAM_VERSION})

# SteFly device family (RemoteStick / RotaryPanel) incl. startup-time
# USB discovery; mirrors HAVE_REMOTESTICK in build/driver.mk.  The
# discovery code has per-platform backends (SetupAPI / libudev) and a
# no-op fallback, so the define is safe on every target.
option(HAVE_REMOTESTICK "SteFly RemoteStick/RotaryPanel drivers" ON)
if (HAVE_REMOTESTICK)
    add_compile_definitions(HAVE_REMOTE_STICK)
endif()

# use the actually running cmake instead of a hardcoded path
set(CMAKE_APP ${CMAKE_COMMAND})
get_filename_component(CMAKE_PROGRAM_PATH ${CMAKE_APP} DIRECTORY)
get_filename_component(CMAKE_PROGRAM_PATH ${CMAKE_PROGRAM_PATH} DIRECTORY)
if (DEFINED ENV{XCSOAR_PROGRAM_DIR})
  file(TO_CMAKE_PATH "$ENV{XCSOAR_PROGRAM_DIR}" USER_PROGRAM_PATH)
else()
  # parent dir of the cmake installation, e.g. D:/Programs or C:/Program Files
  get_filename_component(USER_PROGRAM_PATH ${CMAKE_PROGRAM_PATH} DIRECTORY)
endif()
  message(STATUS "CMAKE_SYSTEM_PROGRAM_PATH - ${CMAKE_SYSTEM_PROGRAM_PATH}")
  message(STATUS "CMAKE_APP                 - ${CMAKE_APP}")
  message(STATUS "CMAKE_PROGRAM_PATH        - ${CMAKE_PROGRAM_PATH}")
  message(STATUS "USER_PROGRAM_PATH         - ${USER_PROGRAM_PATH}")

include(build/cmake/xcsoar.cmake)
get_filename_component(OUTPUT_FOLDER ${PROJECTGROUP_BINARY_DIR}/.. ABSOLUTE)
set(OUTPUT ${OUTPUT_FOLDER})

# 2021 clear the right place:
set(JASPER_OUTSIDE      OFF)
set(ZZIP_OUTSIDE        OFF)

add_definitions(-DCMAKE_PROJECT)  # to decide if make or cmake...

# find all necessary programs/apps
find_program(PYTHON_APP         NAMES python3 python REQUIRED)
find_program(PERL_APP           NAMES perl   REQUIRED)
# find_program(BMP_CONVERT_APP    NAMES ImageMagick/convert )
    list(APPEND CMAKE_PROGRAM_PATH "${USER_PROGRAM_PATH}/GnuWin/libxslt/bin")
find_program(XSLTPROC_APP    NAMES xsltproc REQUIRED)
    list(APPEND CMAKE_PROGRAM_PATH "${USER_PROGRAM_PATH}/Inkscape/bin")
find_program(INKSCAPE_APP    NAMES inkscape REQUIRED)
    list(APPEND CMAKE_PROGRAM_PATH "${USER_PROGRAM_PATH}/ImageMagick")
    # find versioned standard installs like "ImageMagick-7.1.1-Q16-HDRI"
    file(GLOB _im_dirs "${USER_PROGRAM_PATH}/ImageMagick*")
    list(APPEND CMAKE_PROGRAM_PATH ${_im_dirs})
# ImageMagick 7 ships magick.exe; the classic convert.exe/montage.exe
# are optional legacy tools. NEVER fall back to Windows' System32
# convert.exe - that is the FAT->NTFS converter ("Unzulaessiger
# Parameter"...)!
find_program(MAGICK_APP         NAMES magick)
if (MAGICK_APP)
    set(BMP_CONVERT_APP ${MAGICK_APP})
    set(BMP_MONTAGE_APP ${MAGICK_APP} montage)
else()
    find_program(BMP_CONVERT_APP    NAMES convert REQUIRED)
    if (WIN32 AND BMP_CONVERT_APP MATCHES "[Ss]ystem32")
        message(FATAL_ERROR "found Windows' System32/convert.exe instead of "
            "ImageMagick - please install ImageMagick (magick.exe) or set "
            "BMP_CONVERT_APP")
    endif()
    find_program(BMP_MONTAGE_APP    NAMES montage REQUIRED)
endif()
    list(APPEND CMAKE_PROGRAM_PATH "${USER_PROGRAM_PATH}/7-Zip")
find_program(ZIP_APP            NAMES 7z gzip REQUIRED)  # REQUIRED since CMake 3.18!

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set (PERCENT_CHAR "\%")
    set (DOLLAR_CHAR "$$")
#    message(FATAL_ERROR "Stop: Clang Compiler??? - '${CMAKE_CXX_COMPILER}'")
elseif(CMAKE_CXX_COMPILER MATCHES "clang[+][+]")
    set (PERCENT_CHAR "\%")
    set (DOLLAR_CHAR "$$")
    set(CLANG ON)
elseif (WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
    message(STATUS "MSVC-Compiler - ${CMAKE_CXX_COMPILER_ID}")
    set (PERCENT_CHAR "%%")
    set (DOLLAR_CHAR "\$")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "GCC-Compiler - ${CMAKE_CXX_COMPILER_ID}")
    set (PERCENT_CHAR "%%")
    set (DOLLAR_CHAR "$$")
else()
  ### message(FATAL_ERROR "GDI-Stop!  in ./CmakeLists")
  message(FATAL_ERROR "Stop: unknown Compiler??? - '${CMAKE_CXX_COMPILER}'")
endif()

add_subdirectory(build/cmake)

if (TARGET_IS_OPENVARIO)
    set(DEFAULT_XCI_FILE   defaultOV.xci)
else()
    set(DEFAULT_XCI_FILE   default.xci)
endif()

if(UNIX)
 set(ENABLE_OPENGL ON)  # better outside????
  # moved to build/cmake/LinuxGCC.cmake
elseif (MSVC OR 1)  # 1 = always..
# if (MSVC AND 0)  # 0 = never..
 # Windows is ALWAYS the OpenGL flavor (upstream TARGET=WIN64OPENGL:
 # OPENGL=y + ENABLE_SDL=y + USE_ANGLE=y, GLES2 via D3D11).  The GDI
 # flavor has been retired from the CMake build system.
 set(XCSOAR_USE_OPENGL ON)
 set(ENABLE_OPENGL ON)
 set(ENABLE_SDL ON)
 set(USE_ANGLE ON)
 set(USE_FREETYPE ON)   # -> 3rd_party/freetype.cmake, sdl2.cmake
 # feature switches as ui/CMakeSource.cmake expects them (make names):
 # upstream sdl.mk sets LIBPNG=y LIBJPEG=y, libtiff.mk TIFF=GEOTIFF=y (PC)
 set(FREETYPE ON)
 set(LIBPNG ON)
 set(LIBJPEG ON)
 set(TIFF ON)
 set(USE_MEMORY_CANVAS OFF)  # das ist hier auch falsch!!!!
else()
 set(ENABLE_OPENGL ON)  # better outside????
 set(ENABLE_SDL OFF)  # better outside????
 set(USE_MEMORY_CANVAS OFF)  # das ist hier auch falsch!!!!
endif()

#---------------------------------------------------------
if (USE_MEMORY_CANVAS)
       add_compile_definitions(USE_MEMORY_CANVAS)
elseif(ENABLE_OPENGL)
       add_compile_definitions(ENABLE_OPENGL)
       # add_compile_definitions(ENABLE_SDL)  # only windows????
       # add_compile_definitions(USE_FREETYPE)  # only windows????
else()
      message(FATAL_ERROR "the GDI flavor is no longer supported by the "
          "CMake build - Windows builds are always OpenGL (msvc2026gl)")
endif()
# WorkAround to enable Internet every time
# moved in WinXXX.cmake: add_compile_definitions(HAVE_HTTP)
#---------------------------------------------------------

set_property(GLOBAL PROPERTY USE_FOLDERS ON)  # important for folder structure f.e. in Visual Studio
# Setting Area ===================================================
include(ExternalProject)  # possible since 3.10

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # only by Makefile or Ninja generators: export compile commands
set(XCSOAR_LIB_TYPE STATIC)  # SHARED)

if (CMAKE_TOOLCHAIN_FILE)
  message(STATUS "CMAKE_TOOLCHAIN_FILE = '${CMAKE_TOOLCHAIN_FILE}'")
  message(STATUS "CMAKE_SYSTEM_NAME = '${CMAKE_SYSTEM_NAME}'")
endif()

if(ANDROID OR KOBO OR SDL)
    add_compile_definitions(HAVE_BATTERY)
endif()

# NOTE: APP_NAME deliberately does NOT depend on the checkout directory
# name (the old  get_filename_component(APP_NAME ...)  silently rebranded
# the build depending on where the repo was cloned!).  APP_NAME is set
# once at the top of this file - the branding topic switches it there.
  
message(STATUS "CMAKE_HOST_SYSTEM_NAME = ${CMAKE_HOST_SYSTEM_NAME} vs. CMAKE_SYSTEM_NAME = ${CMAKE_SYSTEM_NAME} ")

set(SRC "${PROJECTGROUP_SOURCE_DIR}/src")
if (CMAKE_INCLUDE_SYSTEM)
  include(${CMAKE_INCLUDE_SYSTEM})  # build/cmake/LinuxMinGW.cmake)
elseif (WIN32 AND MSVC)
   include(build/cmake/WinMSVC.cmake)
 elseif(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
   include(build/cmake/WinClang.cmake)
   set(CLANG ON)
 elseif(WIN32 AND (CMAKE_CXX_COMPILER MATCHES "clang[+][+]"))
   message(FATAL_ERROR "+++ Unknown System: CLANG-1!")
   include(build/cmake/WinClang.cmake)
   set(CLANG ON)
 elseif(${CMAKE_HOST_SYSTEM_NAME}' MATCHES 'Linux') # August2111: Test only
   include(build/cmake/LinuxMinGW.cmake)
 elseif(WIN32 AND MINGW AND (${CMAKE_HOST_SYSTEM_NAME}' MATCHES 'Linux'))
   include(build/cmake/LinuxMinGW.cmake)
 elseif(WIN32 AND MINGW)
   include(build/cmake/WinMinGW.cmake)
 elseif(WIN32 AND CLANG)
    message(FATAL_ERROR "+++ Unknown System: CLANG!")
 elseif(WIN32 AND NINJA)
    message(FATAL_ERROR "+++ Unknown System: NINJA!")
 elseif(UNIX)
   message(STATUS "+++ System = LINUX / GCC(?) (${TOOLCHAIN})!")
   include(build/cmake/LinuxGCC.cmake)
 elseif(ANDROID)
     message(STATUS "+++ System = ANDROID / ${ANDROID_ABI}!")
 else()
   message(FATAL_ERROR "+++ Unknown System: ${CMAKE_SYSTEM}!")
endif()

if (MSVC)  # Themes for MSVC only!
    add_compile_definitions(GNU_CONST=)
    add_compile_definitions(GNU_PURE=)
else()   ### MSVC
    add_compile_definitions("GNU_CONST=[[gnu::const]]")
    add_compile_definitions("GNU_PURE=[[gnu::pure]]")
endif()  ### MSVC


if (MINGW)
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=stringop-truncation")  # remove this with solving!
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=maybe-uninitialized")  # remove this with solving!
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=strict-aliasing")      # remove this with solving!
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=return-local-addr")    # remove this with solving!
endif(MINGW)

# CMAKE_BUILD_TYPE arrives from the runner (XCSOAR_CONFIG); default to
# Debug only when it is really unset.
# NOTE the old check here was  if(CMAKE_BUILD_TYPE MATCHES "")  - the
# empty regex matches EVERY string, so this silently forced Debug on
# every configure and no Release third-party lib was ever built
if (NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE Debug)
endif()

if (CMAKE_BUILD_TYPE MATCHES "Debug")
  message(STATUS "+++ Debug build: CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
else()
  message(STATUS "+++ Release build: CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")
endif()

# git commit hash macro
execute_process(
  COMMAND  git rev-parse --short HEAD
  WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_COMMIT_ID
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
add_compile_definitions("GIT_COMMIT_ID=\"${GIT_COMMIT_ID}\"")

if (HAVE_SKYSIGHT)
  add_compile_definitions(HAVE_SKYSIGHT)
  add_compile_definitions(SKYSIGHT_LIVE) 
  if (SKYSIGHT_FORECAST)
    add_compile_definitions(SKYSIGHT_FORECAST)
  endif (SKYSIGHT_FORECAST)
  if (SKYSIGHT_FILE_DEBUG)
    add_compile_definitions(SKYSIGHT_FILE_DEBUG)
  endif (SKYSIGHT_FILE_DEBUG)
  if (SKYSIGHT_OFFLINE_MODE)
    add_compile_definitions(SKYSIGHT_OFFLINE_MODE)
  endif (SKYSIGHT_OFFLINE_MODE)
  if (SKYSIGHT_REQUEST_LOG)
    add_compile_definitions(SKYSIGHT_REQUEST_LOG)
  endif (SKYSIGHT_REQUEST_LOG)
  if (SKYSIGHT_HTTP_LOG)
    add_compile_definitions(SKYSIGHT_HTTP_LOG)
  endif (SKYSIGHT_HTTP_LOG)
else (HAVE_SKYSIGHT)
  ###   message(FATAL_ERROR "Stop!  No SkySight")
  message(STATUS " -> No SkySight")
endif (HAVE_SKYSIGHT)

set(HAVE_REMOTESTICK ON)
if (HAVE_REMOTESTICK)
    add_compile_definitions(HAVE_REMOTE_STICK)

# libudev for Linux USB / serial hotplug detection (PortMonitorLinux).
# On non-Linux platforms the .cpp is an empty translation unit, so no
# library is needed.
#
# We try pkg-config first (the usual case); if that fails we fall back
# to a plain find_library / find_path search so that systems without
# pkg-config or without a libudev.pc still get detected as long as
# libudev.so + libudev.h are installed somewhere standard.
if (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  if (PkgConfig_FOUND)
    pkg_check_modules(LIBUDEV QUIET libudev)
  endif()

  if (NOT LIBUDEV_FOUND)
    find_path(LIBUDEV_INCLUDE_DIRS NAMES libudev.h)
    find_library(LIBUDEV_LIB        NAMES udev)
    if (LIBUDEV_INCLUDE_DIRS AND LIBUDEV_LIB)
      set(LIBUDEV_FOUND TRUE)
      set(LIBUDEV_LIBRARIES ${LIBUDEV_LIB})
      message(STATUS "libudev: pkg-config miss, but found header (${LIBUDEV_INCLUDE_DIRS}) and library (${LIBUDEV_LIB})")
    endif()
  endif()

  if (LIBUDEV_FOUND)
    add_compile_definitions(HAVE_LIBUDEV)
    message(STATUS "libudev found: hotplug monitor enabled")
    # NOTE: link LIBUDEV_LIBRARIES into the Device target — done where
    # the Device target is defined.
  else()
    message(STATUS "libudev not found: hotplug monitor disabled "
                   "(install libudev-dev + pkg-config, or set "
                   "LIBUDEV_INCLUDE_DIRS / LIBUDEV_LIB manually)")
  endif()
endif()
endif (HAVE_REMOTESTICK)

include(CMakeSource.cmake)
set(3RDPARTY_DEPENDENCIES ON)
set(3RDPARTY_TARGETS )
add_subdirectory(3rd_party)
add_subdirectory(Data)
add_subdirectory(po)

message(STATUS "+++ Lib Pre- and Suffix: '${LIB_PREFIX}' -- '${LIB_SUFFIX}' !")


include_directories("${SRC}/io") # only regarding Logfile!
include_directories("${SRC}/lib/fmt") # ???!
# include_directories("${OUTPUT_FOLDER}/include")

#3rd party components
add_compile_definitions(CURL_STATICLIB)

# 3rd Party !!!!
#=============================
message(STATUS "### THIRDPARTY_INCLUDES = '${THIRDPARTY_INCLUDES}'")
include_directories(${THIRDPARTY_INCLUDES} )
#####################################################################################
#####################################################################################
add_compile_definitions(NOMINMAX)

if(ENABLE_OPENGL AND WIN32)
    # ==================================================================
    # OpenGL ES 2.0 via ANGLE (upstream: build/opengl.mk + build/angle.mk)
    # prebuilt DLLs from https://github.com/XCSoar/angle-libs; fetched and
    # completed with import libraries by tools/python/fetch_angle.py
    # (Python counterpart of windows/fetch-angle-from-github.sh):
    #   ${LINK_LIBS}/angle/include  GLES2/, GLES3/, EGL/, KHR/
    #   ${LINK_LIBS}/angle/bin      libGLESv2.dll, libEGL.dll
    #   ${LINK_LIBS}/angle/lib      libGLESv2.lib/.dll.a, libEGL.lib/.dll.a
    # ==================================================================
    set(ANGLE_DIR ${LINK_LIBS}/angle)
    if(MSVC)
        set(_ANGLE_TOOLSET msvc)
        set(_ANGLE_IMPLIB ${ANGLE_DIR}/lib/libGLESv2.lib)
    else()
        set(_ANGLE_TOOLSET mingw)
        set(_ANGLE_IMPLIB ${ANGLE_DIR}/lib/libGLESv2.dll.a)
    endif()
    if(NOT EXISTS ${_ANGLE_IMPLIB})
        message(STATUS "ANGLE not found - fetching into ${ANGLE_DIR} ...")
        execute_process(
            COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/fetch_angle.py
                    ${ANGLE_DIR} --arch x64 --toolset ${_ANGLE_TOOLSET}
            RESULT_VARIABLE _angle_rc)
        if(NOT _angle_rc EQUAL 0 OR NOT EXISTS ${_ANGLE_IMPLIB})
            message(FATAL_ERROR "fetching ANGLE failed (rc=${_angle_rc}) - "
                "run manually: python tools/python/fetch_angle.py ${ANGLE_DIR} "
                "--toolset ${_ANGLE_TOOLSET}")
        endif()
    endif()
    include_directories(SYSTEM ${ANGLE_DIR}/include)
    link_directories(${ANGLE_DIR}/lib)
    if(MSVC)
        list(APPEND BASIC_LINK_LIBRARIES libGLESv2.lib libEGL.lib)
    else()
        list(APPEND BASIC_LINK_LIBRARIES GLESv2 EGL)
    endif()

    # GLES2 feature set, exactly like upstream OPENGL_CPPFLAGS
    add_compile_definitions(HAVE_GLES HAVE_GLES2 GL_GLEXT_PROTOTYPES USE_ANGLE)
    # GLM (header-only, in-tree since upstream uses lib/glm).
    # lib/glm is a git SUBMODULE - a fresh clone or a git worktree
    # starts with it empty, so initialise it on demand
    if(NOT EXISTS ${PROJECTGROUP_SOURCE_DIR}/lib/glm/glm/mat4x4.hpp)
        message(STATUS "initialising git submodule lib/glm ...")
        execute_process(
            COMMAND git -C ${PROJECTGROUP_SOURCE_DIR}
                    submodule update --init --depth 1 lib/glm
            RESULT_VARIABLE _glm_rc)
        if(NOT _glm_rc EQUAL 0 OR
           NOT EXISTS ${PROJECTGROUP_SOURCE_DIR}/lib/glm/glm/mat4x4.hpp)
            message(FATAL_ERROR "lib/glm submodule missing - run: "
                "git submodule update --init lib/glm")
        endif()
    endif()
    include_directories(SYSTEM ${PROJECTGROUP_SOURCE_DIR}/lib/glm)
    add_compile_definitions(GLM_FORCE_PURE)

    # SDL2 + FreeType are built like every other dependency by
    # 3rd_party/sdl2.cmake and 3rd_party/freetype.cmake (ExternalProject,
    # installed to ${LINK_LIBS}/<lib>/<lib>-<version>/lib/<toolchain>[d]);
    # their targets/includes arrive via SDL2_TARGET / FREETYPE_TARGET and
    # THIRDPARTY_INCLUDES after add_subdirectory(3rd_party)
    add_compile_definitions(ENABLE_SDL USE_FREETYPE)
    add_compile_definitions(USE_LIBTIFF USE_GEOTIFF)   # libtiff.mk (PC)
    # NO USE_WIN32_RESOURCES: upstream sets it only for
    # HAVE_WIN32 && !ENABLE_SDL (build/resource.mk) - the GL/SDL build
    # loads its resources from bin2c'ed data instead of the .rc
endif()
#####################################################################################
##################################################################################### 

### Boost Section =============================

set(Boost_INCLUDE_DIR ${BOOST_INCLUDE_DIR})
### set(Boost_ROOT ${BOOST_ROOT})
# link_directories isn't necessary because header only...
# link_directories(${BOOST_ROOT}/lib/${TOOLCHAIN}) # this line in boost.cmake is not affected to the main project!

# message(FATAL_ERROR "${BOOST_ROOT} /// ${BOOST_INCLUDE_DIR} /// ${BOOST_TARGET_LIBS}" )

### find_package(Boost REQUIRED)
# add_compile_definitions(BOOST_ASIO_SEPARATE_COMPILATION)
# add_compile_definitions(BOOST_MATH_DISABLE_DEPRECATED_03_WARNING=ON) 

# following libs need this settings: Dialog, lib${APP_NAME}, json, WeGlide, Weather, net, Repository, Task,  ...
add_compile_definitions(BOOST_JSON_NO_LIB)
add_compile_definitions(BOOST_CONTAINER_NO_LIB)
#  ??
# add_compile_definitions(BOOST_JSON_HEADER_ONLY)
# add_compile_definitions(BOOST_JSON_STAND_ALONE)

# include_directories(${Boost_INCLUDE_DIR}/boost/compute/interop)
#End of Boost Section =============================


get_filename_component(TEMP_INCLUDES ${PROJECTGROUP_BINARY_DIR}/../include ABSOLUTE)
include_directories(
    ${PROJECTGROUP_SOURCE_DIR}/src
    ${PROJECTGROUP_SOURCE_DIR}/src/Engine
    ${TEMP_INCLUDES}
)

if (JASPER_OUTSIDE)  # 2021
    # before add_subdirectories!!!
    add_compile_definitions(JASPER_OUTSIDE)
endif (JASPER_OUTSIDE)

set(SOURCE_FILES )  # clean it
    add_subdirectory(src/Airspace)
    add_subdirectory(src/Atmosphere)
    add_subdirectory(src/Audio)
    add_subdirectory(src/Blackboard)
    add_subdirectory(src/co)
    add_subdirectory(src/Computer)
if (OFF) ## - 'Cloud'- files are not used yet
   add_subdirectory(src/Cloud)
endif()
    add_subdirectory(src/CrossSection)
    add_subdirectory(src/Engine)
    add_subdirectory(src/event)
    add_subdirectory(src/FLARM)
    add_subdirectory(src/Form)
    add_subdirectory(src/Formatter)
    add_subdirectory(src/Gauge)
    add_subdirectory(src/Geo)
    add_subdirectory(src/Hardware)
    add_subdirectory(src/IGC)
    add_subdirectory(src/InfoBoxes)
    add_subdirectory(src/Input)
    add_subdirectory(src/Job)
    add_subdirectory(src/json)
    add_subdirectory(src/Language)
    add_subdirectory(src/Logger)
    add_subdirectory(src/Look)
    add_subdirectory(src/lua)
    add_subdirectory(src/Markers)
    add_subdirectory(src/Math)
    add_subdirectory(src/Menu)
    add_subdirectory(src/Monitor)
    add_subdirectory(src/net)
    add_subdirectory(src/NOTAM)
    add_subdirectory(src/Storage)
    add_subdirectory(src/NMEA)
    add_subdirectory(src/Operation)
    add_subdirectory(src/system)
    add_subdirectory(src/Plane)
    add_subdirectory(src/Polar)
    add_subdirectory(src/Profile)
    add_subdirectory(src/Projection)
    add_subdirectory(src/Renderer)
    add_subdirectory(src/Replay)
    add_subdirectory(src/Repository)
    add_subdirectory(src/Screen)
    add_subdirectory(src/Task)
    add_subdirectory(src/TeamCode)
    add_subdirectory(src/Terrain)
# if (NOT JASPER_OUTSIDE)  # 2021
    add_subdirectory(src/Terrain/jasper)  # 2021
# endif (NOT JASPER_OUTSIDE)
    add_subdirectory(src/thread)
    add_subdirectory(src/time)
    add_subdirectory(src/Topography)
    add_subdirectory(src/ui    )
    add_subdirectory(src/UIUtil)
    add_subdirectory(src/Units)
    add_subdirectory(src/util)
    add_subdirectory(src/Waypoint)
    add_subdirectory(src/Weather)
    add_subdirectory(src/Widget)
    add_subdirectory(src/XML)

    add_subdirectory(src/Device)
    add_subdirectory(src/Dialogs)
    add_subdirectory(src/io)
    add_subdirectory(src/MapWindow)
    add_subdirectory(src/Tracking)
    add_subdirectory(src/net/client/WeGlide)
if(NOT ZZIP_OUTSIDE)
    add_subdirectory(src/zzip)   # aug: new!!
endif()
add_subdirectory(src/lib)
add_subdirectory(src)   # lib${APP_NAME}!

# OpenVarioBaseMenu (standalone tool for the OpenVario flight computer,
# src/OpenVario/) - opt-in: it is not needed for the normal app build
option(WITH_OPENVARIO_BASEMENU "build the OpenVarioBaseMenu tool" OFF)
if (WITH_OPENVARIO_BASEMENU OR TARGET_IS_OPENVARIO)
    add_subdirectory(src/OpenVario)
endif()

add_subdirectory(src/Android)  # Android  -> EXCLUDE_FROM_ALL TRUE

# the upstream main source file is XCSoar.cpp regardless of branding;
# only the executable NAME follows APP_NAME
list(APPEND SOURCE_FILES "src/XCSoar.cpp")
list(APPEND SOURCE_FILES  ${RESOURCE_FILES})

list(APPEND SCRIPT_FILES "CMakeSource.cmake")

if (MSVC)
  file(GLOB DEF_FILE "Data/*.def")
  if (DEF_FILE)
    list(APPEND SOURCE_FILES ${DEF_FILE})
  endif()
endif()
#==========================================================
#==========================================================

# preparing target_link_libraries!
list(APPEND XCSOAR_LINK_LIBRARIES  lib${APP_NAME} ${XCSOAR_SOURCE_LISTS})

# TODO(aug): Cleaning up this code!!!!  
if(1)
  message(STATUS "Boost:        BOOST_LIBS       =  ${Boost_LIBRARIES} ")  # BOOST_LIB ???
  message(STATUS "C-Ares:       CARES_LIBS       =  ${CARES_LIBS}     / ${CARES_TARGET} ")
  message(STATUS "Curl:         CURL_LIBS        =  ${CURL_LIBS}      / ${CURL_TARGET} ")
if (JASPER_OUTSIDE)
  message(STATUS "Jasper:       JASPER_LIBS      =  ${JASPER_LIBS}    / ${JASPER_TARGET} ")
endif()
  message(STATUS "Png:          PNG_LIBS         =  ${PNG_LIBS}       / ${PNG_TARGET} ")
  message(STATUS "Sodium:       SODIUM_LIBS      =  ${SODIUM_LIBS}    / ${SODIUM_TARGET} ")
  message(STATUS "Lua:          LUA_LIBS         =  ${LUA_LIBS}       / ${LUA_TARGET} ")
  message(STATUS "ZLib:         ZLIB_LIBS        =  ${ZLIB_LIBS}      / ${ZLIB_TARGET} ")
if (ZZIP_OUTSIDE)
  message(STATUS "ZZip:         ZZIP_LIBS        =  ${ZZIP_LIBS}      / ${ZZIP_TARGET} ")
endif()
  message(STATUS "Fmt:          FMT_LIBS         =  ${FMT_LIBS}       / ${FMT_TARGET} ")
  message(STATUS "tiff:         TIFF_LIBS        =  ${TIFF_LIBS}      / ${TIFF_TARGET} ")
  message(STATUS "geotiff:      GEOTIFF_LIBS     =  ${GEOTIFF_LIBS}   / ${GEOTIFF_TARGET} ")
  message(STATUS "sqlite3:      SQLITE3_LIBS     =  ${SQLITE3_LIBS}   / ${SQLITE3_TARGET} ")
  ### message(STATUS "szip:         SZIP_LIBS        =  ${SZIP_LIBS}      / ${SZIP_TARGET} ")
if (HAVE_SKYSIGHT)
  message(STATUS "proj:         PROJ_LIBS        =  ${PROJ_LIBS}      / ${PROJ_TARGET} ")
  message(STATUS "netcdf-c:     NETCDF_C_LIBS    =  ${NETCDF_C_LIBS}  / ${NETCDF_C_TARGET} ")
  message(STATUS "netcdf-cxx:   NETCDF_CXX_LIBS  =  ${NETCDF_CXX_LIBS} / ${NETCDF_CXX_TARGET} ")
endif()
  ### message(STATUS "hdf5:         HDF5_LIBS        =  ${HDF5_LIBS}      / ${HDF5_TARGET} ")
# message(FATAL_ERROR "Stop: XCSOAR_LINK_LIBRARIES")
endif()

## set(LIBS_OR_LIBRARY ON)
  ## message(FATAL_ERROR "Stop: Test Library 5")
  ## list(APPEND XCSOAR_LINK_LIBRARIES
  ## list(APPEND 3RDPARTY_LINK_LIBRARIES
set(3RDPARTY_LINK_LIBRARIES
    ${LUA_TARGET}
    ${CURL_TARGET}
    ${FMT_TARGET}
    #   ${MAPSERVER_TARGET}
    ${PNG_TARGET}     # prepare_3rdparty(png ...) -> PNG_TARGET (LIBPNG_TARGET was empty)
    ${ZLIB_TARGET}
    ${ZZIP_TARGET}    # internal or external...
    ${SODIUM_TARGET}  # new at 06/2020
    # SYSTEMLIBS: ${SSL_LIBS}  # new at 03/2021
    # SYSTEMLIBS: ${CRYPTO_LIBS}  # new at 03/2021
    ${CARES_TARGET}  # new at 03/2021
    # SYSTEMLIBS: ${CARES_ADD_LIBS}  # new at 12/2025
    
    ${TIFF_TARGET}  # new at 06/2024

    # ${Boost_LIBRARIES}
)
if (ENABLE_SDL)      # OpenGL flavor
    list(APPEND 3RDPARTY_LINK_LIBRARIES ${SDL2_TARGET} ${SDL2_SYSTEM_LIBS} ${JPEG_TARGET})
endif()
if (USE_FREETYPE)    # OpenGL flavor
    list(APPEND 3RDPARTY_LINK_LIBRARIES ${FREETYPE_TARGET})
endif()
if (HAVE_SKYSIGHT)
    ## message(FATAL_ERROR "Stop: Test Library 6")
    ## list(APPEND XCSOAR_LINK_LIBRARIES
    ## list(APPEND SKYSIGHT_LINK_LIBRARIES
    set(SKYSIGHT_LINK_LIBRARIES
      # ${SZIP_TARGET}  # new at 06/2024
      ## 2026-01-02 removed: ${SZIP_LIBS}  # new at 06/2024
      ${GEOTIFF_TARGET}  # new at 06/2024
      ${SQLITE3_TARGET}  # new at 11/2024
      # ${SQLITE3_LIBRARY}  # new at 11/2024
      ## 2026-01-02 removed: ${HDF5_TARGET}  # new at 06/2024
      ## 2026-01-02 removed:  ${HDF5_HL_LIBRARY} 
      ${PROJ_TARGET}  # new at 06/2024
      ${NETCDF_C_TARGET}  # new at 06/2024
      ${NETCDF_CXX_TARGET}  # new at 06/2024
)
endif(HAVE_SKYSIGHT)

set(SYSTEM_LINK_TARGETS
      ${SSL_LIBS}  # new at 03/2021
      ${CRYPTO_LIBS}  # new at 03/2021
      ${CARES_ADD_LIBS}  # new at 12/2025  (MinGW only?)
)
message(STATUS "+++: Test Library - XCSOAR_LINK_LIBRARIES:   ${XCSOAR_LINK_LIBRARIES} ")
message(STATUS "+++: Test Library - 3RDPARTY_LINK_LIBRARIES: ${3RDPARTY_LINK_LIBRARIES} ")
message(STATUS "+++: Test Library - SKYSIGHT_LINK_LIBRARIES: ${SKYSIGHT_LINK_LIBRARIES} ")
message(STATUS "+++: Test Library - SYSTEM_LINK_TARGETS:     ${SYSTEM_LINK_TARGETS} ")
  ## message(FATAL_ERROR "Stop: Test Library 5")
list(APPEND XCSOAR_LINK_LIBRARIES ${3RDPARTY_LINK_LIBRARIES} ${SKYSIGHT_LINK_LIBRARIES} ${SYSTEM_LINK_TARGETS})

if (WIN32)  # Windows only:
    # BASIC_LINK_LIBRARIES from Toolchain
    list(APPEND XCSOAR_LINK_LIBRARIES ${BASIC_LINK_LIBRARIES})
endif()
  #==========================================================
  #==========================================================

list(APPEND SCRIPT_FILES
    CMakeSource.cmake
    ${PROJECTGROUP_BINARY_DIR}/VERSION.txt
    NEWS.txt
)
# optional, IDE-visible files - only if they exist (branding topic adds them)
foreach(_opt_file ${APP_NAME}.config OpenSoar-News.md OpenSoar-AddOn.md)
  if(EXISTS ${PROJECTGROUP_SOURCE_DIR}/${_opt_file})
    list(APPEND SCRIPT_FILES ${_opt_file})
  endif()
endforeach()

if (MSVC)
  list(APPEND SCRIPT_FILES ${PROJECTGROUP_SOURCE_DIR}/ide/msvc/OpenSoar.vcxproj.user.in)
endif()

if (ANDROID)  # library!
   add_library(${APP_NAME} STATIC ${SOURCE_FILES})
elseif(MINGW)
   add_executable(${APP_NAME}     ${SOURCE_FILES} ${SCRIPT_FILES} ${EXT_PROJ})
 # add_executable(${APP_NAME}     ${SOURCE_FILES} ${SCRIPT_FILES} ${XCSOAR_LINK_LIBRARIES} ${EXT_PROJ})
else()        # executable!
   message(STATUS  "### add_executable(${APP_NAME}     ${SOURCE_FILES} ${SCRIPT_FILES} ${EXT_PROJ}")
   add_executable(${APP_NAME}     ${SOURCE_FILES} ${SCRIPT_FILES} ${EXT_PROJ})
endif()

list(APPEND XCSOAR_LINK_LIBRARIES ) # sequence is very important, I don't no, why'!!!!

if (ENABLE_OPENGL AND WIN32 AND ANGLE_DIR)
    # the app needs libEGL.dll + libGLESv2.dll next to the executable
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${ANGLE_DIR}/bin/libEGL.dll ${ANGLE_DIR}/bin/libGLESv2.dll
                $<TARGET_FILE_DIR:${APP_NAME}>
        COMMENT "copy ANGLE DLLs")
endif()
target_include_directories(${APP_NAME} PRIVATE ${Boost_INCLUDE_DIRS})
if (ENABLE_SDL AND WIN32)
  # Data/<AppName>Icon.rc references its .ico relative to Data/
  set_source_files_properties(${PROJECTGROUP_SOURCE_DIR}/Data/${APP_NAME}Icon.rc
      PROPERTIES INCLUDE_DIRECTORIES ${PROJECTGROUP_SOURCE_DIR}/Data)
endif()

if (VERBOSE_LEVEL)
    message(STATUS "----- XCSOAR_LINK_LIBRARIES = ${XCSOAR_LINK_LIBRARIES}")
    foreach(libs ${XCSOAR_LINK_LIBRARIES})
      message(STATUS "+++ XCSOAR_LINK_LIBRARIES: ${libs}")
    endforeach()
    if (VERBOSE_LEVEL GREATER 100)
       message(FATAL_ERROR "Stop =================")
    endif()
endif()

# target_link_libraries(${APP_NAME} PUBLIC ${XCSOAR_LINK_LIBRARIES})
if (HAVE_SKYSIGHT)
#   target_link_libraries(${APP_NAME} PUBLIC ${SQLITE3_LIBRARY})
#  target_link_libraries(${PROJ_TARGET} INTERFACE ${SQLITE3_LIBRARY} ${CURL_LIBRARY})
if (MINGW)
# August2111: geht bei MSVC nicht... ;-(
  target_link_libraries(${PROJ_TARGET} INTERFACE ${SQLITE3_LIBRARY})
  target_link_libraries(${GEOTIFF_TARGET} ) ## 2026/01/06
  target_link_libraries(${NETCDF_C_TARGET} ) ## 2026/01/06:INTERFACE ${HDF5_LIBRARY} ${HDF5_HL_LIBRARY})
  target_link_libraries(${NETCDF_CXX_TARGET} INTERFACE ${NETCDF_C_LIBRARY})
endif (MINGW)
if (CLANG)
  # target_link_libraries(${GEOTIFF_TARGET} ) ## 2026/01/06
endif(CLANG)
#  target_link_libraries(${PROJ_LIBRARY} PRIVATE ${SQLITE3_LIBRARY})
# target_link_libraries(${APP_NAME} PUBLIC ${HDF5_LIBRARY})
# target_link_libraries(${APP_NAME} PUBLIC ${HDF5_HL_LIBRARY})
endif(HAVE_SKYSIGHT)

# message(FATAL_ERROR "XCSOAR_LINK_LIBRARIES = ${XCSOAR_LINK_LIBRARIES}")
target_link_libraries(${APP_NAME} PRIVATE ${XCSOAR_LINK_LIBRARIES})

if(MSVC)
## if(WIN32)
   target_link_options(${APP_NAME} PUBLIC "/SUBSYSTEM:WINDOWS")
elseif(CLANG)
  # target_link_options(${APP_NAME} PUBLIC "/subsystem:windows")
  # target_link_options(${APP_NAME} PUBLIC "-mwindows")
endif()

add_custom_command(TARGET ${APP_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${directory} ${CMAKE_CURRENT_BINARY_DIR}/po
)

foreach(translation ${TRANSLATIONS})
        add_custom_command(TARGET ${APP_NAME} PRE_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECTGROUP_SOURCE_DIR}/src/_Deprecated/${translation}.mo ${CMAKE_CURRENT_BINARY_DIR}/po/${translation}.mo
            # TODO(aug): later do the creation here...
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECTGROUP_SOURCE_DIR}/src/_Deprecated/${translation}.mo.c ${CMAKE_CURRENT_BINARY_DIR}/po/${translation}.mo.c
        )
endforeach()

add_dependencies(${APP_NAME} util lib${APP_NAME})
if (3RDPARTY_BUILD_TARGETS)
    # third-party libs that still have to be built/installed this run:
    # force them BEFORE the app so the link step finds the fresh libs
    add_dependencies(${APP_NAME} ${3RDPARTY_BUILD_TARGETS})
    message(STATUS "+++ app waits for 3rd-party builds: ${3RDPARTY_BUILD_TARGETS}")
endif()

add_custom_command(TARGET ${APP_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${directory} ${CMAKE_CURRENT_BINARY_DIR}/po
)

#=============================
    # Test-Folder !!!!
    add_subdirectory(test/src)

# isn't possible: set_target_properties(ALL_BUILD PROPERTIES FOLDER _CMake)
# isn't possible: set_target_properties(ZERO_CHECK PROPERTIES FOLDER _CMake)
if (MSVC)
    # The data directory the debugger starts against.  XCSOAR_BASEDATA
    # decides (machine-specific, e.g.  set XCSOAR_BASEDATA=C:/Data),
    # exactly like the run step in CMakeSoaringProject.py: each brand
    # gets its own <base>/<brand>Data.  Without it, fall back to the
    # old guess.
    if (DEFINED ENV{XCSOAR_BASEDATA} AND NOT "$ENV{XCSOAR_BASEDATA}" STREQUAL "")
      string(REPLACE "\\" "/" APP_DATA_BASE "$ENV{XCSOAR_BASEDATA}")
      string(REGEX REPLACE "/+$" "" APP_DATA_BASE "${APP_DATA_BASE}")
      set(APP_DATA ${APP_DATA_BASE}/${APP_NAME}Data)
    elseif (EXISTS /Data/${APP_NAME}Data)
      set(APP_DATA /Data/${APP_NAME}Data)
    else ()
      set(APP_DATA /${APP_NAME}Data)
    endif ()
    message(STATUS "Debugger data: ${APP_DATA}")
    message(STATUS "Configure:     ${PROJECTGROUP_BINARY_DIR}/${APP_NAME}.vcxproj.user")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/ide/msvc/${APP_NAME}.vcxproj.user.in"
        "${PROJECTGROUP_BINARY_DIR}/${APP_NAME}.vcxproj.user"
    )
endif()
