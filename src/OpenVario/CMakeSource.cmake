set(TEST_SRC_DIR "${PROJECTGROUP_SOURCE_DIR}/test/src")

set(_SOURCES
        OpenVario/OpenVarioBaseMenu.cpp
        OpenVario/FileMenuWidget.cpp
        OpenVario/DisplaySettingsWidget.cpp
        OpenVario/SystemSettingsWidget.cpp

        OpenVario/System/OpenVarioDevice.cpp

        OpenVario/System/SystemMenuWidget.cpp
        OpenVario/System/Setting/RotationWidget.cpp 
        OpenVario/System/Setting/WifiWidget.cpp

        OpenVario/System/WifiDialogOV.cpp
        OpenVario/System/WifiSupplicantOV.cpp
        OpenVario/System/WifiDBus.cpp
        OpenVario/System/NMConnector.cpp

        ${SRC}/Version.cpp
       	${SRC}/Asset.cpp
        ${SRC}/Formatter/HexColor.cpp
        ${SRC}/Formatter/TimeFormatter.cpp
        ${SRC}/Hardware/CPU.cpp
        ${SRC}/Hardware/DisplayDPI.cpp
        ${SRC}/Hardware/RotateDisplay.cpp
        ${SRC}/Hardware/DisplayGlue.cpp
        ${SRC}/Screen/Layout.cpp
        ${SRC}/ui/control/TerminalWindow.cpp
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
        ${SRC}/Dialogs/ProcessDialog.cpp
        ${SRC}/Profile/Map.cpp
        ${SRC}/Profile/File.cpp
        ${SRC}/Profile/NumericValue.cpp
        ${SRC}/LocalPath.cpp
    # ${TEST_SRC_DIR}/Fonts.cpp
    # ${SRC}/Language/Language.cpp  # ${TEST_SRC_DIR}/FakeLanguage.cpp
    ${SRC}/LogFile.cpp   # ${TEST_SRC_DIR}/FakeLogFile.cpp
    # ${SRC}/Kobo/FakeSymbols.cpp
    ${SRC}/Dialogs/DataField.cpp
)

set(OPENVARIOBASEMENU_LIBS
# ov.mk: OV_MENU_DEPENDS = WIDGET FORM DATA_FIELD SCREEN EVENT RESOURCE ASYNC LIBNET OS IO THREAD TIME MATH UTIL
# ov.mk: OV_MENU_STRIP = y
   Profile Widget Form Renderer ui event net system io  thread time Math util 
   co Blackboard Language 
   Dialogs Look MapWindow

   ${FMT_LIB} ${CURL_TARGET} ${CARES_TARGET} ${ZLIB_TARGET}
   ${SODIUM_TARGET}  # new at 06/2020
   ${SSL_LIB}  # new at 03/2021
   ${CRYPTO_LIB}  # new at 03/2021
)

# Win32!
if(WIN32) 
    list(APPEND _SOURCES
        ${SRC}/ui/canvas/gdi/Canvas.cpp
        ${SRC}/ui/canvas/gdi/Bitmap.cpp
        ${SRC}/ui/canvas/gdi/GdiPlusBitmap.cpp
        ${SRC}/ui/canvas/gdi/ResourceBitmap.cpp
    )
    list(APPEND OPENVARIOBASEMENU_LIBS
        msimg32
        winmm
        ws2_32
        gdiplus
    )
endif()
list(APPEND _SOURCES
# FakeLogFile        ${SRC}/LogFile.cpp

        ${SRC}/ProgressGlue.cpp
        ${SRC}/ProgressWindow.cpp

        ${SRC}/Form/DigitEntry.cpp

        ${SRC}/io/FileOutputStream.cxx
        
        ${SRC}/lib/fmt/SystemError.cxx
        ${SRC}/Units/Descriptor.cpp

        ${SRC}/ResourceLoader.cpp

###     ${SRC}/Dialogs/ProcessDialog.cpp
###	    ${SRC}/Kobo/FakeSymbols.cpp
###	    ${SRC}/ui/control/TerminalWindow.cpp

        ${SRC}/Interface.cpp
        ${SRC}/Blackboard/InterfaceBlackboard.cpp

        # ${SRC}/Operation/ConsoleOperationEnvironment.cpp
        ${SRC}/Operation/Operation.cpp

##        ${SRC}/MainWindow.cpp
)
set(SCRIPT_FILES
    CMakeSource.cmake
    ../../build/ov.mk
)
