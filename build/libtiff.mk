ifeq ($(HAVE_GEOTIFF),y)

    $(info libtiff is using HAVE_GEOTIFF)

    $(eval $(call pkg-config-library,LIBTIFF,libtiff-4))
    LIBTIFF_CPPFLAGS += -DUSE_LIBTIFF
    LIBTIFF_CPPFLAGS += -DUSE_GEOTIFF
  
    ifeq ($(USE_THIRDPARTY_LIBS),y)
      $(eval $(call pkg-config-library,PROJ,proj))
      LIBTIFF_CPPFLAGS += $(PROJ_CPPFLAGS)
      LIBTIFF_LDLIBS += $(PROJ_LDLIBS) -lsqlite3 -lproj
    else
      LIBTIFF_CPPFLAGS += -isystem /usr/include/geotiff
    endif
    LIBTIFF_LDLIBS += -ltiff -lgeotiff
else
  # w/o SkySight Forecast you don't need the GeoTiff
endif

LDLIBS += $(LIBTIFF_LDLIBS)
TIFF ?= $(HAVE_GEOTIFF)
