ifeq ($(TARGET_IS_KOBO)$(TARGET_IS_DARWIN),nn)
    # not for KOBO, DARWIN - but for:
    # Android, UNIX, Windows, OpenVario...:
    $(info Features for  Android, UNIX, Windows, OpenVario...)

    # for build:
    HAVE_SKYSIGHT := y

    ifneq ($(TARGET_IS_OPENVARIO),y)
        SKYSIGHT_FORECAST := y
        HAVE_GEOTIFF = y
    endif

    
    # for cpp sources:
    TARGET_CPPFLAGS += -DHAVE_SKYSIGHT
    TARGET_CPPFLAGS += -DSKYSIGHT_LIVE

    ifeq ($(SKYSIGHT_FORECAST),y)
        TARGET_CPPFLAGS += -DSKYSIGHT_FORECAST
    endif
else 
  # Kobo, MacOS, iOS,...:
  HAVE_SKYSIGHT := n
  HAVE_GEOTIFF := n
endif

ifeq ($(HAVE_GEOTIFF),y)
   TARGET_CPPFLAGS += -DUSE_GEOTIFF
endif

$(info HAVE_GEOTIFF = '$(HAVE_GEOTIFF)')
