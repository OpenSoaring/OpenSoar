set(TEST_SRC_DIR "${PROJECTGROUP_SOURCE_DIR}/test/src")

set(_SOURCES
    Kobo/WifiDialog.cpp
    Kobo/WPASupplicant.cpp
)
set (KOBO_MENU_SOURCES
    ${SRC}/Kobo/WPASupplicant.cpp
    ${SRC}/Kobo/System.cpp
    ${SRC}/Kobo/Kernel.cpp
    ${SRC}/Kobo/NetworkDialog.cpp
    ${SRC}/Kobo/SystemDialog.cpp
    ${SRC}/Kobo/ToolsDialog.cpp
    ${SRC}/Kobo/WPASupplicant.cpp
    ${SRC}/Kobo/WifiDialog.cpp
    ${SRC}/Kobo/FakeSymbols.cpp
    ${SRC}/Kobo/KoboMenu.cpp
)
list (APPEND KOBO_MENU_SOURCES
    ${SRC}/Version.cpp
    ${SRC}/Asset.cpp
    ${SRC}/Formatter/HexColor.cpp
    ${SRC}/Hardware/CPU.cpp
    ${SRC}/Hardware/DisplayDPI.cpp
    ${SRC}/Hardware/RotateDisplay.cpp
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
    ${SRC}/Dialogs/Error.cpp
    ${SRC}/Dialogs/LockScreen.cpp
    ${SRC}/Dialogs/TextEntry.cpp
    ${SRC}/Dialogs/KnobTextEntry.cpp
    ${SRC}/Dialogs/TouchTextEntry.cpp
    ${SRC}/Dialogs/SimulatorPromptWindow.cpp
    ${TEST_SRC_DIR}/Fonts.cpp
    ${TEST_SRC_DIR}/FakeLanguage.cpp
    ${TEST_SRC_DIR}/FakeLogFile.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
    ../../build/kobo.mk
)
