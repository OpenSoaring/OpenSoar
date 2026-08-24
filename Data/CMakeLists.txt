cmake_minimum_required(VERSION 3.18)

# Since CMake 3.27 (policy CMP0147) the Visual Studio generator runs the
# custom commands of a project *in parallel* (<BuildInParallel>).  Both
# Inkscape and ImageMagick are not safe to start many times at once on
# Windows (shared preference/lock files, OpenMP pool init) - the graphics
# conversion then hangs at a random icon until Ctrl+C.  Force the
# classic sequential behaviour for everything in this directory.
if(POLICY CMP0147)
  cmake_policy(SET CMP0147 OLD)
endif()

if (SHOW_SUBPROJECTS)
    message(STATUS "+++ Start CMake ${CMAKE_CURRENT_SOURCE_DIR}!")
endif()

# set(_COMPLETE_INSTALL ON)

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

# the internal used directories:
set(_INCLUDE_OUTPUT ${OUTPUT}/include)
set(_DATA_INPUT ${PROJECTGROUP_SOURCE_DIR}/Data)
set(_DATA_OUTPUT ${OUTPUT}/Data)
set(_ICONS_OUTPUT ${_DATA_OUTPUT}/icons)
set(_TEMP_OUTPUT ${_DATA_OUTPUT}/temp)
set(_GRAPHICS_OUTPUT ${_DATA_OUTPUT}/graphics)

include(CMakeSource.cmake)

set (BIN_FILES
   AUTHORS.gz
   COPYING.gz
   NEWS.txt.gz
   THIRD_PARTY_NOTICES.txt.gz
   Data/other/egm96s.dem
)
if(EXISTS ${PROJECTGROUP_SOURCE_DIR}/OpenSoar-News.md)  # branding topic
  list(APPEND BIN_FILES OpenSoar-News.md.gz)
endif()

set(GRAPHIC_FILES
    ${_DATA_INPUT}/graphics/logo.svg
    ${_DATA_INPUT}/graphics/progress_border.svg
    ${_DATA_INPUT}/graphics/title.svg
    ${_DATA_INPUT}/graphics/title_red.svg

    # ${_DATA_INPUT}/graphics/dialog_title.svg
    # ${_DATA_INPUT}/graphics/launcher.svg
)

if("${PERCENT_CHAR}" STREQUAL "")
    message(FATAL_ERROR PERCENT_CHAR not used: Stop!)
endif() 

set(SHOW_MATCHES OFF)

set(SCRIPT_FILES )
set(C_FILES)  # Reset to empty...
if(ON) # MSVC)  # mit minGW geht das nicht??
# if(WIN32)  # bei Linux klappt etwas nich!!!
foreach(bin_file ${BIN_FILES})
  get_filename_component(c_file ${bin_file} NAME)
  set(c_file ${OUTPUT_FOLDER}/Data/${c_file}.c)
  # set(c_file  Data/${c_file}.c)
  # similar add_custom_command - but not the same ;-(
  if(${bin_file} MATCHES "\.gz$")
    string(REGEX REPLACE  "\.gz$"  "" txt_file ${bin_file})
    if(SHOW_MATCHES)
      message(STATUS "MATCHES!!!! ${bin_file}  --- ${txt_file}")
    endif()
    set(zip_file  "${_TEMP_OUTPUT}/${bin_file}")
    # set(zip_file  "${_DATA_OUTPUT}/temp/${bin_file}")
    set (bin_file "${zip_file}")
    if(SHOW_SOURCE_FILES)
        message(STATUS "OUTPUT: ${bin_file} --> ${c_file}")
    endif()
    # message(STATUS "Zip-App: '${ZIP_APP}'")
    add_custom_command(OUTPUT ${c_file}
       COMMENT "BIN2C: ${bin_file}, "
       COMMAND ${CMAKE_COMMAND} -E make_directory ${_DATA_OUTPUT}
       # COMMAND echo "${ZIP_APP} a ${zip_file} ${txt_file}"
       COMMAND ${ZIP_APP} a ${zip_file} ${txt_file}
       # COMMAND echo "${PYTHON_APP} tools/python/bin2c.py  ${bin_file}  ${_DATA_OUTPUT}"
       COMMAND ${PYTHON_APP} tools/python/bin2c.py  ${bin_file}  ${_DATA_OUTPUT}
       WORKING_DIRECTORY  ${PROJECTGROUP_SOURCE_DIR}
       DEPENDS ${bin_file}
       # DEPENDS ${PROJECTGROUP_SOURCE_DIR}/${bin_file}
    )
  else()
    message(STATUS "UNMATCHES!!!! ${bin_file}  --- ${txt_file}")
    message(STATUS "OUTPUT: ${bin_file} --> ${c_file}")
    add_custom_command(OUTPUT ${c_file}
      COMMENT "BIN2C: ${bin_file}, "
      # COMMAND echo "Cmd: '${PYTHON_APP} tools/python/bin2c.py  ${bin_file}   ${_DATA_OUTPUT}' ==> ${c_file}"
      COMMAND ${CMAKE_COMMAND} -E make_directory ${_DATA_OUTPUT}
      COMMAND ${PYTHON_APP} tools/python/bin2c.py  ${bin_file}  ${_DATA_OUTPUT}
      WORKING_DIRECTORY  ${PROJECTGROUP_SOURCE_DIR}
      DEPENDS ${PROJECTGROUP_SOURCE_DIR}/${bin_file}
      # DEPENDS ${bin_file}
    )
  endif()
  list(APPEND C_FILES ${c_file})
