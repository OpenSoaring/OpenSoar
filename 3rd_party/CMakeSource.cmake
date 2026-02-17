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

  list(APPEND CMAKE_FILES tiff.cmake) # August2111: without SkySight too!?
if (HAVE_SKYSIGHT)   # SkySight!
  list(APPEND CMAKE_FILES sqlite3.cmake)
  list(APPEND CMAKE_FILES proj.cmake)
  list(APPEND CMAKE_FILES geotiff.cmake)

  if (SKYSIGHT_FORECAST)  # HAVE_SKYSIGHT)   # SkySight!
    list(APPEND CMAKE_FILES netcdf-c.cmake)
    list(APPEND CMAKE_FILES netcdf-cxx.cmake)
  endif(SKYSIGHT_FORECAST)
endif(HAVE_SKYSIGHT)
