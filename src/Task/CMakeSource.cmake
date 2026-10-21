set(_SOURCES
        Task/DefaultTask.cpp
        Task/Deserialiser.cpp
        Task/FileProtectedTaskManager.cpp
        Task/LoadFile.cpp
        Task/MapTaskManager.cpp
        Task/ProtectedRoutePlanner.cpp
        Task/ProtectedTaskManager.cpp
        Task/RoutePlannerGlue.cpp
        Task/SaveFile.cpp
        Task/Serialiser.cpp
        Task/Summary.cpp
        Task/TaskFile.cpp
        Task/TaskFileIGC.cpp
        Task/TaskFileSeeYou.cpp
        Task/TaskFileXCSoar.cpp
        Task/TaskStore.cpp
        Task/TypeStrings.cpp
        Task/ValidationErrorStrings.cpp
        Task/XCTrackTaskFile.cpp  # add 7.38
        Task/XCTrackTaskDecoder.cpp  # add 7.38
        Task/PolylineDecoder.cpp  # add 7.38
        # task exchange by QR code (XCSoar 7.46); without zxing on Windows
        # the scanner is the stub in QRScanner.cpp, and the QR dialog and
        # its renderer are not built
        Task/CupTaskQR.cpp
        Task/QRScanner.cpp
        Task/ReceiveTask.cpp
        Task/XCSoarTaskQR.cpp
        Task/XCTrackTaskEncoder.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
)
