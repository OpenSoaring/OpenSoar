# libudev for the Linux USB / serial hotplug monitor
# (src/Device/PortMonitorLinux.{hpp,cpp}).
#
# Available on every modern Linux distro as part of systemd-udev. Not
# applicable to Windows, macOS, Android, Kobo.

ifeq ($(TARGET_IS_LINUX),y)
ifneq ($(TARGET_IS_KOBO),y)

$(eval $(call pkg-config-library,LIBUDEV,libudev))

# Tell the C++ code that libudev is available.
LIBUDEV_CPPFLAGS += -DHAVE_LIBUDEV

# Make the PortMonitorLinux translation unit pick up libudev headers.
$(call SRC_TO_OBJ,$(SRC)/Device/PortMonitorLinux.cpp): CPPFLAGS += $(LIBUDEV_CPPFLAGS)
$(call SRC_TO_OBJ,$(SRC)/BackendComponents.cpp): CPPFLAGS += $(LIBUDEV_CPPFLAGS)
$(call SRC_TO_OBJ,$(SRC)/Startup.cpp): CPPFLAGS += $(LIBUDEV_CPPFLAGS)

# Linker needs -ludev for the final executable.
TARGET_LDLIBS += $(LIBUDEV_LDLIBS)

endif
endif
