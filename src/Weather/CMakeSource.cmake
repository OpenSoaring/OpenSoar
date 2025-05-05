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
        Weather/Skysight/Skysight.cpp
        Weather/Skysight/SkysightAPI.cpp
        Weather/Skysight/SkysightRegions.cpp
        Weather/Skysight/APIQueue.cpp

        Weather/Skysight/SkysightRenderer.cpp
        Weather/Skysight/SkySightRequest.cpp
  )
  if(SKYSIGHT_FORECAST)
    list(APPEND _SOURCES Weather/Skysight/CDFDecoder.cpp    )
  endif()
endif()

set(SCRIPT_FILES
    CMakeSource.cmake
)

