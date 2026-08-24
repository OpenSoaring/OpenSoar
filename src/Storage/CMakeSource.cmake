# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
set(_SOURCES
        Storage/DirEntry.cpp
        Storage/PlatformStorageHotplugMonitor.cpp
        Storage/PlatformStorageMonitor.cpp
        Storage/StorageDevice.cpp
        Storage/StorageEvents.cpp
        Storage/StorageManager.cpp
        Storage/StorageUtil.cpp
)
if(UNIX)
  list(APPEND _SOURCES
        Storage/linux/LinuxStorageDevice.cpp
        Storage/linux/LinuxStorageHotplugMonitor.cpp
        Storage/linux/LinuxStorageMonitor.cpp
  )
endif(UNIX)
if(WIN32)
  list(APPEND _SOURCES
        Storage/win/WinHotplugForward.cpp
        Storage/win/WindowsStorageDevice.cpp
        Storage/win/WindowsStorageHotplugMonitor.cpp
        Storage/win/WindowsStorageMonitor.cpp
  )
endif(WIN32)
if(ANDROID)
  list(APPEND _SOURCES
        Storage/android/AndroidSAFStorageDevice.cpp
        Storage/android/AndroidStorageHotplugMonitor.cpp
        Storage/android/AndroidStorageMonitor.cpp
        Storage/android/SAFOutputStream.cpp
        Storage/android/SAFReader.cpp
  )
endif(ANDROID)
