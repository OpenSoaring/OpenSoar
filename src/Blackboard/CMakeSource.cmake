set(_SOURCES
        Blackboard/BlackboardListener.cpp
        Blackboard/DeviceBlackboard.cpp
        Blackboard/InterfaceBlackboard.cpp
        Blackboard/LiveBlackboard.cpp
        Blackboard/ProxyBlackboardListener.cpp
        Blackboard/RateLimitedBlackboardListener.cpp
        Blackboard/ScopeCalculatedListener.cpp
        Blackboard/ScopeGPSListener.cpp
)
set(SCRIPT_FILES
    CMakeSource.cmake
)


# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        Blackboard/BlackboardListenerRegistration.cpp
)
