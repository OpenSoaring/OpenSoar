ifeq ($(HAVE_SKYSIGHT),y)
  NETCDF = y

  ifeq ($(TARGET),ANDROID)
    NETCDF_LDLIBS += -l:libnetcdf_c++.a -l:libnetcdf.a
  else
    $(eval $(call pkg-config-library,NETCDF,netcdf-cxx4))
    $(eval $(call link-library,netcdfcpp,NETCDF))
    NETCDF_LDLIBS = -lnetcdf_c++4 -lnetcdf
  endif
  LDLIBS += $(NETCDF_LDLIBS)

else
  NETCDF = n
endif