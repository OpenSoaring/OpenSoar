# Injected into the third-party builds that consume our libtiff.
#
# Since libtiff is built with zlib, its exported package links the
# imported target ZLIB::ZLIB - but PROJ and libgeotiff never call
# find_package(ZLIB) themselves, so nothing defines that target and
# their find_package(TIFF) fails with
#   The link interface of target "TIFF::tiff" contains: ZLIB::ZLIB
#   but the target was not found.
#
# CMAKE_PROJECT_INCLUDE_BEFORE runs this right after their project()
# call, early enough for every find_package(TIFF) that follows.

if (NOT TARGET ZLIB::ZLIB AND ZLIB_LIBRARY)
  add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_LOCATION "${ZLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}")
  message(STATUS "zlib-imported: ZLIB::ZLIB -> ${ZLIB_LIBRARY}")
endif()
