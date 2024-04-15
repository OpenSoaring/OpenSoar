CONFIG = $(topdir)/OpenSoar.config
include $(CONFIG)

# w/o VERSION.txt:
ifeq ($(PROGRAM_VERSION),"")
    # take the version from XCSoar VERSION.txt
    PROGRAM_VERSION = $(strip $(shell cat $(topdir)/VERSION.txt))
endif
EXTRA_CPPFLAGS+= -DPROGRAM_VERSION=\"$(PROGRAM_VERSION)\" 

EXTRA_CPPFLAGS+=-DIS_OPENVARIO

DIALOG_SOURCES = \
	$(SRC)/Dialogs/Inflate.cpp \
	$(SRC)/Dialogs/Message.cpp \
	$(SRC)/Dialogs/LockScreen.cpp \
	$(SRC)/Dialogs/Error.cpp \
	$(SRC)/Dialogs/ListPicker.cpp \
	$(SRC)/Dialogs/ProgressDialog.cpp \
	$(SRC)/Dialogs/CoDialog.cpp \
	$(SRC)/Dialogs/JobDialog.cpp \
	$(SRC)/Dialogs/WidgetDialog.cpp \
	$(SRC)/Dialogs/FileManager.cpp \
	$(SRC)/Dialogs/Device/PortDataField.cpp \
	$(SRC)/Dialogs/Device/PortPicker.cpp \
	$(SRC)/Dialogs/Device/DeviceEditWidget.cpp \
	$(SRC)/Dialogs/Device/DeviceListDialog.cpp \
	$(SRC)/Dialogs/Device/PortMonitor.cpp \
	$(SRC)/Dialogs/Device/ManageCAI302Dialog.cpp \
	$(SRC)/Dialogs/Device/CAI302/UnitsEditor.cpp \
	$(SRC)/Dialogs/Device/CAI302/WaypointUploader.cpp \
	$(SRC)/Dialogs/Device/ManageFlarmDialog.cpp \
	$(SRC)/Dialogs/Device/BlueFly/BlueFlyConfigurationDialog.cpp \
	$(SRC)/Dialogs/Device/ManageI2CPitotDialog.cpp \
	$(SRC)/Dialogs/Device/LX/ManageLXNAVVarioDialog.cpp \
	$(SRC)/Dialogs/Device/LX/LXNAVVarioConfigWidget.cpp \
	$(SRC)/Dialogs/Device/LX/ManageNanoDialog.cpp \
	$(SRC)/Dialogs/Device/LX/NanoConfigWidget.cpp \
	$(SRC)/Dialogs/Device/LX/ManageLX16xxDialog.cpp \
	$(SRC)/Dialogs/Device/Vega/VegaParametersWidget.cpp \
	$(SRC)/Dialogs/Device/Vega/VegaConfigurationDialog.cpp \
	$(SRC)/Dialogs/Device/Vega/VegaDemoDialog.cpp \
	$(SRC)/Dialogs/Device/Vega/SwitchesDialog.cpp \
	$(SRC)/Dialogs/Device/FLARM/ConfigWidget.cpp \
	$(SRC)/Dialogs/MapItemListDialog.cpp \
	$(SRC)/Dialogs/MapItemListSettingsDialog.cpp \
	$(SRC)/Dialogs/MapItemListSettingsPanel.cpp \
	$(SRC)/Dialogs/ColorListDialog.cpp \
	$(SRC)/Dialogs/Airspace/dlgAirspace.cpp \
	$(SRC)/Dialogs/Airspace/dlgAirspacePatterns.cpp \
	$(SRC)/Dialogs/Airspace/dlgAirspaceDetails.cpp \
	$(SRC)/Dialogs/Airspace/AirspaceList.cpp \
	$(SRC)/Dialogs/Airspace/AirspaceCRendererSettingsDialog.cpp \
	$(SRC)/Dialogs/Airspace/AirspaceCRendererSettingsPanel.cpp \
	$(SRC)/Dialogs/Airspace/dlgAirspaceWarnings.cpp \
	$(SRC)/Dialogs/Settings/WindSettingsPanel.cpp \
	$(SRC)/Dialogs/Settings/WindSettingsDialog.cpp \
	$(SRC)/Dialogs/Settings/dlgBasicSettings.cpp \
	$(SRC)/Dialogs/Settings/dlgConfiguration.cpp \
	$(SRC)/Dialogs/Settings/dlgConfigInfoboxes.cpp \
	$(SRC)/Dialogs/Traffic/TrafficList.cpp \
	$(SRC)/Dialogs/Traffic/FlarmTrafficDetails.cpp \
	$(SRC)/Dialogs/Traffic/TeamCodeDialog.cpp \
	$(SRC)/Dialogs/dlgAnalysis.cpp \
	$(SRC)/Dialogs/dlgChecklist.cpp \
	$(SRC)/Dialogs/ProfileListDialog.cpp \
	$(SRC)/Dialogs/Plane/PlaneListDialog.cpp \
	$(SRC)/Dialogs/Plane/PlaneDetailsDialog.cpp \
	$(SRC)/Dialogs/Plane/PlanePolarDialog.cpp \
	$(SRC)/Dialogs/Plane/PolarShapeEditWidget.cpp \
	$(SRC)/Dialogs/DataField.cpp \
	$(SRC)/Dialogs/ComboPicker.cpp \
	$(SRC)/Dialogs/FilePicker.cpp \
	$(SRC)/Dialogs/HelpDialog.cpp \
	$(SRC)/Dialogs/dlgInfoBoxAccess.cpp \
	$(SRC)/Dialogs/ReplayDialog.cpp \
	$(SRC)/Dialogs/dlgSimulatorPrompt.cpp \
	$(SRC)/Dialogs/SimulatorPromptWindow.cpp \
	$(SRC)/Dialogs/StartupDialog.cpp \
	$(SRC)/Dialogs/ProfilePasswordDialog.cpp \
	\
	$(SRC)/Dialogs/dlgStatus.cpp \
	$(SRC)/Dialogs/StatusPanels/StatusPanel.cpp \
	$(SRC)/Dialogs/StatusPanels/FlightStatusPanel.cpp \
	$(SRC)/Dialogs/StatusPanels/SystemStatusPanel.cpp \
	$(SRC)/Dialogs/StatusPanels/TaskStatusPanel.cpp \
	$(SRC)/Dialogs/StatusPanels/RulesStatusPanel.cpp \
	$(SRC)/Dialogs/StatusPanels/TimesStatusPanel.cpp \
	\
	$(SRC)/Dialogs/Waypoint/WaypointInfoWidget.cpp \
	$(SRC)/Dialogs/Waypoint/WaypointCommandsWidget.cpp \
	$(SRC)/Dialogs/Waypoint/dlgWaypointDetails.cpp \
	$(SRC)/Dialogs/Waypoint/Manager.cpp \
	$(SRC)/Dialogs/Waypoint/dlgWaypointEdit.cpp \
	$(SRC)/Dialogs/Waypoint/WaypointList.cpp \
	$(SRC)/Dialogs/Waypoint/NearestWaypoint.cpp \
	\
	$(SRC)/Dialogs/Settings/Panels/AirspaceConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/GaugesConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/VarioConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/GlideComputerConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/WindConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/InfoBoxesConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/InterfaceConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/LayoutConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/LoggerConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/MapDisplayConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/PagesConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/RouteConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/SafetyFactorsConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/SiteConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/SymbolsConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/TaskRulesConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/TaskDefaultsConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/ScoringConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/TerrainDisplayConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/UnitsConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/TimeConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/WaypointDisplayConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/TrackingConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/CloudConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/WeatherConfigPanel.cpp \
	$(SRC)/Dialogs/Settings/Panels/WeGlideConfigPanel.cpp \
	\
	$(SRC)/Dialogs/Task/Widgets/ObservationZoneEditWidget.cpp \
	$(SRC)/Dialogs/Task/Widgets/CylinderZoneEditWidget.cpp \
	$(SRC)/Dialogs/Task/Widgets/LineSectorZoneEditWidget.cpp \
	$(SRC)/Dialogs/Task/Widgets/SectorZoneEditWidget.cpp \
	$(SRC)/Dialogs/Task/Widgets/KeyholeZoneEditWidget.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskMapButtonRenderer.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskManagerDialog.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskClosePanel.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskEditPanel.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskPropertiesPanel.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskMiscPanel.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskActionsPanel.cpp \
	$(SRC)/Dialogs/Task/Manager/TaskListPanel.cpp \
	$(SRC)/Dialogs/Task/Manager/WeGlideTasksPanel.cpp \
	$(SRC)/Dialogs/Task/OptionalStartsDialog.cpp \
	$(SRC)/Dialogs/Task/TaskPointDialog.cpp \
	$(SRC)/Dialogs/Task/MutateTaskPointDialog.cpp \
	$(SRC)/Dialogs/Task/dlgTaskHelpers.cpp \
	$(SRC)/Dialogs/Task/TargetDialog.cpp \
	$(SRC)/Dialogs/Task/AlternatesListDialog.cpp \
	\
	$(SRC)/Dialogs/Tracking/CloudEnableDialog.cpp \
	\
	$(SRC)/Dialogs/NumberEntry.cpp \
	$(SRC)/Dialogs/TextEntry.cpp \
	$(SRC)/Dialogs/KnobTextEntry.cpp \
	$(SRC)/Dialogs/TouchTextEntry.cpp \
	$(SRC)/Dialogs/TimeEntry.cpp \
	$(SRC)/Dialogs/DateEntry.cpp \
	$(SRC)/Dialogs/GeoPointEntry.cpp \
	$(SRC)/Dialogs/Weather/WeatherDialog.cpp \
	$(SRC)/Dialogs/Weather/RASPDialog.cpp \
	$(SRC)/Dialogs/dlgCredits.cpp \
	$(SRC)/Dialogs/dlgQuickMenu.cpp \
    \
	$(SRC)/Dialogs/DownloadFilePicker.cpp \

