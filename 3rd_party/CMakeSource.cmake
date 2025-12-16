file(GLOB SCRIPT_FILES *.txt *.in)
list(APPEND SCRIPT_FILES CMakeSource.cmake 3rd_party.cmake)


set(CMAKE_FILES
  boost.cmake
  zlib.cmake
  lua.cmake
  png.cmake
  cares.cmake
)

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
  list(APPEND CMAKE_FILES curl.cmake)
else()
# TODO(August2111): only temporarily curl.cmake
endif()

set(WITH_SODIUM 1)
if (WITH_SODIUM)
  list(APPEND CMAKE_FILES sodium.cmake)
endif (WITH_SODIUM)
list(APPEND CMAKE_FILES fmt.cmake)

set(WITH_TIFF 1)
if (WITH_TIFF)
  list(APPEND CMAKE_FILES tiff.cmake) # August2111: without SkySight too!?
endif (WITH_TIFF)

if (HAVE_SKYSIGHT)    # SkySight!
  set (HAVE_HDF5 ON)
  set (SKYSIGHT_LIBS ON)
  # --------------------
  if (HAVE_HDF5)
    list(APPEND CMAKE_FILES szip.cmake)
    list(APPEND CMAKE_FILES hdf5.cmake)
  endif (HAVE_HDF5)
  if (SKYSIGHT_LIBS)
# MSVC2026     list(APPEND CMAKE_FILES sqlite.cmake)
     list(APPEND CMAKE_FILES proj.cmake)
     # list(APPEND CMAKE_FILES sqlite3.cmake)
     list(APPEND CMAKE_FILES geotiff.cmake)
     list(APPEND CMAKE_FILES netcdf-c.cmake)
     list(APPEND CMAKE_FILES netcdf-cxx.cmake)
  endif(SKYSIGHT_LIBS)
endif(HAVE_SKYSIGHT)

if(0)  # MapServer
    list(APPEND CMAKE_FILES mapserver.cmake)
endif(0)
if(0)  # zzip
    list(APPEND CMAKE_FILES zzip.cmake)
endif(0)
if(0) # XML-Parser
    list(APPEND CMAKE_FILES xmlparser.cmake)
endif(0)

# for icon convert:
if(0) # RSVG
    list(APPEND CMAKE_FILES rsvg.cmake)
endif(0)
if(0)  # InkScape
    list(APPEND CMAKE_FILES inkscape.cmake)
endif(0)
if(0) # RSVG
    list(APPEND CMAKE_FILES iconv.cmake)
    list(APPEND CMAKE_FILES xml2.cmake)
    list(APPEND CMAKE_FILES xlst.cmake)
endif(0)


# Preparing for OpenGL:
if(0)  # freeglut
    list(APPEND CMAKE_FILES freeglut.cmake)
endif(0)
if(ENABLE_SDL) # SDL for OpenGL?
    list(APPEND CMAKE_FILES sdl.cmake)
endif(ENABLE_SDL)
if(ENABLE_GLM) # GLM for OpenGL?
    list(APPEND CMAKE_FILES glm.cmake)
endif(ENABLE_GLM)
