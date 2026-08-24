set(_SOURCES
# no sources up to now in Win32 or Linux...
)

set(SCRIPT_FILES
    CMakeSource.cmake
)

# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        Cloud/OGNAprs.cpp
        Cloud/OGNClient.cpp
        Cloud/OGNTraffic.cpp
)
