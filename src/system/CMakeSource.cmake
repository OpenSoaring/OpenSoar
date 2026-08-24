set(_SOURCES
        system/EventPipe.cxx
        system/FileUtil.cpp
        system/Path.cpp
        system/PathName.cpp
        system/Process.cpp
        system/RunFile.cpp
        system/SystemLoad.cpp
        system/OpenLink.cpp
)
if(UNIX)
  list(APPEND _SOURCES
##        system/EventPipe.cpp
  )
endif()

set(SCRIPT_FILES
    CMakeSource.cmake
)

# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        system/StandardVersion.cpp
)
if(WIN32)
  list(APPEND _SOURCES
        system/UTF8Win32.cpp
  )
endif(WIN32)
