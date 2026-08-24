set (BIN_FILES
   AUTHORS.gz
   COPYING.gz
   NEWS.txt.gz
   Data/other/egm96s.dem
)
if(EXISTS ${PROJECTGROUP_SOURCE_DIR}/OpenSoar-News.md)  # branding topic
  list(APPEND BIN_FILES OpenSoar-News.md.gz)
endif()

file(GLOB ICON_FILES        "icons/*.svg")
set(GRAPHIC_FILES )
list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/logo.svg")
list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/progress_border.svg")
list(APPEND GRAPHIC_FILES   "${_DATA_INPUT}/graphics/title.svg")

set(SCRIPT_FILES )
set(C_FILES)  # Reset to empty...

