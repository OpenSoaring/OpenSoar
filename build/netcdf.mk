ifeq ($(HAVE_SKYSIGHT)$(SKYSIGHT_FORECAST),yy)
  $(info OpenSoar is using SKYSIGHT_FORECAST)

  NETCDF = y

  ifeq ($(TARGET),ANDROID)
    NETCDF_LDLIBS += -l:libnetcdf_c++.a -l:libnetcdf.a
  else
    ifeq ($(HAVE_WIN32),y)
      $(eval $(call pkg-config-library,NETCDF,netcdf-cxx4))
      $(eval $(call link-library,netcdfcpp,NETCDF))
      NETCDF_LDLIBS = -lnetcdf_c++ -lnetcdf
    else
      $(eval $(call pkg-config-library,NETCDF,netcdf-cxx4))
      $(eval $(call link-library,netcdfcpp,NETCDF))
      NETCDF_LDLIBS = -lnetcdf_c++4 -lnetcdf
    endif
  endif
  LDLIBS += $(NETCDF_LDLIBS)

else
  NETCDF = n
endif