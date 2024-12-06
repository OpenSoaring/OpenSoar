# set(TARGET_NAME "XCSoarAug-MinGW")  # hardcoded yet
set(TARGET_NAME "OpenSoar-MinGW")  # hardcoded yet

message(STATUS "+++ System = WIN32 / MinGW (${TOOLCHAIN})  on ${CMAKE_HOST_SYSTEM_NAME} !!!")

add_compile_definitions(DEBUG_CONSOLE_OUTPUT)

set(LIB_PREFIX "lib" )  # "lib")
set(LIB_SUFFIX ".a")    # "a")

set(TARGET_IS_OPENVARIO OFF)  # no OpenVario menu
if (TARGET_IS_OPENVARIO)
  add_compile_definitions(IS_OPENVARIO) 
endif()

add_compile_definitions(BOOST_ASIO_SEPARATE_COMPILATION)
add_compile_definitions(BOOST_MATH_DISABLE_DEPRECATED_03_WARNING=ON)
        # add_compile_definitions(HAVE_MSVCRT)
# add_compile_definitions(UNICODE)  # ???
# add_compile_definitions(_UNICODE)
add_compile_definitions(STRICT)
add_compile_definitions(_USE_MATH_DEFINES)   # necessary under C++17!
add_compile_definitions(ZZIP_1_H)   # definition of uint32_t and Co.!
add_compile_definitions(_WIN32_WINNT_WIN10=0x0A00)
add_compile_definitions(_WIN32_WINDOWS=_WIN32_WINNT_WIN10)

# string(APPEND CMAKE_CXX_FLAGS " -Og -funit-at-a-time -ffast-math -g -std=c++20 -fno-threadsafe-statics -fmerge-all-constants -fcoroutines -fconserve-space -fno-operator-names -fvisibility=hidden -finput-charset=utf-8 -Wall -Wextra -Wwrite-strings -Wcast-qual -Wpointer-arith -Wsign-compare -Wundef -Wmissing-declarations -Wredundant-decls -Wmissing-noreturn -Wvla -Wno-format-truncation -Wno-missing-field-initializers -Wcast-align -Werror -I./src/unix -I./_build/include -isystem /usr/lib/link_libs/boost/boost-1.80.0 ")
# string(APPEND CMAKE_CXX_FLAGS " -c -Og -funit-at-a-time -ffast-math -g -std=c++20 -fno-threadsafe-statics -fmerge-all-constants -fcoroutines -fconserve-space -fno-operator-names -finput-charset=utf-8 -Wall -Wextra -Wwrite-strings -Wcast-qual -Wpointer-arith -Wsign-compare -Wundef -Wmissing-declarations -Wredundant-decls -Wmissing-noreturn -Wvla -Wno-format-truncation -Wno-missing-field-initializers -Wcast-align -Werror -m64 -mwin32 -mwindows -mms-bitfields")