endforeach()
# endif(WIN32)
    
message(STATUS "Image-App: '${BMP_CONVERT_APP}' ")
  
# ============================================================================   
add_custom_command(OUTPUT ${_INCLUDE_OUTPUT}/ProgramVersion.h
  COMMENT "Make Config file!"
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_TEMP_OUTPUT}
  COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_config.py 
          ${CONFIG_FILE}
          ${_INCLUDE_OUTPUT}/ProgramVersion.h
          ${GIT_COMMIT_ID}
  DEPENDS ${CONFIG_FILE}
          ${PROJECTGROUP_BINARY_DIR}/VERSION.txt
          ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_config.py 
  WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
)
# ============================================================================   
# GDI retired: create_resource.py is only kept for the generated
# MakeResource.hpp (numeric resource ids, used by every flavor); the
# .rc it also writes is no longer compiled into anything
add_custom_command(OUTPUT ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc ${_INCLUDE_OUTPUT}/MakeResource.hpp
  COMMENT "Create MakeResource.hpp!"
  COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_resource.py 
          ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt 
          ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc
          ${_DATA_INPUT}
          ${_DATA_OUTPUT}
          ${_INCLUDE_OUTPUT}/MakeResource.hpp 
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt
          ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_resource.py
)

add_custom_command(OUTPUT ${_INCLUDE_OUTPUT}/ResourceLookup_entries.hpp
  COMMENT "Create Resource File! from ${PROJECTGROUP_SOURCE_DIR}/Data/resources.txt"
  COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/GenerateResourceLookup.py
          ## ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt 
          ${PROJECTGROUP_SOURCE_DIR}/Data/resources.txt   # das ist falsch!
          > ${_INCLUDE_OUTPUT}/ResourceLookup_entries.hpp
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt
          ${PROJECTGROUP_SOURCE_DIR}/tools/python/GenerateResourceLookup.py
)
# message(FATAL_ERROR "!!!!!! ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc")
# if (_COMPLETE_INSTALL)
# pure cmd line in console with output in console (bypass: > resource.h):
# perl -ne "print \"#define $1 $2\n\" if /^MAKE_RESOURCE\((\w+), (\d+)\);/;" [path-to-xcsoar]/XCSoar/src/Resources.hpp
    
## moved to WinMSVC and so on!
if("${DOLLAR_CHAR}" STREQUAL "")
    message(FATAL_ERROR DOLLAR_CHAR not used: Stop!)
