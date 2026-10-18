# "make dist" packs the program into a zip file that can be put on a web
# page as it is: unpacked anywhere, the program runs from that folder.
#
# The file is named after the program and the version of the build
# (OpenSoar-7.45.25.t05-Win64.zip), so that downloads of different
# versions do not overwrite each other and it stays clear which one a
# user has.  The Windows flavors get the names users know (Win32,
# Win64) instead of the make target names (PC, WIN64).

ifeq ($(TARGET_FLAVOR),PC)
DIST_PLATFORM = Win32
else ifeq ($(TARGET_FLAVOR),WIN64)
DIST_PLATFORM = Win64
else
DIST_PLATFORM = $(TARGET_FLAVOR)
endif

TARGET_DIST_NAME = $(PROGRAM_NAME)-$(VERSION)-$(DIST_PLATFORM)
TARGET_DIST_DIR = $(TARGET_OUTPUT_DIR)/dist
TARGET_DIST_ZIP = $(OUT)/$(TARGET_DIST_NAME).zip

DIST_DEPS = $(XCSOAR_BIN) $(VALI_XCS_BIN)

ifeq ($(HAVE_WIN32)$(FREETYPE),yy)
DIST_DEPS += $(FONT_TARGETS)
endif

# the OpenGL build for Windows renders through ANGLE, and Windows only
# starts the program when the two ANGLE DLLs lie next to it
ifeq ($(HAVE_WIN32)$(USE_ANGLE),yy)
DIST_DLLS = $(ANGLE_BIN_DLLS)
else
DIST_DLLS =
endif
DIST_DEPS += $(DIST_DLLS)

dist: $(DIST_DEPS)
	rm -rf $(TARGET_DIST_DIR)/$(TARGET_DIST_NAME)
	$(MKDIR) -p $(TARGET_DIST_DIR)/$(TARGET_DIST_NAME)
	cp $(XCSOAR_BIN) $(VALI_XCS_BIN) $(DIST_DLLS) COPYING \
		$(TARGET_DIST_DIR)/$(TARGET_DIST_NAME)/
ifeq ($(HAVE_WIN32)$(FREETYPE),yy)
	$(MKDIR) -p $(TARGET_DIST_DIR)/$(TARGET_DIST_NAME)/fonts
	cp $(FONT_TARGETS) \
		$(TARGET_DIST_DIR)/$(TARGET_DIST_NAME)/fonts/
endif
	rm -f $(TARGET_DIST_DIR)/$(TARGET_DIST_NAME).zip
	cd $(TARGET_DIST_DIR) && $(ZIP) -r $(TARGET_DIST_NAME).zip $(TARGET_DIST_NAME)
	mv $(TARGET_DIST_DIR)/$(TARGET_DIST_NAME).zip $(TARGET_DIST_ZIP)