# 2024-12-12: w/o '-c': string(APPEND CMAKE_CXX_FLAGS   " -c -Og -funit-at-a-time -ffast-math -g")
string(APPEND CMAKE_CXX_FLAGS   " -Og -funit-at-a-time -ffast-math -g")
if (1)
  string(APPEND CMAKE_CXX_FLAGS   " -fno-threadsafe-statics")
  string(APPEND CMAKE_CXX_FLAGS   " -fmerge-all-constants ")
  string(APPEND CMAKE_CXX_FLAGS   " -fconserve-space")
  string(APPEND CMAKE_CXX_FLAGS   " -fno-operator-names")
  string(APPEND CMAKE_CXX_FLAGS   " -finput-charset=utf-8")
  string(APPEND CMAKE_CXX_FLAGS   " -std=c++20")    # C++20
  string(APPEND CMAKE_CXX_FLAGS   " -fcoroutines")  # use CoRoutines
  string(APPEND CMAKE_CXX_FLAGS   " -Wall")
  string(APPEND CMAKE_CXX_FLAGS   " -Wextra")
  string(APPEND CMAKE_CXX_FLAGS   " -Wwrite-strings")
  string(APPEND CMAKE_CXX_FLAGS   " -Wcast-qual")
  string(APPEND CMAKE_CXX_FLAGS   " -Wpointer-arith")
  # string(APPEND CMAKE_CXX_FLAGS   " -Wsign-compare")  # OpenSoaring\OpenSoar\src\OpenVario\DisplaySettingsWidget.cpp:108:24: error: comparison of integer expressions of different signedness: 'int' and 'unsigned int'
  # string(APPEND CMAKE_CXX_FLAGS   " -Wundef") # boost/config/platform/win32.hpp:81:59: warning: "WINAPI_FAMILY_PHONE_APP" is not defined
  # string(APPEND CMAKE_CXX_FLAGS   " -Wmissing-declarations") # boost/json/impl/pointer.ipp:96:6: error: no previous declaration for ...
  # string(APPEND CMAKE_CXX_FLAGS   " -Wredundant-decls")
  string(APPEND CMAKE_CXX_FLAGS   " -Wmissing-noreturn")
  string(APPEND CMAKE_CXX_FLAGS   " -Wvla")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-format-truncation")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-missing-field-initializers")
  string(APPEND CMAKE_CXX_FLAGS   " -Wcast-align")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-cpp")  # disable the warning 'Please include winsock2.h before windows.h'
  string(APPEND CMAKE_CXX_FLAGS   " -Werror")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=cast-qual")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=cast-align")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=redundant-decls")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=undef")

  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=unused-parameter") # OpenSoaring\OpenSoar\src\OpenVario
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=sign-compare") # OpenSoaring\OpenSoar\src\OpenVario
  # OpenVario:  !! ?? !!
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=type-limits")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=unused-function")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=unused-variable")

  # string(APPEND CMAKE_CXX_FLAGS " -Wno-error=XXX")
  # 2025-01-27:
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=maybe-uninitialized")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=stringop-truncation")
  # string(APPEND CMAKE_CXX_FLAGS   " -Wno-error=array-bounds")
  string(APPEND CMAKE_CXX_FLAGS   " -Wno-strict-aliasing")
  #------------------------
  string(APPEND CMAKE_CXX_FLAGS   " -m64")
  string(APPEND CMAKE_CXX_FLAGS   " -mwin32")
  string(APPEND CMAKE_CXX_FLAGS   " -mwindows")  # use the GUI instead of console!  "Option: -mwindows"
  string(APPEND CMAKE_CXX_FLAGS   " -mms-bitfields")
else (0)
  list(APPEND CMAKE_CXX_FLAGS "-fno-threadsafe-statics")
  list(APPEND CMAKE_CXX_FLAGS "-fmerge-all-constants ")
  list(APPEND CMAKE_CXX_FLAGS "-fconserve-space")
  list(APPEND CMAKE_CXX_FLAGS "-fno-operator-names")
  list(APPEND CMAKE_CXX_FLAGS "-finput-charset=utf-8")
  list(APPEND CMAKE_CXX_FLAGS "-std=c++20")    # C++20
  list(APPEND CMAKE_CXX_FLAGS "-fcoroutines")  # use CoRoutines
  list(APPEND CMAKE_CXX_FLAGS "-Wall")
  list(APPEND CMAKE_CXX_FLAGS "-Wextra")
  list(APPEND CMAKE_CXX_FLAGS "-Wwrite-strings")
  list(APPEND CMAKE_CXX_FLAGS "-Wcast-qual")
  list(APPEND CMAKE_CXX_FLAGS "-Wpointer-arith")
  list(APPEND CMAKE_CXX_FLAGS "-Wsign-compare")
  # list(APPEND CMAKE_CXX_FLAGS "-Wundef") # boost/config/platform/win32.hpp:81:59: warning: "WINAPI_FAMILY_PHONE_APP" is not defined
  list(APPEND CMAKE_CXX_FLAGS "-Wmissing-declarations")
  # list(APPEND CMAKE_CXX_FLAGS "-Wredundant-decls")
  list(APPEND CMAKE_CXX_FLAGS "-Wmissing-noreturn")
  list(APPEND CMAKE_CXX_FLAGS "-Wvla")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-format-truncation")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-missing-field-initializers")
  list(APPEND CMAKE_CXX_FLAGS "-Wcast-align")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-cpp")  # disable the warning 'Please include winsock2.h before windows.h'
  list(APPEND CMAKE_CXX_FLAGS "-Werror")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=cast-qual")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=cast-align")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=redundant-decls")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=undef")

  # OpenVario:  !! ?? !!
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=type-limits")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=unused-function")
  list(APPEND CMAKE_CXX_FLAGS "-Wno-error=unused-variable")

  list(APPEND CMAKE_CXX_FLAGS "-m64")
  list(APPEND CMAKE_CXX_FLAGS "-mwin32")
  # list(APPEND CMAKE_CXX_FLAGS " -Wno-error=XXX")
  list(APPEND CMAKE_CXX_FLAGS "-mwindows")  # use the GUI instead of console!  "Option: -mwindows"
  list(APPEND CMAKE_CXX_FLAGS "-mms-bitfields")