endif() 

       ## add_custom_command(TARGET ${TARGET_NAME} PRE_LINK
    set(INPUTEVENTS_CPP
        ${_INCLUDE_OUTPUT}/InputEvents_Char2GCE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Char2NE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Text2Event.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Text2GCE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Text2NE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_default.hpp
    )

  set(TOOLS
    ${PROJECTGROUP_SOURCE_DIR}/tools/Char2GCE.pl
    ${PROJECTGROUP_SOURCE_DIR}/tools/Char2NE.pl
    ${PROJECTGROUP_SOURCE_DIR}/tools/Text2Event.pl
    ${PROJECTGROUP_SOURCE_DIR}/tools/Text2GCE.pl
    ${PROJECTGROUP_SOURCE_DIR}/tools/Text2NE.pl
    ${PROJECTGROUP_SOURCE_DIR}/tools/xci2cpp.pl
    ${PROJECTGROUP_SOURCE_DIR}/tools/python/bin2c.py
    ${PROJECTGROUP_SOURCE_DIR}/tools/python/replace.py
    ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_config.py
    ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_resource.py
  )

    set(DEFAULT_XCI   ${_DATA_INPUT}/Input/${DEFAULT_XCI_FILE})
    message(STATUS "### DEFAULT_XCI = ${DEFAULT_XCI}")
    add_custom_command(OUTPUT  ${INPUTEVENTS_CPP} ${_INCLUDE_OUTPUT}/InputCppFiles.txt
        COMMENT "Create InputEvents_*.hpp!"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_INCLUDE_OUTPUT} 
        COMMAND perl tools/Char2GCE.pl    src/Input/InputQueue.hpp >${_INCLUDE_OUTPUT}/InputEvents_Char2GCE.hpp
        COMMAND perl tools/Char2NE.pl     src/Input/InputQueue.hpp >${_INCLUDE_OUTPUT}/InputEvents_Char2NE.hpp
        COMMAND perl tools/Text2Event.pl  src/Input/InputEvents.hpp >${_INCLUDE_OUTPUT}/InputEvents_Text2Event.hpp
        COMMAND perl tools/Text2GCE.pl    src/Input/InputQueue.hpp >${_INCLUDE_OUTPUT}/InputEvents_Text2GCE.hpp
        COMMAND perl tools/Text2NE.pl     src/Input/InputQueue.hpp >${_INCLUDE_OUTPUT}/InputEvents_Text2NE.hpp
        COMMAND perl tools/xci2cpp.pl     ${DEFAULT_XCI}           >${_INCLUDE_OUTPUT}/InputEvents_default.hpp
        COMMAND perl tools/xci2cpp.pl     ${DEFAULT_XCI}           >${_INCLUDE_OUTPUT}/InputCppFiles.txt
        DEPENDS ${PROJECTGROUP_SOURCE_DIR}/src/Input/InputQueue.hpp 
                ${PROJECTGROUP_SOURCE_DIR}/src/Input/InputEvents.hpp
                ${DEFAULT_XCI}
        WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
    )
    
    add_custom_command( OUTPUT ${OUTPUT_FOLDER}/include/Status_defaults.hpp
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_INCLUDE_OUTPUT}
        COMMAND perl tools/xcs2cpp.pl    Data/Status/default.xcs >${OUTPUT_FOLDER}/include/Status_defaults.hpp
        DEPENDS ${_DATA_INPUT}/Status/default.xcs
        WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
    )
    
    list(APPEND INPUTEVENTS_CPP ${OUTPUT_FOLDER}/include/Status_defaults.hpp)

    # Quick Guide: compile-time Markdown slice of NEWS.txt
    # (mirrors build/generate.mk: QuickGuideNEWS.hpp)
    add_custom_command(OUTPUT ${_INCLUDE_OUTPUT}/QuickGuideNEWS.hpp
        COMMENT "Create QuickGuideNEWS.hpp!"
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_INCLUDE_OUTPUT}
        COMMAND ${PYTHON_APP} tools/news_to_quickguide_md.py NEWS.txt >${_INCLUDE_OUTPUT}/QuickGuideNEWS.hpp
        DEPENDS ${PROJECTGROUP_SOURCE_DIR}/NEWS.txt
                ${PROJECTGROUP_SOURCE_DIR}/tools/news_to_quickguide_md.py
        WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
    )
    list(APPEND INPUTEVENTS_CPP ${_INCLUDE_OUTPUT}/QuickGuideNEWS.hpp)
    
    # list(APPEND C_FILES ${_INCLUDE_OUTPUT}/Status_defaults.cpp)

    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/logo.svg")
    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/progress_border.svg")
    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/title.svg")

    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/dialog_title.svg")
    
    include_directories(${_DATA_INPUT})
endif()
    include_directories( ${OUTPUT_FOLDER}/include  )
    include_directories(${Boost_INCLUDE_DIR})

if(MSVC)
    add_compile_definitions(__MSVC__)
endif()

