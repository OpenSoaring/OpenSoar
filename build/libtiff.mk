ifeq ($(HAVE_SKYSIGHT),y)
    GEOTIFF ?= y

    $(eval $(call pkg-config-library,LIBTIFF,libtiff-4))
    LIBTIFF_CPPFLAGS += -DUSE_LIBTIFF
  
    ifeq ($(GEOTIFF),y)
      LIBTIFF_CPPFLAGS += -DUSE_GEOTIFF
      ifneq ($(USE_THIRDPARTY_LIBS),y)
        LIBTIFF_CPPFLAGS += -isystem /usr/include/geotiff
      endif
      LIBTIFF_LDLIBS += -ltiff -lgeotiff
    endif
  
    ifeq ($(GEOTIFF)$(USE_THIRDPARTY_LIBS),yy)
      $(eval $(call pkg-config-library,PROJ,proj))
      LIBTIFF_CPPFLAGS += $(PROJ_CPPFLAGS)
      LIBTIFF_LDLIBS += $(PROJ_LDLIBS) -lsqlite3 -lproj
    endif
else
  GEOTIFF ?= n
endif

LDLIBS += $(LIBTIFF_LDLIBS)
TIFF ?= $(GEOTIFF)