## ifeq ($(HAVE_HTTP),y)
DIALOG_SOURCES += \
	$(SRC)/Dialogs/DownloadFilePicker.cpp \
	$(SRC)/Repository/Glue.cpp \
	
##    $(SRC)/Renderer/NOAAListRenderer.cpp \
##	$(SRC)/Weather/PCMet/Images.cpp \
##	$(SRC)/Weather/PCMet/Overlays.cpp \
##	$(SRC)/Weather/NOAAGlue.cpp \
##	$(SRC)/Weather/METARParser.cpp \
##	$(SRC)/Weather/NOAAFormatter.cpp \
##	$(SRC)/Weather/NOAADownloader.cpp \
##	$(SRC)/Weather/NOAAStore.cpp \
##	$(SRC)/Weather/NOAAUpdater.cpp
## endif

OV_MENU_SOURCES = \
	$(DIALOG_SOURCES) \
	$(SRC)/OpenVario/OpenVarioBaseMenu.cpp \
	$(SRC)/OpenVario/System/OpenVarioDevice.cpp \
	$(SRC)/OpenVario/FileMenuWidget.cpp \
	$(SRC)/OpenVario/System/SystemMenuWidget.cpp \
	$(SRC)/OpenVario/DisplaySettingsWidget.cpp \
	$(SRC)/OpenVario/System/Setting/RotationWidget.cpp \
	$(SRC)/OpenVario/System/Setting/WifiWidget.cpp \
	\
	$(SRC)/OpenVario/System/WifiDialogOV.cpp \
	$(SRC)/OpenVario/System/WifiSupplicantOV.cpp \
	$(SRC)/OpenVario/System/WifiDBus.cpp \
	$(SRC)/OpenVario/System/NMConnector.cpp \
	\
	$(SRC)/OpenVario/SystemSettingsWidget.cpp \
	\
	$(SRC)/Version.cpp \
	$(SRC)/Asset.cpp \
	$(SRC)/Formatter/HexColor.cpp \
	$(SRC)/Formatter/TimeFormatter.cpp \
	$(SRC)/Hardware/CPU.cpp \
	$(SRC)/Hardware/DisplayDPI.cpp \
	$(SRC)/Hardware/RotateDisplay.cpp \
	$(SRC)/Hardware/DisplayGlue.cpp \
	$(SRC)/Screen/Layout.cpp \
	$(SRC)/ui/control/TerminalWindow.cpp \
	$(SRC)/Look/TerminalLook.cpp \
	$(SRC)/Look/DialogLook.cpp \
	$(SRC)/Look/ButtonLook.cpp \
	$(SRC)/Look/CheckBoxLook.cpp \
	$(SRC)/Renderer/TwoTextRowsRenderer.cpp \
	$(SRC)/Gauge/LogoView.cpp \
	$(SRC)/Dialogs/DialogSettings.cpp \
	$(SRC)/Dialogs/WidgetDialog.cpp \
	$(SRC)/Dialogs/HelpDialog.cpp \
	$(SRC)/Dialogs/Message.cpp \
	$(SRC)/Dialogs/LockScreen.cpp \
	$(SRC)/Dialogs/TextEntry.cpp \
	$(SRC)/Dialogs/KnobTextEntry.cpp \
	$(SRC)/Dialogs/TouchTextEntry.cpp \
	$(SRC)/Dialogs/ProcessDialog.cpp \
	$(SRC)/Profile/Map.cpp \
	$(SRC)/Profile/File.cpp \
	$(SRC)/Profile/NumericValue.cpp \
	$(TEST_SRC_DIR)/Fonts.cpp \
	$(SRC)/Language/Language.cpp \
    \
    $(SRC)/LocalPath.cpp \
	$(SRC)/LogFile.cpp \
    $(SRC)/Form/DigitEntry.cpp \
    $(SRC)/Renderer/TextRowRenderer.cpp \
    $(SRC)/net/http/DownloadManager.cpp \
    $(SRC)/Profile/Map.cpp \
    $(SRC)/Profile/File.cpp \
    $(SRC)/Profile/NumericValue.cpp \
    $(SRC)/Profile/Profile.cpp \
    $(SRC)/Profile/ProfileMap.cpp \
    \
	$(SRC)/ProgressWindow.cpp \
    \
    $(SRC)/ResourceLoader.cpp \
    $(SRC)/Repository/Parser.cpp \
    $(SRC)/ResourceLoader.cpp \
	$(SRC)/event/Call.cxx \
	$(SRC)/Math/FastTrig.cpp \
	$(SRC)/ui/window/ContainerWindow.cpp \
	\