endif(0)

list(APPEND LINK_OPTIONS "-mwindows")  # use the GUI instead of console!  "Option: -mwindows"

# set(VERBOSE_CXX ON)
if(VERBOSE_CXX)
    string(APPEND CMAKE_CXX_FLAGS " -v")  # verbose..
endif()

if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL Windows)
    include_directories("D:/Programs/MinGW/${TOOLCHAIN}/include")
    #  later: include_directories("${PROJECTGROUP_SOURCE_DIR}/output/include")
    include_directories(${LINK_LIBS}/boost/boost-1.85.0/include/boost-1_85)
else()
include_directories(
       /usr/include
       /usr/include/x86_64-linux-gnu
       /usr/lib/gcc/x86_64-w64-mingw32/10-win32/include
       #  later: include_directories("${PROJECTGROUP_SOURCE_DIR}/output/include")
       include_directories("${PROJECTGROUP_SOURCE_DIR}/output/src/boost_1_85_0")
)

endif()
#######################################################################
      # list(APPEND XCSOAR_LINK_LIBRARIES
      set(BASIC_LINK_LIBRARIES
        msimg32
        winmm
        # dl
        pthread
        stdc++
        user32
        gdi32
        gdiplus
        ws2_32
        mswsock
        kernel32
        # ?? msvcrt32
        shell32
        gcc_s
      )

set(MINGW ON)
add_compile_definitions(__MINGW__)
#********************************************************************************
if(AUGUST_SPECIAL)
    add_compile_definitions(_AUG_MGW)
endif()
#********************************************************************************
# set(CMAKE_C_FLAGS    "${CMAKE_C_FLAGS}")
string (APPEND CMAKE_CXX_STANDARD_LIBRARIES " -static-libgcc -static-libstdc++ -m64")
string (APPEND CMAKE_EXE_LINKER_FLAGS       " -static -static-libstdc++ -Wl,-Bstatic,--whole-archive -Wl,--no-whole-archive -v")
# string (APPEND CMAKE_EXE_LINKER_FLAGS       " -v")

# set(CMAKE_CXX_STANDARD_LIBRARIES "-static-libgcc -static-libstdc++ -m64 -lwsock32 -lws2_32 -lgdi32 -lgdiplus -lcrypt32 ${CMAKE_CXX_STANDARD_LIBRARIES}")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libstdc++ -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive -v")
    
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL Windows)
    set(SSL_LIBS)
    set(CRYPTO_LIBS Crypt32.lib BCrypt.lib) # no (OpenSSL-)crypto lib on windows!
endif()

set(PERCENT_CHAR \%)
set(DOLLAR_CHAR  $$)

# if(EXISTS "D:/Programs")  # on Windows - and on Flaps6 (August2111)
if (${CMAKE_HOST_SYSTEM_NAME} STREQUAL Windows)
    list(APPEND CMAKE_PROGRAM_PATH "D:/Programs")
endif()