if(WIN32)
    set(BITMAP_EXTENSION  .bmp)
else()
    set(BITMAP_EXTENSION  .png)
endif()


if(WIN32)
  # resources are embedded via resources.c; the .rc only carries the
  # application icon (Data/<AppName>Icon.rc - the branding topic
  # provides its own OpenSoarIcon.rc)
  set(RESOURCE_FILES ${CMAKE_CURRENT_SOURCE_DIR}/${APP_NAME}Icon.rc)
else()
  set(RESOURCE_FILES)
endif()

set(RESOURCE_FILES ${RESOURCE_FILES} PARENT_SCOPE)

# ---------------------------------------------------------------------------
# non-Win32 resources (SDL/OpenGL flavor): PNG + raw sounds embedded as C
# byte arrays, plus resource_data.h - see tools/python/gen_resources_data.py
# (mirrors build/resource.mk + tools/LinkResources.pl/GenerateResources.pl)
# ---------------------------------------------------------------------------
if (ENABLE_SDL OR NOT WIN32)
    set(RESOURCE_DEFINES)
    foreach(_d XCSOAR_TESTING ENABLE_OPENGL EYE_CANDY ENABLE_SDL)
        if(${_d})
            list(APPEND RESOURCE_DEFINES --define ${_d})
        endif()
    endforeach()
    file(GLOB _RES_SOURCES
        ${_DATA_INPUT}/icons/*.svg
        ${_DATA_INPUT}/graphics/*.svg
        ${_DATA_INPUT}/bitmaps/*.bmp
        ${_DATA_INPUT}/sound/*.wav
        ${PROJECTGROUP_SOURCE_DIR}/doc/manual/figures/gesture_*.svg)
    set(RESOURCES_C ${OUTPUT_FOLDER}/Data/resources.c)
    add_custom_command(OUTPUT ${RESOURCES_C} ${_INCLUDE_OUTPUT}/resource_data.h
        COMMENT "Create resources.c / resource_data.h (PNG + raw sounds)"
        COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/gen_resources_data.py
                --src ${PROJECTGROUP_SOURCE_DIR}
                --data ${_DATA_OUTPUT}
                --out-c ${RESOURCES_C}
                --out-h ${_INCLUDE_OUTPUT}/resource_data.h
                --inkscape ${INKSCAPE_APP}
                --convert ${BMP_CONVERT_APP}
                --xsltproc ${XSLTPROC_APP}
                --config ${CONFIG_FILE}
                ${RESOURCE_DEFINES}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt
                ${CONFIG_FILE}
                ${PROJECTGROUP_SOURCE_DIR}/tools/python/gen_resources_data.py
                ${_RES_SOURCES}
        WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
    )
    list(APPEND C_FILES ${RESOURCES_C})
endif()

set(TEXT_FILES 
          ${_INCLUDE_OUTPUT}/InputCppFiles.txt
          ${PROJECTGROUP_BINARY_DIR}/VERSION.txt
          ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt 
)

# IDE folder 
source_group("Graphics" FILES  ${GRAPHIC_FILES})
source_group("TextFiles" FILES  ${TEXT_FILES}) 
source_group("Tools" FILES  ${TOOLS})

add_library(${TARGET_NAME}  ${XCSOAR_LIB_TYPE}
#   ${RESOURCE_FILES}  # depends on system?
   
   ${C_FILES}
   ${HEADER_FILES}
   ${RESOURCE_OUT}
   ${GRAPHIC_FILES}
   ${TOOLS}
   ${INPUTEVENTS_CPP}
   ${TEXT_FILES}
   ${_INCLUDE_OUTPUT}/ProgramVersion.h
   ${_INCLUDE_OUTPUT}/MakeResource.hpp
   ${_INCLUDE_OUTPUT}/ResourceLookup_entries.hpp
# InputCppFiles    # role for input files
   # DefaultStatus # role for status defaults
   # EXCLUDE_FROM_ALL
   # ${CMAKE_CURRENT_SOURCE_DIR}/${PROGRAM_NAME}.rc.in
)

set_target_properties(${TARGET_NAME} PROPERTIES 
    LINKER_LANGUAGE C
    FOLDER Data
    #  ENABLE_EXPORTS TRUE
)

add_dependencies(${TARGET_NAME} po-Files)

# set (RESOURCE_FILES ${RESOURCE_FILES}) 
