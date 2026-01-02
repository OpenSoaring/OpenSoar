# D:\Projects\OpenSoaring\OpenSoar\3rd_party\3rd_party.cmake
# ./3rd_party/3rd_party.cmake
# =================================================================

 ### BOOST:
set(BOOST_VERSION       "1.87.0") # 
set(BOOST_VERSION       "1.90.0") # 2025-12-10

 ### ZLIB:
set(ZLIB_VERSION        "1.2.11")
set(ZLIB_VERSION        "1.3")

 ### CARES:
# 25.12.23 
# set(CARES_VERSION       "1.17.1")  # old version necessary...
# set(CARES_VERSION       "1.24.0")  # XCSoar version () # not valid?
set(CARES_VERSION       "1.34.6")  # 2025-12-08

### CURL:
set(CURL_VERSION        "7.5.0")
set(CURL_VERSION        "8.1.2")
set(CURL_VERSION        "8.2.1")
set(CURL_VERSION        "8.5.0")  # functional with OpenSoar(2025-12-23)
set(CURL_VERSION        "8.12.1")

### PNG:
#  set(PNG_VERSION         "1.6.40") 
set(PNG_VERSION         "1.6.43") # XCSoar version

### SODIUM:
set(SODIUM_VERSION      "1.0.20") # 2024-05-25 (XCSoar since ...)
if (${TOOLCHAIN} MATCHES "mgw112")
  set(SODIUM_VERSION      "1.0.18") # 2025-12-22: 1.0.20 klappt mit mgw112 nicht
endif()

### LUA:
set(LUA_VERSION         "5.4.4")  # "5.4.6") 08.11.2024: sollte gehen...

### FMT:
set(FMT_VERSION         "10.2.1")
set(FMT_VERSION         "11.1.4") # github latest


if (1)  # SkySight
  set(TIFF_VERSION        "4.6.0")
  
  set(GEOTIFF_VERSION     "1.7.1")  # functional with OpenSoar(2025-12-23)
  # set(GEOTIFF_VERSION     "1.7.2")  # 2024-0?-?? - missing CURL::libcurl_static!?!
  # set(GEOTIFF_VERSION     "1.7.3")  # 2025-??-?? - missing CURL::libcurl_static!?!
  set(GEOTIFF_VERSION     "1.7.4")  # 2025-02-20
  
  ## For SQLite we need the download of zip file from sources, otherwize on Windows (from github) 
  ## we need the 'configure' cmd (but this is not working)
  set(SQLITE3_VERSION      "3.46.1")
  set(SQLITE3_VERSION      "3.47.0")
  set(SQLITE3_VERSION      "3.42.0")
  set(SQLITE3_VERSION      "3.50.4")
  set(SQLITE3_VERSION      "3.51.1")
endif() 

  set(PROJ_VERSION        "9.3.1")  # 9.3.1 ->  9.4.1
  set(PROJ_VERSION        "9.4.1")  # 9.3.1 ->  9.4.1  # functional with OpenSoar(2025-12-23)?
  # set(PROJ_VERSION        "9.7.1")  # 9.3.1 ->  9.4.1

  set(NETCDF_C_VERSION    "4.6.2")  # 2028-11-19 # functional with OpenSoar(2025-12-23)
  set(NETCDF_C_VERSION    "4.7.4")  # 2020-03-27
  # set(NETCDF_C_VERSION    "4.8.1")  # 2021-08-18
  # set(NETCDF_C_VERSION    "4.9.1")  # 2023-03-02
  # set(NETCDF_C_VERSION    "4.9.2")
  # set(NETCDF_C_VERSION    "4.9.3")  # 2025-02-07

  # set(NETCDF_CXX_VERSION  "4.2.1")  # is without cmake (starts with 4.3.1)
  set(NETCDF_CXX_VERSION  "4.3.1")  # functional with OpenSoar(2025-12-23)
  set(NETCDF_CXX_VERSION  "main")  # check 2025-12-28

  # set(HDF5_VERSION        "1.14.4.3")  # ??
if (0)
# # #  set(HDF5_VERSION        "1.14.5")  # ?? 
# # #  set(SZIP_VERSION        "2.1")    # New 02.12.2024??? Not used up to now...
set(INKSCAPE_VERSION    "1.2.1")
set(FREEGLUT_VERSION    "3.2.2")
set(SDL_VERSION         "2.28.5")  # for OpenGL...
set(GLM_VERSION         "0.9.9.8")  # GL Mathematics for OpenGL...
# set(RSVG_VERSION        "2.55.1")
if (NO_MSVC)
    set(XLST_VERSION        "1.1.37")
    set(XML2_VERSION        "2.10.2")
    set(ICONV_VERSION       "1.17")
endif()

set(XMLPARSER_VERSION "1.08")
endif()

### 2025-12-31 remove???  if (NO_MSVC)
### 2025-12-31 remove??? ##    set(TIFF_VERSION        "4.5.1")
### 2025-12-31 remove???     set(JPEG_VERSION        "3.0.0")
### 2025-12-31 remove???     # set(PROJ_VERSION        "9.3.1") 
### 2025-12-31 remove???     # set(PROJ_VERSION        "9.7.1")
### 2025-12-31 remove???     set(FREETYPE_VERSION    "2.13.1")
### 2025-12-31 remove???     set(OPENSSL_VERSION     "3.1.2")
### 2025-12-31 remove???     set(UPSTREAM_VERSION    "8.0.1")
### 2025-12-31 remove???     set(GCC_VERSION         "13.2.0")
### 2025-12-31 remove???     set(BINUTILS_VERSION    "2.41")
### 2025-12-31 remove??? endif()

# 3rd-party! 
if(JASPER_OUTSIDE)
    set(JASPER_VERSION      "2.0.33")  #"25")
else()
    set(JASPER_LIB "jasper")
endif()

# set(XCSOAR_MAPSERVER_VERSION "mapserver-0.0.1")
set(XCSOAR_MAPSERVER_VERSION "mapserver-xcsoar")

# 3rd-party! 
if (ZZIP_OUTSIDE)
    # set(XCSOAR_ZZIP_VERSION "zzip-0.0.1")
    # set(XCSOAR_ZZIP_VERSION "zzip-0.36c")
    set(XCSOAR_ZZIP_VERSION "zzip-xcsoar")
else()
    set(ZZIP_LIB "zzip")
endif()

#=========================
# August2111: Why that????
set(WITH_3RD_PARTY ON)

if (MSVC)    # VisualStudio:
    message(STATUS "+++ 3rd party System: MSVC!")
elseif(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(STATUS "+++ 3rd party System: WinClang!")
elseif (MINGW) # MinGW
    message(STATUS "+++ 3rd party System: MINGW!")
else()
    message(FATAL_ERROR "+++ 3rd party System: Unknown!!!")
endif()
#=========================

