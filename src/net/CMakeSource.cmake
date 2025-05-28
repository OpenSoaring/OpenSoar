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
        http/CoDownload.cpp
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

set (SCRIPT_FILES
    CMakeSource.cmake
)

