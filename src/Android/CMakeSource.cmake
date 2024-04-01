
set(_SOURCES
	${SRC}/java/Global.cxx
	${SRC}/java/Object.cxx
	${SRC}/java/String.cxx
	${SRC}/java/Exception.cxx
	${SRC}/java/File.cxx
	${SRC}/java/Path.cxx
	${SRC}/java/InputStream.cxx
	${SRC}/java/URL.cxx
	${SRC}/java/Closeable.cxx
	
    ${SRC}/Device/AndroidSensors.cpp
	${SRC}/Device/Port/AndroidPort.cpp
	${SRC}/Device/Port/AndroidBluetoothPort.cpp
	${SRC}/Device/Port/AndroidIOIOUartPort.cpp
	${SRC}/Device/Port/AndroidUsbSerialPort.cpp
	
    NativeView.cpp
	Environment.cpp
	Bitmap.cpp
	Product.cpp
	InternalSensors.cpp
	SoundUtil.cpp
	TextUtil.cpp
	EventBridge.cpp
	NativePortListener.cpp
	NativeInputListener.cpp
	PortBridge.cpp
	Sensor.cpp
	BluetoothHelper.cpp
	NativeDetectDeviceListener.cpp
	NativeSensorListener.cpp
	Battery.cpp
	GliderLink.cpp
	DownloadManager.cpp
	Vibrator.cpp
	Context.cpp
	BMP085Device.cpp
	I2CbaroDevice.cpp
	NunchuckDevice.cpp
	VoltageDevice.cpp
	IOIOHelper.cpp
	UsbSerialHelper.cpp
	TextEntryDialog.cpp
	FileProvider.cpp
	Main.cpp
)

set(SCRIPT_FILES
    CMakeSource.cmake
)

# # # jetzt nicht mehr so ;-)  
# # # jetzt nicht mehr so ;-)  
# # # jetzt nicht mehr so ;-)  add_library(${LIB_TARGET_NAME} ${XCSOAR_LIB_TYPE}
# # # jetzt nicht mehr so ;-)      ${SOURCE_FILES}
# # # jetzt nicht mehr so ;-)      ${HEADER_FILES}
# # # jetzt nicht mehr so ;-)      ${CMAKE_CURRENT_LIST_DIR}/CMakeSource.cmake
# # # jetzt nicht mehr so ;-)      ${SCRIPT_FILES}
# # # jetzt nicht mehr so ;-)  )
# # # jetzt nicht mehr so ;-)  # message(FATAL_ERROR "Stop!")
# # # jetzt nicht mehr so ;-)  
# # # jetzt nicht mehr so ;-)  set_target_properties(${LIB_TARGET_NAME} PROPERTIES FOLDER Libs)
# # # jetzt nicht mehr so ;-)  
# # # jetzt nicht mehr so ;-)  target_link_libraries(${LIB_TARGET_NAME} PUBLIC IO)
# # # jetzt nicht mehr so ;-)  
# # # jetzt nicht mehr so ;-)  endif()
