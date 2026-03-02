ifeq ($(HAVE_SKYSIGHT)$(SKYSIGHT_FORECAST),yy)
  $(info OpenSoar is using SKYSIGHT_FORECAST)

  NETCDF = y

  ifeq ($(USE_THIRDPARTY_LIBS),y)
    $(eval $(call pkg-config-library,NETCDF,netcdf-cxx4))
    $(eval $(call link-library,netcdfcpp,NETCDF))
    NETCDF_LDLIBS = -lnetcdf-cxx4 -lnetcdf
  else
    NETCDF_LDLIBS = -lnetcdf_c++4 -lnetcdf
  endif
  LDLIBS += $(NETCDF_LDLIBS)

else
  NETCDF = n
endif