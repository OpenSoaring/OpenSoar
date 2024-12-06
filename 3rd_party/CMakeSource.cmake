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

list(APPEND CMAKE_FILES sodium.cmake)
list(APPEND CMAKE_FILES fmt.cmake)


set (SKYSIGHT_LIBS ON)
if (SKYSIGHT_LIBS)    # SkySight!
  set (HAVE_HDF5 ON)
  if (HAVE_HDF5)
    list(APPEND CMAKE_FILES szip.cmake)
    list(APPEND CMAKE_FILES hdf5.cmake)
  endif (HAVE_HDF5)
  set (SKYSIGHT_TEST ON)
  if (SKYSIGHT_TEST)
     list(APPEND CMAKE_FILES sqlite.cmake)
     list(APPEND CMAKE_FILES proj.cmake)
     # list(APPEND CMAKE_FILES sqlite3.cmake)
     list(APPEND CMAKE_FILES tiff.cmake)
     list(APPEND CMAKE_FILES geotiff.cmake)
     list(APPEND CMAKE_FILES netcdf-c.cmake)
     list(APPEND CMAKE_FILES netcdf-cxx.cmake)
  endif(SKYSIGHT_TEST)
endif(SKYSIGHT_LIBS)

if(0)  # MapServer
    list(APPEND CMAKE_FILES mapserver.cmake)
endif()
if(0)  # zzip
    list(APPEND CMAKE_FILES zzip.cmake)
endif()
if(0) # XML-Parser
    list(APPEND CMAKE_FILES xmlparser.cmake)
endif()

# for icon convert:
if(0) # RSVG
    list(APPEND CMAKE_FILES rsvg.cmake)
endif()
if(0)  # InkScape
    list(APPEND CMAKE_FILES inkscape.cmake)
endif()
if(0) # RSVG
    list(APPEND CMAKE_FILES iconv.cmake)
    list(APPEND CMAKE_FILES xml2.cmake)
    list(APPEND CMAKE_FILES xlst.cmake)
endif()


# For OpenGL:
if(0)  # freeglut
    list(APPEND CMAKE_FILES freeglut.cmake)
endif()
if(ENABLE_SDL) # SDL for OpenGL?
    list(APPEND CMAKE_FILES sdl.cmake)
endif()
if(ENABLE_GLM) # GLM for OpenGL?
    list(APPEND CMAKE_FILES glm.cmake)
endif()
