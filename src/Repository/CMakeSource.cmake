set(_SOURCES
        Repository/FileRepository.cpp
        Repository/Glue.cpp
        Repository/Parser.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
)

# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        Repository/FileType.cpp
)
