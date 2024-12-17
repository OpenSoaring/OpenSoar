cmake_minimum_required(VERSION 3.15)
if (SHOW_SUBPROJECTS)
    message(STATUS "+++ Start CMake ${CMAKE_CURRENT_SOURCE_DIR}!")
endif()

  get_filename_component(TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

if (HAVE_SKYSIGHT)
  add_compile_definitions(USE_GEOTIFF)
endif (HAVE_SKYSIGHT)


include(CMakeSource.cmake)
# organize the files in subdirectories

set(SOURCE_FILES )
foreach(source_file ${_SOURCES})
    string(REPLACE "${TARGET_NAME}/" "" source_file ${source_file})
    string(REPLACE "${SRC}/" "" source_file ${source_file})
    list(APPEND SOURCE_FILES ${source_file})
    get_filename_component(src_path ${source_file} DIRECTORY)
    if (src_path)
        string(REPLACE "/" "\\" src_path ${src_path})  ### for VisualStudio-IDE
    endif()
    source_group("Source\\${src_path}" FILES  ${source_file})
    ### hide: message(STATUS "### ${src_path}  --- ${source_file}")
endforeach()

if(NOT HEADER_FILES)  #  STREQUAL ""
file(GLOB_RECURSE   HEADER_FILES_TEMP   "${CMAKE_CURRENT_SOURCE_DIR}/*.h*")  # ;../*.hxx;../*.h
#    message(FATAL_ERROR "### ### Header-Liste  --- ${HEADER_FILES_TEMP}" )
set(HEADER_FILES)
foreach(header_file ${HEADER_FILES_TEMP})
    string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}/" "" header_file ${header_file})
    list(APPEND HEADER_FILES ${header_file})
    get_filename_component(src_path ${header_file} DIRECTORY)
    if (src_path)
        string(REPLACE "/" "\\" src_path ${src_path})
    endif()
    source_group("Header\\${src_path}" FILES  ${header_file})
    ## message(STATUS "### ### ${src_path}  --- ${header_file}" )
endforeach()
#     message(FATAL_ERROR "### ### Header-Liste  --- ${HEADER_FILES}" )
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_library(${TARGET_NAME} ${XCSOAR_LIB_TYPE}
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${SCRIPT_FILES}
)
# message(FATAL_ERROR "Stop!")
set_target_properties(${TARGET_NAME} PROPERTIES FOLDER FrontEnd)

if(MINGW)
  ## target_link_directories(${TARGET_NAME} PRIVATE ${COMPILER_LIB_PATH})
  ### set(_LINK_LIBRARIES  "gdiplus msimg32")
#  set(_LINK_LIBRARIES  "gdiplus libmsimg32.a")
### 2024-12-13  set(_LINK_LIBRARIES  "gdiplus D:/Programs/MinGW/mgw122/x86_64-w64-mingw32/lib/libmsimg32.a")
  set(_LINK_LIBRARIES  "gdiplus ${COMPILER_LIB_PATH}/libmsimg32.a")
  # set(_LINK_LIBRARIES  "gdiplus")
endif(MINGW)
if (HAVE_SKYSIGHT)
  message(STATUS "TIFF_TARGET = ${TIFF_TARGET}!")
  message(STATUS "TIFF_LIBRARY = ${TIFF_LIBRARY}!")
  message(STATUS "GEOTIFF_LIBRARY = ${GEOTIFF_LIBRARY}!")
### message(FATAL_ERROR "Stop!")
#    target_link_libraries(${TARGET_NAME} ${TIFF_TARGET})
   # target_link_libraries(${TARGET_NAME} ${TIFF_LIBRARY} ${GEOTIFF_LIBRARY})
   add_dependencies(${TARGET_NAME} util ${GEOTIFF_TARGET})
   # string(APPEND _LINK_LIBRARIES  " ${TIFF_LIBRARY} ${GEOTIFF_LIBRARY}")
   # string(APPEND _LINK_LIBRARIES  " ${TIFF_TARGET} ${GEOTIFF_TARGET}")
   list(APPEND _LINK_LIBRARIES ${TIFF_TARGET} ${GEOTIFF_TARGET})
   # string(APPEND _LINK_LIBRARIES " ${TIFF_TARGET} ${GEOTIFF_TARGET}")
endif()

if(_LINK_LIBRARIES)
  target_link_libraries(${TARGET_NAME} PRIVATE ${_LINK_LIBRARIES})
endif(_LINK_LIBRARIES)
message(STATUS "_LINK_LIBRARIES = ${_LINK_LIBRARIES}!")
# message(FATAL_ERROR "Stop!")
