# valid for which target?
ifeq (y,y)
    TARGET_CPPFLAGS += -DTWO_LOGO_APP
endif

ifeq ($(TARGET_IS_KOBO)$(TARGET_IS_DARWIN),nn)
    # not for KOBO, DARWIN - but for:
    # Android, UNIX, Windows, OpenVario...:
    $(info Features for  Android, UNIX, Windows, OpenVario...)

    # for build:
    HAVE_SKYSIGHT := y

    ifneq ($(TARGET_IS_OPENVARIO),y)
        HAVE_GEOTIFF = y
    endif
    
    ifeq ($(HAVE_SKYSIGHT),y)
        ifneq ($(TARGET_IS_OPENVARIO),y)
            SKYSIGHT_FORECAST := y
        endif
    
        # for cpp sources:
        TARGET_CPPFLAGS += -DHAVE_SKYSIGHT
        TARGET_CPPFLAGS += -DSKYSIGHT_LIVE

        ifeq ($(SKYSIGHT_FORECAST),y)
            TARGET_CPPFLAGS += -DSKYSIGHT_FORECAST
            TARGET_CPPFLAGS += -DSKYSIGHT_OFFLINE_MODE
        endif

        # debug feature for SkySight:
        # TARGET_CPPFLAGS += -DSKYSIGHT_FILE_DEBUG
        # TARGET_CPPFLAGS += -DSKYSIGHT_REQUEST_LOG
        # TARGET_CPPFLAGS += -DSKYSIGHT_HTTP_LOG

    endif  # HAVE_SKYSIGHT

else 
  # Kobo, MacOS, iOS,...:
  HAVE_SKYSIGHT := n
  HAVE_GEOTIFF := n
endif

# check if blanks beside 'n' or 'y':
# $(info HAVE_GEOTIFF = '$(HAVE_GEOTIFF)')
ifeq ($(HAVE_GEOTIFF),y)
   TARGET_CPPFLAGS += -DUSE_GEOTIFF
endif
