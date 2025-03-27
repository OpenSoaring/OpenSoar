ifeq ($(HAVE_SKYSIGHT)$(SKYSIGHT_FORECAST),yy)
  NETCDF = y

  ifeq ($(TARGET),ANDROID)
    NETCDF_LDLIBS += -l:libnetcdf_c++.a -l:libnetcdf.a
  else
    ifeq ($(HAVE_WIN32),y)
      $(eval $(call pkg-config-library,NETCDF,netcdf-cxx4))
      $(eval $(call link-library,netcdfcpp,NETCDF))
      NETCDF_LDLIBS = -lnetcdf_c++ -lnetcdf
    else
      NETCDF_PREFIX = output/lib/x86_64/lib
      NETCDF_LDLIBS = -L$(NETCDF_PREFIX)/lib  -lnetcdf_c++ -lnetcdf
      NETCDF_CPPFLAGS = -isystem$(NETCDF_PREFIX)/include

    endif
  endif
  LDLIBS += $(NETCDF_LDLIBS)

else
  NETCDF = n
endif