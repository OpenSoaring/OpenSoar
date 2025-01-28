cmake_minimum_required(VERSION 3.18)
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
   OpenSoar-News.md.gz
   Data/other/egm96s.dem
)

set(GRAPHIC_FILES
    ${_DATA_INPUT}/graphics/logo.svg
    ${_DATA_INPUT}/graphics/progress_border.svg
    ${_DATA_INPUT}/graphics/title.svg

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
      COMMENT BIN2C: ${bin_file}, 
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
add_custom_command(OUTPUT ${_TEMP_OUTPUT}/title.svg
  COMMENT "Replace Strings!"
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_TEMP_OUTPUT}
  COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/replace.py 
          ${PROJECTGROUP_SOURCE_DIR}/OpenSoar.config
          ${_DATA_INPUT}/graphics/title.svg
          ${_TEMP_OUTPUT}/title.svg
          ${_INCLUDE_OUTPUT}/ProgramVersion.h
  DEPENDS ${_DATA_INPUT}/graphics/title.svg
          ${PROJECTGROUP_SOURCE_DIR}/OpenSoar.config
          ${PROJECTGROUP_BINARY_DIR}/VERSION.txt
          ${PROJECTGROUP_SOURCE_DIR}/tools/python/replace.py 
  WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
)
# ============================================================================   
add_custom_command(OUTPUT ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc ${_INCLUDE_OUTPUT}/MakeResource.hpp
  COMMENT "Create Resource File!"
  COMMAND ${PYTHON_APP} ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_resource.py 
          ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt 
          ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc
          ${_DATA_INPUT}
          ${_DATA_OUTPUT}
          ${_INCLUDE_OUTPUT}/MakeResource.hpp 
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources.txt
          ${PROJECTGROUP_SOURCE_DIR}/tools/python/create_resource.py
)
# message(FATAL_ERROR "!!!!!! ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc")
# if (_COMPLETE_INSTALL)
set(GRAPHIC_BITMAPS
   ${_GRAPHICS_OUTPUT}/logo_160.bmp
   ${_GRAPHICS_OUTPUT}/logo_80.bmp
   ${_GRAPHICS_OUTPUT}/launcher_640.bmp
   ${_GRAPHICS_OUTPUT}/launcher_640_1.bmp
   ${_GRAPHICS_OUTPUT}/launcher_640_2.bmp
   ${_GRAPHICS_OUTPUT}/title_320.bmp
   ${_GRAPHICS_OUTPUT}/title_110.bmp
) # set

# ============================================================================   
add_custom_command(OUTPUT ${GRAPHIC_BITMAPS}
  COMMENT "Convert graphics!"
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_GRAPHICS_OUTPUT}
  COMMAND ${BMP_CONVERT_APP} -size 160 Data/graphics/logo.svg bmp3:${_GRAPHICS_OUTPUT}/logo_160.bmp
  COMMAND ${BMP_CONVERT_APP}  -size 80 Data/graphics/logo.svg bmp3:${_GRAPHICS_OUTPUT}/logo_80.bmp
  COMMAND ${BMP_CONVERT_APP}  -background white -layers flatten +matte +dither -compress none -type optimize -colors 256  Data/graphics/launcher.svg bmp3:${_GRAPHICS_OUTPUT}/launcher_640.bmp
  COMMAND ${BMP_CONVERT_APP}  -crop "50${PERCENT_CHAR}x100${PERCENT_CHAR}" -scene 1  ${_GRAPHICS_OUTPUT}/launcher_640.bmp "bmp3:${_GRAPHICS_OUTPUT}/launcher_640_${PERCENT_CHAR}d.bmp"
  COMMAND ${BMP_CONVERT_APP}  Data/graphics/progress_border.svg bmp3:${_GRAPHICS_OUTPUT}/progress_border.bmp
  COMMAND ${BMP_CONVERT_APP}  -size 110x110 ${_TEMP_OUTPUT}/title.svg bmp3:${_GRAPHICS_OUTPUT}/title_110.bmp 
  COMMAND ${BMP_CONVERT_APP}  -size 320x320 ${_TEMP_OUTPUT}/title.svg bmp3:${_GRAPHICS_OUTPUT}/title_320.bmp
  
  # COMMAND ${BMP_CONVERT_APP}  -size 320 -background white -layers flatten +matte +dither -compress none -type optimize -colors 256  Data/graphics/title.svg bmp3:${_GRAPHICS_OUTPUT}/title_320.bmp
  # WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
  DEPENDS ${_DATA_INPUT}/graphics/logo.svg
          ${_DATA_INPUT}/graphics/progress_border.svg
          ${_DATA_INPUT}/graphics/launcher.svg
          ${_TEMP_OUTPUT}/title.svg
  WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
) #add_custom_command
# endif()

