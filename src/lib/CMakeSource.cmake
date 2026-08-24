set(_SOURCES
        fmt/RuntimeError.cxx
        fmt/SystemError.cxx
)

set(SCRIPT_FILES
    CMakeSource.cmake

#    ../../build/lib.mk  # menu sorces in main.mk!
)

# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
if(UNIX)
  list(APPEND _SOURCES
        lib/dbus/CallMethodSync.cxx
        lib/dbus/Properties.cxx
  )
endif(UNIX)
