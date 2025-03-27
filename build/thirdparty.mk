TARGET_IS_UNIX = n
ifeq ($(TARGET_IS_KOBO),y)
  USE_THIRDPARTY_LIBS = y
else ifeq ($(TARGET),PC)
  USE_THIRDPARTY_LIBS = y
else ifeq ($(TARGET),ANDROID)
  ifeq ($(FAT_BINARY),y)
    # this is handled by android.mk
    USE_THIRDPARTY_LIBS = n
  else
    USE_THIRDPARTY_LIBS = y
  endif
else ifeq ($(TARGET_IS_IOS),y)
  WRAPPED_CC = ccache clang++
  USE_THIRDPARTY_LIBS = y
else ifeq ($(TARGET_IS_DARWIN),y)
  WRAPPED_CC = ccache clang++
  # and not IOS!
  # HOST_TRIPLET = aarch64-apple-darwin (arm) or x86_64-apple-darwin (intel)
  USE_THIRDPARTY_LIBS = y
else
  # UNIX???
  HOST_TRIPLET = x86_64-linux-gnu
  USE_THIRDPARTY_LIBS = y
  TARGET_IS_UNIX = y
endif

ifeq ($(USE_THIRDPARTY_LIBS),y)

# -Wl,--gc-sections breaks the (Kobo) glibc build
THIRDPARTY_LDFLAGS_FILTER_OUT = -L$(THIRDPARTY_LIBS_DIR)/% -Wl,--gc-sections

THIRDPARTY_LIBS_DIR = $(ARCH_OUTPUT_DIR)/lib
THIRDPARTY_LIBS_ROOT = $(THIRDPARTY_LIBS_DIR)/$(HOST_TRIPLET)

.PHONY: libs
libs: $(THIRDPARTY_LIBS_DIR)/stamp

compile-depends += $(THIRDPARTY_LIBS_DIR)/stamp
$(THIRDPARTY_LIBS_DIR)/stamp:
	./build/thirdparty.py $(THIRDPARTY_LIBS_DIR) "$(HOST_TRIPLET)" "$(TARGET_ARCH)" "$(TARGET_CPPFLAGS)" "$(filter-out $(THIRDPARTY_LDFLAGS_FILTER_OUT),$(TARGET_LDFLAGS))" "$(WRAPPED_CC)" "$(WRAPPED_CXX)" $(AR) "$(ARFLAGS)" $(RANLIB) $(STRIP) "$(WINDRES)"
	touch $@

ifeq ($(TARGET_IS_KOBO),n)
TARGET_CPPFLAGS += -isystem $(THIRDPARTY_LIBS_ROOT)/include
TARGET_LDFLAGS += -L$(THIRDPARTY_LIBS_ROOT)/lib
endif

ifeq ($(TARGET_IS_UNIX),y)
    # reset USE_THIRDPARTY_LIBS for later access
    USE_THIRDPARTY_LIBS = n
else ifeq ($(TARGET_IS_DARWIN)$(TARGET_IS_IOS),yn)
    # reset USE_THIRDPARTY_LIBS for later access
    USE_THIRDPARTY_LIBS = n
endif

endif

ifeq ($(TARGET_IS_KOBO),y)
  # we build a toolchain as part of the thirdparty-library build
  BUILD_TOOLCHAIN_TARGET = $(THIRDPARTY_LIBS_DIR)/stamp
else
  BUILD_TOOLCHAIN_TARGET =
endif

compile-depends += boost
