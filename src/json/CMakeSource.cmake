set(_SOURCES
        json/Boost.cxx
        json/ParserOutputStream.cxx
        json/Serialize.cxx
        json/Parse.cxx  # add 7.38
	
        json/Get.cpp  # add OpenSoar 7.43-3.23.9
        json/File.cpp  # add OpenSoar 7.43-3.23.9
)

set(SCRIPT_FILES
    CMakeSource.cmake
)

file(GLOB_RECURSE HEADER_FILES "*.h;*.hxx;*.hpp")