set(ICON_BITMAPS ) # set to empty
if(ON) # ==>  for test it only with few icons!
  file(GLOB ICON_FILES  ${_DATA_INPUT}/icons/*.svg)
else()
  set (ICON_FILES  
    ${_DATA_INPUT}/icons/map_bridge.svg
    ${_DATA_INPUT}/icons/map_castle.svg
  )
endif()
# set(ICON_FILES  )  # empty!!!!
foreach(icon_file ${ICON_FILES})
  get_filename_component(_filename ${icon_file} NAME_WE)
  set(icon_file_out ${_ICONS_OUTPUT}/${_filename})
  set(icon_file_std ${_ICONS_OUTPUT}/${_filename}_96)
  set(icon_file_hd  ${_ICONS_OUTPUT}/${_filename}_160)
  set(icon_file_uhd ${_ICONS_OUTPUT}/${_filename}_300)
  set(tmp_file ${_TEMP_OUTPUT}/icons/${_filename})
  set(BMP_CONVERT_PARAM +dither -compress none -type optimize -colors 256 )
  set(BMP_BACKGROUND white)  # original gray! 
  # if (_COMPLETE_INSTALL)
    list(APPEND ICON_BITMAPS ${icon_file_std}.bmp  ${icon_file_hd}.bmp ${icon_file_uhd}.bmp)
    if (WITH_REMOVE)
       set (REMOVE_CMD
         COMMAND ${CMAKE_COMMAND} -E remove -f ${tmp_file}_alpha.png
         COMMAND ${CMAKE_COMMAND} -E remove -f ${tmp_file}_rgb.png
         COMMAND ${CMAKE_COMMAND} -E remove -f ${tmp_file}.png
       )
     else()
       set (REMOVE_CMD)
     endif()
  if (OFF)
     # August2111: linux is using XSLTPROC_APP, on Windows we using INKSCAPE_APP, is this the same???
     set (COPY_CMD
       COMMAND ${XSLTPROC_APP} --nonet --stringparam DisableAA_Select "MASK_NOAA_" --output  ${PROJECTGROUP_SOURCE_DIR}/build/svg_preprocess.xsl  ${icon_file}
     )
   else()
     set (COPY_CMD
       COMMAND ${INKSCAPE_APP} ${icon_file_out}.svg --export-dpi=124 --export-type=PNG --export-overwrite --export-filename=${icon_file_std}.png
       COMMAND ${INKSCAPE_APP} ${icon_file_out}.svg --export-dpi=200 --export-type=PNG --export-overwrite --export-filename=${icon_file_hd}.png
       COMMAND ${INKSCAPE_APP} ${icon_file_out}.svg --export-dpi=300 --export-type=PNG --export-overwrite --export-filename=${icon_file_uhd}.png
     )
   endif()
   # =========================================================================
   add_custom_command(OUTPUT ${icon_file_std}.bmp  ${icon_file_hd}.bmp ${icon_file_uhd}.bmp
     COMMENT File aus 'icons': ${icon_file_uhd}.bmp
     COMMAND ${CMAKE_COMMAND} -E make_directory ${_ICONS_OUTPUT}
     COMMAND ${CMAKE_COMMAND} -E make_directory ${_TEMP_OUTPUT}/icons
     COMMAND ${CMAKE_COMMAND} -E copy ${icon_file} ${icon_file_out}.svg
     ${COPY_CMD}
     COMMAND ${BMP_CONVERT_APP} ${icon_file_uhd}.png -alpha Extract -colors 8 ${tmp_file}_alpha.png
     # "-alpha Off" == "+matte"
     COMMAND ${BMP_CONVERT_APP} ${icon_file_uhd}.png -background ${BMP_BACKGROUND} -flatten -alpha Off +dither -colors 64  ${tmp_file}_rgb.png
     COMMAND ${BMP_MONTAGE_APP} -tile 2x1 -geometry +0+0 ${tmp_file}_alpha.png ${tmp_file}_rgb.png -depth 8 ${tmp_file}.png
     COMMAND ${BMP_CONVERT_APP} ${tmp_file}.png  -resize 132x66 -colors 64 BMP3:${icon_file_uhd}.bmp
     COMMAND ${BMP_CONVERT_APP} ${tmp_file}.png  -resize  44x22 -colors 64 BMP3:${icon_file_hd}.bmp
     COMMAND ${BMP_CONVERT_APP} ${tmp_file}.png  -resize  44x22 -colors 64 BMP3:${icon_file_std}.bmp
     # COMMAND ${BMP_CONVERT_APP} ${tmp_file}.png  -resize 20x10 -colors 64 BMP3:${icon_file_std}.bmp
     ${REMOVE_CMD}
     MAIN_DEPENDENCY ${icon_file}
     DEPENDS ${icon_file}
     WORKING_DIRECTORY ${PROJECTGROUP_SOURCE_DIR}
   )
# endif()
endforeach()
       
# pure cmd line in console with output in console (bypass: > resource.h):
# perl -ne "print \"#define $1 $2\n\" if /^MAKE_RESOURCE\((\w+), (\d+)\);/;" [path-to-xcsoar]/XCSoar/src/Resources.hpp
    
## moved to WinMSVC and so on!
if("${DOLLAR_CHAR}" STREQUAL "")
    message(FATAL_ERROR DOLLAR_CHAR not used: Stop!)
endif() 

   
    if (ON) # MSVC) # in other targets not possible to add this files to the project???
       source_group("Bitmaps" FILES  ${ICON_BITMAPS}  ${GRAPHIC_BITMAPS})
    endif()
    source_group("Icons" FILES  ${ICON_FILES})
    source_group("Graphics" FILES  ${GRAPHIC_FILES})
    
    ## add_custom_command(TARGET ${TARGET_NAME} PRE_LINK
    set(INPUTEVENTS_CPP
        ${_INCLUDE_OUTPUT}/InputEvents_Char2GCE.hpp
        # ${_INCLUDE_OUTPUT}/InputEvents_Char2GCE.cpp
        ${_INCLUDE_OUTPUT}/InputEvents_Char2NE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Text2Event.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Text2GCE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_Text2NE.hpp
        ${_INCLUDE_OUTPUT}/InputEvents_default.hpp
    )

    set(DEFAULT_XCI   ${_DATA_INPUT}/Input/defaultOV.xci)
    # set(DEFAULT_XCI   ${_DATA_INPUT}/Input/${DEFAULT_XCI_FILE})
    add_custom_command(OUTPUT  ${INPUTEVENTS_CPP} ${_INCLUDE_OUTPUT}/InputCppFiles.txt
        COMMENT Create InputEvents_*.hpp!
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
    
    # list(APPEND C_FILES ${_INCLUDE_OUTPUT}/Status_defaults.cpp)

    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/logo.svg")
    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/progress_border.svg")
    list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/title.svg")
    
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


if(MSVC)
  set(RESOURCE_FILES
    ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc
  )
elseif(MINGW)
  set(RESOURCE_FILES
     D:/Projects/Binaries/OpenSoar/dev-branch/mgw122/OpenSoar.rc
  )
#  message(FATAL_ERROR "MINGW!")
elseif(CLANG)
  set(RESOURCE_FILES
     # ${OUTPUT}/${PROGRAM_NAME}.rc
     # ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc
  # Clang:?    ${CMAKE_CURRENT_SOURCE_DIR}/OpenSoar.rc
)
else()
    # ${_DATA_OUTPUT}/OpenSoar.rc
    # ${PROGRAM_NAME}.rc
#  set(RESOURCE_FILES ${PROJECTGROUP_BINARY_DIR}/${PROGRAM_NAME}.rc
  set(RESOURCE_FILES ${PROJECTGROUP_BINARY_DIR}/OpenSoar.rc
)
endif()

add_library(${TARGET_NAME}  ${XCSOAR_LIB_TYPE}
   ${RESOURCE_FILES}  # depends on system?
   
   ${C_FILES}
   ${HEADER_FILES}
   ${RESOURCE_OUT}
   # GraphicBitmaps  # the role...
   ${GRAPHIC_BITMAPS}
   ${GRAPHIC_FILES}
   ${ICON_BITMAPS}
   ${ICON_FILES}
   ${PROJECTGROUP_SOURCE_DIR}/tools/Char2GCE.pl
   ${INPUTEVENTS_CPP}
   ${_INCLUDE_OUTPUT}/InputCppFiles.txt
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

add_dependencies(${TARGET_NAME} MathTables po-Files)
# add_dependencies(${TARGET_NAME} util MathTables)
# add_dependencies(${TARGET_NAME} util po-Files)

# set (RESOURCE_FILES ${RESOURCE_FILES}) 
