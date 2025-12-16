cmake_minimum_required(VERSION 3.15)
if (SHOW_SUBPROJECTS)
    message(STATUS "+++ Start CMake ${CMAKE_CURRENT_SOURCE_DIR}!")
endif()

get_filename_component(TARGET_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

set(TEST_APPLICATION ON)
set(SRC_DIR ${PROJECTGROUP_SOURCE_DIR}/test/src)
include(CMakeSource.cmake)

foreach(source_file ${TEST_LIST})
    ## CMake Debug:  message(STATUS "+++ Add Test-Project to test: ${source_file} ")

    add_compile_definitions("__STDC_VERSION__= 199901L") # ?
    get_filename_component(test ${source_file} NAME_WE)
    get_filename_component(test_path ${source_file} DIRECTORY)

    list(APPEND source_file ${SRC_DIR}/DebugPort.cpp)
    list(APPEND source_file ${PROJECTGROUP_SOURCE_DIR}/src/Operation/ConsoleOperationEnvironment.cpp)
    add_executable(${test}  ${source_file} 
      ${test_path}/tap.c 
      # ${test_path}/test_debug.cpp  needs getopt
      ${test_path}/FakeLogFile.cpp 
      ${SCRIPT_FILES}
    )
    set_target_properties(${test} PROPERTIES 
                FOLDER _Test/Console
                EXCLUDE_FROM_ALL TRUE
    )
    target_link_libraries(${test} PUBLIC ${XCSOAR_LINK_LIBRARIES})

    add_dependencies(${test} util libOpenSoar)

#     message(STATUS "### ${test_path}  --- ${test}")
endforeach()

foreach(source_file ${GUI_TEST_LIST})
    # show inserted test in cmake output:
    # message(STATUS "+++ Add (Gui-)Test-Project to test: ${source_file} ")

    if (TEST_APPLICATION)
      add_compile_definitions("GUITEST_APPLICATION")
    endif (TEST_APPLICATION)
    add_compile_definitions("__STDC_VERSION__= 199901L") # ?
    get_filename_component(test ${source_file} NAME_WE)
    get_filename_component(test_path ${source_file} DIRECTORY)

    list(APPEND source_file ${SRC_DIR}/DebugPort.cpp)
    list(APPEND source_file ${PROJECTGROUP_SOURCE_DIR}/src/Operation/ConsoleOperationEnvironment.cpp)
    list(APPEND source_file ${SRC_DIR}/Fonts.cpp)
    add_executable(${test}  ${source_file} 
      ${test_path}/tap.c 
      ${SCRIPT_FILES}
    )
    set_target_properties(${test} PROPERTIES 
                FOLDER _Test/Gui
                EXCLUDE_FROM_ALL TRUE
    )
    if(MSVC)
        target_link_options(${test} PUBLIC "/SUBSYSTEM:WINDOWS")
    endif()
    target_link_libraries(${test} PUBLIC ${XCSOAR_LINK_LIBRARIES} )

    add_dependencies(${test} util libOpenSoar)

endforeach()

set (USE_RUNMAPWINDOW OFF)
if (USE_RUNMAPWINDOW)

set(TEST_NAME RunMapWindow)
# this is a trial to make the RunMapWindow similar to the make project
set (TEST_SRC_DIR ${SRC_DIR})
set (ENGINE_SRC_DIR ${SRC}/Engine)
set (IO_SRC_DIR ${SRC}/io)

set( RUN_MAP_WINDOW_SOURCES
	${ENGINE_SRC_DIR}/Contest/Settings.cpp 
	${SRC}/Engine/Util/Gradient.cpp 
	${SRC}/Engine/Trace/Point.cpp 
	${SRC}/Engine/Trace/Trace.cpp 
	${SRC}/Engine/Trace/Vector.cpp 
	${SRC}/Engine/Navigation/Aircraft.cpp 
	${ENGINE_SRC_DIR}/ThermalBand/ThermalBand.cpp 
	${ENGINE_SRC_DIR}/ThermalBand/ThermalSlice.cpp 
	${IO_SRC_DIR}/MapFile.cpp 
	${IO_SRC_DIR}/DataFile.cpp 
	${IO_SRC_DIR}/ConfiguredFile.cpp 
	${SRC}/Engine/Navigation/TraceHistory.cpp 
	${SRC}/FLARM/Id.cpp 
	${SRC}/FLARM/Friends.cpp 
	${SRC}/FLARM/List.cpp 
	${SRC}/FLARM/Global.cpp 
	${SRC}/Airspace/ActivePredicate.cpp 
	${SRC}/Airspace/ProtectedAirspaceWarningManager.cpp 
	${SRC}/Airspace/AirspaceParser.cpp 
	${SRC}/Airspace/AirspaceVisibility.cpp 
	${SRC}/Airspace/AirspaceComputerSettings.cpp 
	${SRC}/Renderer/GeoBitmapRenderer.cpp 
	${SRC}/Renderer/TransparentRendererCache.cpp 
	${SRC}/Renderer/AirspaceRendererSettings.cpp 
	${SRC}/Renderer/BackgroundRenderer.cpp 
	${SRC}/LocalPath.cpp 
	${SRC}/Projection/Projection.cpp 
	${SRC}/Projection/WindowProjection.cpp 
	${SRC}/Projection/CompareProjection.cpp 
	${SRC}/Weather/Rasp/RaspStore.cpp 
	${SRC}/Weather/Rasp/RaspCache.cpp 
	${SRC}/Weather/Rasp/RaspRenderer.cpp 
	${SRC}/Weather/Rasp/RaspStyle.cpp 
	${SRC}/Renderer/FAITriangleAreaRenderer.cpp 
	${SRC}/Renderer/OZRenderer.cpp 
	${SRC}/Renderer/TaskRenderer.cpp 
	${SRC}/Renderer/TaskPointRenderer.cpp 
	${SRC}/Renderer/AircraftRenderer.cpp 
	${SRC}/Renderer/AirspaceRenderer.cpp 
	${SRC}/Renderer/AirspaceRendererGL.cpp 
	${SRC}/Renderer/AirspaceRendererOther.cpp 
	${SRC}/Renderer/AirspaceLabelList.cpp 
	${SRC}/Renderer/AirspaceLabelRenderer.cpp 
	${SRC}/Renderer/BestCruiseArrowRenderer.cpp 
	${SRC}/Renderer/CompassRenderer.cpp 
	${SRC}/Renderer/FinalGlideBarRenderer.cpp 
	${SRC}/Renderer/TrackLineRenderer.cpp 
	${SRC}/Renderer/TrafficRenderer.cpp 
	${SRC}/Renderer/TrailRenderer.cpp 
	${SRC}/Renderer/WaypointIconRenderer.cpp 
	${SRC}/Renderer/WaypointRenderer.cpp 
	${SRC}/Renderer/WaypointRendererSettings.cpp 
	${SRC}/Renderer/WaypointLabelList.cpp 
	${SRC}/Renderer/WindArrowRenderer.cpp 
	${SRC}/Renderer/WaveRenderer.cpp 
	${SRC}/Math/Screen.cpp 
	${MORE_SCREEN_SOURCES}
	${SRC}/Renderer/LabelBlock.cpp 
	${SRC}/Renderer/TextInBox.cpp 
	${SRC}/UISettings.cpp 
	${SRC}/Audio/Settings.cpp 
	${SRC}/Audio/VarioSettings.cpp 
	${SRC}/DisplaySettings.cpp 
	${SRC}/PageSettings.cpp 
	${SRC}/InfoBoxes/InfoBoxSettings.cpp 
	${SRC}/Dialogs/DialogSettings.cpp 
	${SRC}/Gauge/VarioSettings.cpp 
	${SRC}/Gauge/TrafficSettings.cpp 
	${SRC}/MapSettings.cpp 
	${SRC}/Computer/Settings.cpp 
	${SRC}/Computer/Wind/Settings.cpp 
	${SRC}/TeamCode/Settings.cpp 
	${SRC}/Logger/Settings.cpp 
	${SRC}/Computer/TraceComputer.cpp 
	${SRC}/IGC/IGCParser.cpp 
	${SRC}/Task/ProtectedRoutePlanner.cpp 
	${SRC}/Task/RoutePlannerGlue.cpp 
	${SRC}/Units/Units.cpp 
	${SRC}/Units/Settings.cpp 
	${SRC}/Formatter/Units.cpp 
	${SRC}/Formatter/UserUnits.cpp 
	${SRC}/Formatter/AirspaceUserUnitsFormatter.cpp 
	${SRC}/Formatter/HexColor.cpp 
	${SRC}/Profile/Profile.cpp 
	${SRC}/Profile/ComputerProfile.cpp 
	${SRC}/Profile/TaskProfile.cpp 
	${SRC}/Profile/RouteProfile.cpp 
	${SRC}/Profile/ContestProfile.cpp 
	${SRC}/Profile/AirspaceConfig.cpp 
	${SRC}/Profile/TrackingProfile.cpp 
	${SRC}/Profile/WeatherProfile.cpp 
	${SRC}/Profile/MapProfile.cpp 
	${SRC}/Profile/TerrainConfig.cpp 
	${SRC}/Profile/Screen.cpp 
	${SRC}/Profile/FlarmProfile.cpp 
	${SRC}/Waypoint/HomeGlue.cpp 
	${SRC}/Waypoint/LastUsed.cpp 
	${SRC}/Waypoint/WaypointGlue.cpp 
	${SRC}/Waypoint/Factory.cpp 
	${SRC}/Compatibility/fmode.c 
	${SRC}/RadioFrequency.cpp 
	${SRC}/Atmosphere/Pressure.cpp 
	${SRC}/Atmosphere/AirDensity.cpp 
	${SRC}/Operation/ConsoleOperationEnvironment.cpp 

    ${TEST_SRC_DIR}/DebugPort.cpp
	${TEST_SRC_DIR}/Fonts.cpp 
	${TEST_SRC_DIR}/FakeAsset.cpp 
	${TEST_SRC_DIR}/FakeDialogs.cpp 
	# ${TEST_SRC_DIR}/FakeLanguage.cpp 
	${TEST_SRC_DIR}/FakeLogFile.cpp 
	${TEST_SRC_DIR}/RunMapWindow.cpp

	${SRC}/ResourceLoader.cpp 

)
# if (${HAVE_HTTP})
list(APPEND RUN_MAP_WINDOW_SOURCES
	${SRC}/Weather/NOAAGlue.cpp 
	${SRC}/Weather/NOAAStore.cpp
)
# endif()

set(RUN_MAP_WINDOW_DEPENDS 
	LIBMAPWINDOW 
	PROFILE TERRAIN TOPO 
	FORM 
	LOOK 
	SCREEN EVENT 
	RESOURCE 
	OPERATION 
	ASYNC OS IO THREAD 
	TASK ROUTE GLIDE WAYPOINT WAYPOINTFILE AIRSPACE 
	JASPER ZZIP LIBNMEA GEO MATH TIME UTIL
)
# ${eval ${call link-program,RunMapWindow,RUN_MAP_WINDOW))

    # list(APPEND RUN_MAP_WINDOW_SOURCES ${SRC_DIR}/DebugPort.cpp)
    # list(APPEND RUN_MAP_WINDOW_SOURCES ${PROJECTGROUP_SOURCE_DIR}/src/Operation/ConsoleOperationEnvironment.cpp)
    # list(APPEND RUN_MAP_WINDOW_SOURCES ${SRC_DIR}/Fonts.cpp)
    add_executable(${TEST_NAME}  ${RUN_MAP_WINDOW_SOURCES} 
      ${test_path}/tap.c 
      # ${SCRIPT_FILES}
    )
    set_target_properties(${TEST_NAME}  PROPERTIES 
                FOLDER _GuiTest
                EXCLUDE_FROM_ALL TRUE
    )
 set (RUN_MAP_LINK_LIBS ) ## ${XCSOAR_LINK_LIBRARIES} )
 # list(REMOVE_ITEM RUN_MAP_LINK_LIBS libOpenSoar)

 foreach(lib ${XCSOAR_LINK_LIBRARIES})
    get_filename_component(test ${lib} NAME_WE)
    if (${test} MATCHES "libOpenSoar")
       message(STATUS "--- Remove Lib ${test}: ${lib} ")
    elseif (${test} MATCHES "Dialogs")
        message(STATUS "--- Remove Lib ${test}: ${lib} ")
   else()
       list(APPEND RUN_MAP_LINK_LIBS ${lib} )
       message(STATUS "### Add Lib ${test}: ${lib} ")
    endif()
  endforeach()

 if(MSVC)
     target_link_options(${TEST_NAME} PUBLIC "/SUBSYSTEM:WINDOWS")
 endif()
target_link_libraries(${TEST_NAME} PUBLIC ${RUN_MAP_LINK_LIBS} )

# add_dependencies(${test} util libOpenSoar)
# add_dependencies(${test} util libOpenSoar)


endif()
