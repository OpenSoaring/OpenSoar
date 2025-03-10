ifeq ($(TARGET_IS_KOBO)$(TARGET_IS_DARWIN),nn)
  # not for KOBO, DARWIN - but for:
  # Android, UNIX, OV, Windows, OpenVario...:
    # for build:
    HAVE_SKYSIGHT := y
    # for cpp sources:
    TARGET_CPPFLAGS += -DUSE_GEOTIFF
    TARGET_CPPFLAGS += -DHAVE_SKYSIGHT
    TARGET_CPPFLAGS += -DSKYSIGHT_LIVE
else 
  # Kobo, MacOS, iOS,...:
  HAVE_SKYSIGHT := n
endif
