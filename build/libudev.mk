# libudev for the Linux USB / serial hotplug monitor
# (src/Device/PortMonitorLinux.{hpp,cpp}).
#
# Available on every modern Linux distro as part of systemd-udev. Not
# applicable to Windows, macOS, Android, Kobo.
#
# Important on Android: TARGET_IS_LINUX is set to "y" because the
# Android kernel is Linux — but the NDK sysroot has no libudev.h, and
# pkg-config (the host one) would happily report the host libudev,
# leading to a cross-compile failure. So we ALSO require
# TARGET_IS_ANDROID != y here.
#
# We probe in three steps:
#   1. ask pkg-config (preferred — picks up custom sysroots, multilib)
#   2. fall back to looking for libudev.h in the default include path
#   3. give up silently — HAVE_LIBUDEV stays undefined, hotplug monitor
#      compiles to an empty translation unit and the build still links.

ifeq ($(TARGET_IS_LINUX),y)
ifneq ($(TARGET_IS_KOBO),y)
ifneq ($(TARGET_IS_ANDROID),y)

# Step 1: pkg-config probe. Use call-pkg-config which returns ERROR on
# miss instead of aborting the build (unlike pkg-config-library which
# $(error)s).
LIBUDEV_PKG_CFLAGS := $(call call-pkg-config,libudev,cflags)
LIBUDEV_PKG_LIBS   := $(call call-pkg-config,libudev,libs)

ifeq ($(filter ERROR,$(LIBUDEV_PKG_CFLAGS) $(LIBUDEV_PKG_LIBS)),)
  # pkg-config knows libudev — use its output.
  LIBUDEV_CPPFLAGS := $(call pkg-config-cppflags-filter,$(LIBUDEV_PKG_CFLAGS))
  LIBUDEV_LDLIBS   := $(call pkg-config-ldlibs-filter,$(LIBUDEV_PKG_LIBS))
  LIBUDEV_AVAILABLE := y
else
  # Step 2: pkg-config didn't find it. Look for the header in the
  # default toolchain include path (just /usr/include/libudev.h on a
  # normal Debian/Ubuntu after `apt install libudev-dev`).
  LIBUDEV_HEADER := $(wildcard /usr/include/libudev.h)
  ifneq ($(LIBUDEV_HEADER),)
    LIBUDEV_CPPFLAGS :=
    LIBUDEV_LDLIBS   := -ludev
    LIBUDEV_AVAILABLE := y
    $(info libudev: pkg-config miss, found /usr/include/libudev.h — falling back to -ludev)
  else
    LIBUDEV_AVAILABLE := n
    $(info libudev: not found, hotplug monitor disabled (install libudev-dev + pkg-config))
  endif
endif

ifeq ($(LIBUDEV_AVAILABLE),y)
  # Tell the C++ code that libudev is available.
  LIBUDEV_CPPFLAGS += -DHAVE_LIBUDEV

  # Make the affected translation units pick up the include path /
  # availability define.
  $(call SRC_TO_OBJ,$(SRC)/Device/PortMonitorLinux.cpp): CPPFLAGS += $(LIBUDEV_CPPFLAGS)
  $(call SRC_TO_OBJ,$(SRC)/BackendComponents.cpp): CPPFLAGS += $(LIBUDEV_CPPFLAGS)
  $(call SRC_TO_OBJ,$(SRC)/Startup.cpp): CPPFLAGS += $(LIBUDEV_CPPFLAGS)

  # Linker needs -ludev for the final executable.
  TARGET_LDLIBS += $(LIBUDEV_LDLIBS)
endif

endif # TARGET_IS_ANDROID != y
endif # TARGET_IS_KOBO != y
endif # TARGET_IS_LINUX == y
