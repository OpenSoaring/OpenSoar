set(_SOURCES
        OpenVario/OpenVarioMenu.cpp

        OpenVario/FileMenuWidget.cpp

        OpenVario/System/System.cpp
        OpenVario/System/SystemMenuWidget.cpp
        OpenVario/System/SystemSettingsWidget.cpp

        ${SRC}/Version.cpp
       	${SRC}/Asset.cpp
        ${SRC}/Dialogs/DialogSettings.cpp
        ${SRC}/Dialogs/WidgetDialog.cpp
        ${SRC}/Dialogs/HelpDialog.cpp
        ${SRC}/Dialogs/Message.cpp
        ${SRC}/Dialogs/LockScreen.cpp
        ${SRC}/Dialogs/TextEntry.cpp
        ${SRC}/Dialogs/KnobTextEntry.cpp
        ${SRC}/Dialogs/TouchTextEntry.cpp
        ${SRC}/Form/DigitEntry.cpp
        ${SRC}/Formatter/HexColor.cpp
        ${SRC}/Formatter/TimeFormatter.cpp
        ${SRC}/Gauge/LogoView.cpp
        ${SRC}/Hardware/CPU.cpp
        ${SRC}/Hardware/DisplayDPI.cpp
        ${SRC}/Hardware/RotateDisplay.cpp
        ${SRC}/Hardware/DisplayGlue.cpp
        ${SRC}/LogFile.cpp
        ${SRC}/LocalPath.cpp
        ${SRC}/Look/TerminalLook.cpp
        ${SRC}/Look/DialogLook.cpp
        ${SRC}/Look/ButtonLook.cpp
        ${SRC}/Look/CheckBoxLook.cpp
        ${SRC}/Renderer/TwoTextRowsRenderer.cpp
        ${SRC}/Gauge/LogoView.cpp
        ${SRC}/Dialogs/DialogSettings.cpp
        ${SRC}/Dialogs/WidgetDialog.cpp
        ${SRC}/Dialogs/HelpDialog.cpp
        ${SRC}/Dialogs/Message.cpp
        ${SRC}/Dialogs/LockScreen.cpp
        ${SRC}/Dialogs/TextEntry.cpp
        ${SRC}/Dialogs/KnobTextEntry.cpp
        ${SRC}/Dialogs/TouchTextEntry.cpp
        ${SRC}/Profile/Map.cpp
        ${SRC}/Profile/File.cpp
        ${SRC}/Profile/NumericValue.cpp

        ${SRC}/ProgressGlue.cpp
        ${SRC}/ProgressWindow.cpp

        ${SRC}/Renderer/TwoTextRowsRenderer.cpp
        ${SRC}/Screen/Layout.cpp

        ${SRC}/io/FileOutputStream.cxx
        
        ${SRC}/lib/fmt/SystemError.cxx
        ${SRC}/Units/Descriptor.cpp

        ${SRC}/ResourceLoader.cpp
        ${SRC}/ui/control/TerminalWindow.cpp
        ${SRC}/ui/canvas/gdi/Canvas.cpp
        ${SRC}/ui/canvas/gdi/Bitmap.cpp
        ${SRC}/ui/canvas/gdi/GdiPlusBitmap.cpp
        ${SRC}/ui/canvas/gdi/ResourceBitmap.cpp

        ${SRC}/Interface.cpp
        ${SRC}/Blackboard/InterfaceBlackboard.cpp
        ${SRC}/MainWindow.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
    ../../build/ov.mk
)
