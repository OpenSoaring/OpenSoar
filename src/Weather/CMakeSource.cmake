set(_SOURCES
        Weather/METARParser.cpp
        Weather/NOAADownloader.cpp
        Weather/NOAAFormatter.cpp
        Weather/NOAAGlue.cpp
        Weather/NOAAStore.cpp
        Weather/NOAAUpdater.cpp
        Weather/PCMet/Images.cpp
        Weather/PCMet/Overlays.cpp
#        Weather/Rasp/Providers.cpp
        Weather/Rasp/RaspCache.cpp
        Weather/Rasp/RaspRenderer.cpp
        Weather/Rasp/RaspStore.cpp
        Weather/Rasp/RaspStyle.cpp
        Weather/Rasp/Configured.cpp
)

if(HAVE_SKYSIGHT)
  list(APPEND _SOURCES
       # SkySight:
        # Weather/Skysight/Skysight.cpp  # [topic/skysight-delta] not in upstream XCSoar yet
        # Weather/Skysight/SkysightAPI.cpp  # [topic/skysight-delta] not in upstream XCSoar yet
        # Weather/Skysight/SkysightRegions.cpp  # [topic/skysight-delta] not in upstream XCSoar yet
        # Weather/Skysight/APIQueue.cpp  # [topic/skysight-delta] not in upstream XCSoar yet

        # Weather/Skysight/SkysightRenderer.cpp  # [topic/skysight-delta] not in upstream XCSoar yet
        # Weather/Skysight/SkySightRequest.cpp  # [topic/skysight-delta] not in upstream XCSoar yet
  )
  if(SKYSIGHT_FORECAST)
    # list(APPEND _SOURCES Weather/Skysight/CDFDecoder.cpp    )  # [topic/skysight-delta] not in upstream XCSoar yet
  endif()
endif()

set(SCRIPT_FILES
    CMakeSource.cmake
)


# --- synced from upstream XCSoar (v7.44 -> master, 2026-08-13) ---
list(APPEND _SOURCES
        Weather/BackgroundDownloadProgress.cpp
        Weather/MapOverlay/ControlsFactory.cpp
        Weather/MapOverlay/ControlsWidget.cpp
        Weather/MapOverlay/CursorBarLabels.cpp
        Weather/MapOverlay/InputEvents.cpp
        Weather/MapOverlay/RaspControlsModel.cpp
        Weather/MapOverlay/SkySightControlsModel.cpp
        Weather/MapOverlay/TimePicker.cpp
        Weather/MapOverlay/WeatherSetupDialog.cpp
        Weather/MapOverlay/XcthermControlsModel.cpp
        Weather/Rasp/ColorMap.cpp
        Weather/Rasp/DownloadGlue.cpp
        Weather/Rasp/FieldControls.cpp
        Weather/Rasp/RaspStylesData.cpp
        Weather/xctherm/FieldControls.cpp
        Weather/xctherm/XCThermAPI.cpp
        Weather/xctherm/XCThermAutoSwitch.cpp
        Weather/xctherm/XCThermCatalog.cpp
        Weather/xctherm/XCThermControlsModel.cpp
        Weather/xctherm/XCThermDownload.cpp
        Weather/xctherm/XCThermDownloadGlue.cpp
        Weather/xctherm/XCThermForecastTime.cpp
        Weather/xctherm/XCThermGeoJSON.cpp
        Weather/xctherm/XCThermGeoJSONOverlay.cpp
        Weather/xctherm/XCThermGeoQuery.cpp
        Weather/xctherm/XCThermMapOverlay.cpp
)
if(HAVE_SKYSIGHT)
  list(APPEND _SOURCES
        Weather/SkySight/FieldControls.cpp
        Weather/SkySight/SkySightAPI.cpp
        Weather/SkySight/SkySightCache.cpp
        Weather/SkySight/SkySightClient.cpp
        Weather/SkySight/SkySightFileDecoder.cpp
        Weather/SkySight/SkySightRequest.cpp
  )
endif(HAVE_SKYSIGHT)
if(ENABLE_OPENGL)  # EDL overlay needs OpenGL (see Weather/Features.hpp)
  list(APPEND _SOURCES
         Weather/MapOverlay/EdlControlsModel.cpp
         Weather/EDL/DownloadGlue.cpp
         Weather/EDL/EdlMbTilesOverlay.cpp
         Weather/EDL/FieldControls.cpp
         Weather/EDL/Glue.cpp
         Weather/EDL/Levels.cpp
         Weather/EDL/StateController.cpp
         Weather/EDL/TileStore.cpp
         Weather/EDL/TileValue.cpp
  )
endif(ENABLE_OPENGL)