OV_MENU_DEPENDS = WIDGET FORM DATA_FIELD SCREEN EVENT RESOURCE ASYNC LIBNET OS IO THREAD TIME MATH UTIL \
	LANGUAGE \
	LIBMAPWINDOW \
	GETTEXT \
    PROFILE \
	LOOK \
    LIBHTTP \
    CO OPERATION UNITS \
    DBUS

# $(TEST_SRC_DIR)/Fonts.cpp
# $(SRC)/Language/Language.cpp # $(TEST_SRC_DIR)/FakeLanguage.cpp
# $(SRC)/LocalPath.cpp 
# $(SRC)/LogFile.cpp           # $(TEST_SRC_DIR)/FakeLogFile.cpp 


###     $(SRC)/Profile/Profile.cpp \
###     $(SRC)/Profile/Map.cpp \
###     $(SRC)/Profile/ProfileMap.cpp \
###     $(SRC)/Profile/File.cpp \
###     $(SRC)/Profile/NumericValue.cpp \
###     $(SRC)/Profile/Current.cpp \

###	$(SRC)/Dialogs/ComboPicker.cpp \
###	$(SRC)/Dialogs/ListPicker.cpp \
###	$(SRC)/Dialogs/FilePicker.cpp \
###	$(SRC)/Dialogs/NumberEntry.cpp \
###	$(SRC)/Dialogs/TextEntry.cpp \
###	$(SRC)/Dialogs/KnobTextEntry.cpp \
###	$(SRC)/Dialogs/TouchTextEntry.cpp \
###	$(SRC)/Dialogs/TimeEntry.cpp \
###	$(SRC)/Dialogs/DateEntry.cpp \
###	$(SRC)/Dialogs/GeoPointEntry.cpp \
###    \
###    $(SRC)/Dialogs/DataField.cpp 


OV_MENU_STRIP = y

$(eval $(call link-program,OpenVarioBaseMenu,OV_MENU))

ifeq ($(TARGET),UNIX)
OPTIONAL_OUTPUTS += $(OV_MENU_BIN)
endif
