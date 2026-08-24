set(_SOURCES
        Tracking/LiveTrack24/SessionID.cpp
        Tracking/LiveTrack24/Glue.cpp
        Tracking/LiveTrack24/Client.cpp

        Tracking/SkyLines/Assemble.cpp
        Tracking/SkyLines/Client.cpp
        Tracking/SkyLines/Glue.cpp
        Tracking/SkyLines/Key.cpp

        Tracking/TrackingGlue.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
)

# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        Tracking/SkyLines/FlarmTrafficBuilder.cpp
)
