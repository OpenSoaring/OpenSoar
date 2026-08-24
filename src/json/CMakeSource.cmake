set(_SOURCES
        json/Boost.cxx
        json/ParserOutputStream.cxx
        json/Serialize.cxx
        json/Parse.cxx  # add 7.38
)
if(IS_OPENSOAR)
  list(APPEND _SOURCES
        # json/Get.cpp  # add OpenSoar 7.43-3.23.9  # [topic/misc] not in upstream XCSoar yet
        # json/File.cpp  # add OpenSoar 7.43-3.23.9  # [topic/misc] not in upstream XCSoar yet
  )
endif()

set(SCRIPT_FILES
    CMakeSource.cmake
)

file(GLOB_RECURSE HEADER_FILES "*.h;*.hxx;*.hpp")
