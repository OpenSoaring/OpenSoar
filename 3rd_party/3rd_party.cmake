# D:\Projects\OpenSoaring\OpenSoar\3rd_party\3rd_party.cmake
# ./3rd_party/3rd_party.cmake
# =================================================================
set(BOOST_VERSION       "1.90.0") # (2025-12-10)
set(ZLIB_VERSION        "1.3.1" )  # 2025-..
set(CARES_VERSION       "1.24.0")  # XCSoar version () # not valid?
set(CURL_VERSION        "8.5.0" )  # functional with OpenSoar(2025-12-23)
set(PNG_VERSION         "1.6.43") # XCSoar version
if (${TOOLCHAIN} MATCHES "mgw*" )
  set(SODIUM_VERSION      "1.0.18") # 
else()
  set(SODIUM_VERSION      "1.0.21") # 2026-01-06
endif()
set(LUA_VERSION         "5.4.6" )  # 2023-05-15 (XCSoar since ...)
set(FMT_VERSION         "11.1.4") # 2025-02-26
set(TIFF_VERSION        "4.6.0" ) # XCSoar version

if (HAVE_SKYSIGHT)  # SkySight
  ## For SQLite we need the download of zip file from sources, otherwise on Windows (from github) 
  ## we need the 'configure' cmd (but this is not working)
  set(SQLITE3_VERSION   "3.51.1")
  set(PROJ_VERSION      "9.4.1" ) # functional with OpenSoar(2025-12-23)?
  set(GEOTIFF_VERSION   "1.7.4" )  # 2025-02-20, XCSoar: 2025-04-25
  if (SKYSIGHT_FORECAST)  # HAVE_SKYSIGHT)   # SkySight!
      set(NETCDF_C_VERSION    "4.10.0")  # 2025-??
      set(NETCDF_CXX_VERSION  "4.3.1")  # functional with OpenSoar(2025-12-23)
  endif(SKYSIGHT_FORECAST)  # HAVE_SKYSIGHT)   # SkySight!
endif() 
