set(_SOURCES
        net/AddressInfo.cxx
        net/HostParser.cxx
        net/Resolver.cxx
        net/SocketError.cxx
        net/State.cpp
        net/ToString.cxx
        net/IPv4Address.cxx
        net/IPv6Address.cxx
        net/StaticSocketAddress.cxx
        net/AllocatedSocketAddress.cxx
        net/SocketAddress.cxx
        net/SocketDescriptor.cxx
)
list(APPEND _SOURCES
        net/client/tim/Client.cpp
        net/client/tim/Glue.cpp
)
list(APPEND _SOURCES
        http/DownloadManager.cpp
        http/Progress.cpp
        http/Init.cpp

        # lib_curl:
        ${SRC}/lib/curl/OutputStreamHandler.cxx
        ${SRC}/lib/curl/Adapter.cxx
        ${SRC}/lib/curl/Setup.cxx
        ${SRC}/lib/curl/Request.cxx
        ${SRC}/lib/curl/CoRequest.cxx
        ${SRC}/lib/curl/CoStreamRequest.cxx
        ${SRC}/lib/curl/Global.cxx
)

# CoDownloadToFile is core upstream code (DownloadManager, Weather tile
# store); it must NEVER depend on the branding switch
list(APPEND _SOURCES
        http/CoDownloadToFile.cpp
)
if(IS_OPENSOAR)
  list(APPEND _SOURCES
        # http/CoDownload.cpp  # [topic/misc] not in upstream XCSoar yet
  )
endif()

set (SCRIPT_FILES
    CMakeSource.cmake
)


# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        net/client/SyncHttp.cpp
        net/client/auth/JwtBearerSession.cpp
        net/client/xctherm/Http.cpp
)
if(UNIX)  # wifi backends use D-Bus / Connman / NetworkManager
  list(APPEND _SOURCES
        net/wifi/ConnmanClient.cpp
        net/wifi/ConnmanWifiBackend.cpp
        net/wifi/LinuxNetWifiDbus.cpp
        net/wifi/LinuxWifiBackend.cpp
        net/wifi/NetworkManagerClient.cpp
        net/wifi/NetworkManagerWifiBackend.cpp
        net/wifi/WifiError.cpp
  )
endif(UNIX)
