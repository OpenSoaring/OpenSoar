# D:\Projects\OpenSoaring\OpenSoar\3rd_party\3rd_party.cmake
# ./3rd_party/3rd_party.cmake
# =================================================================

set(BOOST_VERSION       "1.87.0")
set(CARES_VERSION       "1.24.0")  # not valid!
# 25.12.23 
set(CARES_VERSION       "1.17.1")  # old version necessary...
set(CURL_VERSION        "8.5.0")
set(PNG_VERSION         "1.6.40")
set(SODIUM_VERSION      "1.0.18")
set(LUA_VERSION         "5.4.4")  # "5.4.6") 08.11.2024: sollte gehen...
set(FMT_VERSION         "10.2.1")

if (1) # SkySight
  set(TIFF_VERSION        "4.6.0")
  set(GEOTIFF_VERSION     "1.7.1")  # oder 1.7.3 ??
  set(PROJ_VERSION        "9.3.1")  # 9.3.1 ->  9.4.1
  set(NETCDF_C_VERSION    "4.6.2")  # 4.6.2 -> 4.9.2 ??
  set(NETCDF_CXX_VERSION  "4.3.1")  # 4.2.1 -> 4.3.1 ??

  # set(HDF5_VERSION        "1.14.4.3")  # ??
  set(HDF5_VERSION        "1.14.5")  # ?? 

  # set(SQLITE_VERSION      "3.46.1")
  # set(SQLITE_YEAR         2024)
  # set(URL_HASH )
  set(SQLITE_VERSION      "3.47.0")
  set(SQLITE_YEAR         2024)
  # set(URL_HASH            23075baddabe87d609251bbb66884f218dea9e185dea3178fe20eecc5337a6bb)
  set(SQLITE_HASH         e35ee48efc24fe58d0e9102034ac0a41e3904641a745e94ab11a841fe9d7355e)
endif() 

if (NO_MSVC)
##    set(TIFF_VERSION        "4.5.1")
    set(JPEG_VERSION        "3.0.0")
    set(PROJ_VERSION        "9.3.1")
    set(FREETYPE_VERSION    "2.13.1")
    set(OPENSSL_VERSION     "3.1.2")
    set(UPSTREAM_VERSION    "8.0.1")
    set(GCC_VERSION         "13.2.0")
    set(BINUTILS_VERSION    "2.41")
endif()


set(ZLIB_VERSION        "1.3")
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

set(XCSOAR_XMLPARSER_VERSION "xml-1.08")
set(XMLPARSER_VERSION "1.08")

#=========================
# August2111: Why that????
if (MSVC)    # VisualStudio:
    message(STATUS "+++ 3rd party System: MSVC!")
    set(WITH_3RD_PARTY ON)
    set(LIB_PREFIX "" )
    set(LIB_SUFFIX ".lib")
    set(EXE_PREFIX ".exe")
elseif(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
    message(STATUS "+++ 3rd party System: WinClang!")
    set(WITH_3RD_PARTY ON)
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".a")

    set(EXE_PREFIX ".exe")

    # set(TOOLCHAIN clang14)  # ??? 2022-09-08: necessary???
    set(LINK_LIBS D:/Projects/link_libs)
elseif (MINGW) # MinGW
    message(STATUS "+++ 3rd party System: MINGW!")
    set(WITH_3RD_PARTY ON)
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".a")
    set(EXE_PREFIX ".exe")
else()
#     message(FATAL_ERROR "+++ unknown (3rd party-)System: ${CMAKE_SYSTEM}, Compiler = ${CMAKE_CXX_COMPILER_ID} !")
    set(WITH_3RD_PARTY ON)
    set(LIB_PREFIX "lib")
    set(LIB_SUFFIX ".a")
    set(EXE_PREFIX "")
endif()
#=========================

