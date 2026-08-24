set(_SOURCES
        IGC/Generator.cpp
        IGC/IGCFix.cpp
        IGC/IGCParser.cpp
        IGC/IGCString.cpp
        IGC/IGCWriter.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
)

# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        IGC/IgcMetaCache.cpp
)
