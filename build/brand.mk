# OpenSoar branding hook for the make-based targets (ANDROID, KOBO,
# UNIX, ...).
#
# The CMake build reads a <Brand>.config file in the repository root
# (currently OpenSoar.config) for product name, version and the
# Android identifiers.  This fragment feeds the SAME file into the
# make world, so there is exactly one source of truth.  Without a
# config file every variable keeps its XCSoar default - the hook is
# neutral for upstream trees.
#
# The config file is valid make syntax (KEY=value per line) and
# defines: PROGRAM_NAME, PROGRAM_VERSION, ANDROID_VERSIONCODE,
# ANDROID_PACKAGE and the optional ANDROID_ICON_BACKGROUND.
#
# Included early (from options.mk, before the TESTING default) and
# guarded against double inclusion; version.mk picks up BRAND_VERSION
# and BRAND_CPPFLAGS later.

ifndef BRAND_MK_INCLUDED
BRAND_MK_INCLUDED := y

BRAND_CONFIG := $(wildcard $(topdir)/OpenSoar.config)

ifneq ($(BRAND_CONFIG),)

include $(BRAND_CONFIG)

# product name: drives the binary name, PRODUCT_NAME/PRODUCT_NAME_LC
# defines (src/ProductName.hpp) and the strings.xml substitution.
#
# The config file spells it "PROGRAM_NAME", but in the make world that
# name belongs to the executable and is derived from PRODUCT_NAME
# later (main.mk, lower case on POSIX).  The config value must not
# survive, or a file included before main.mk sees the wrong one - the
# macOS app bundle did, and asked for "bin/OpenSoar" while the link
# rule produced "bin/opensoar".
PRODUCT_NAME := $(PROGRAM_NAME)
undefine PROGRAM_NAME

# version override for version.mk (instead of VERSION.txt).  Format
# major.minor.sub[.tN]; the numeric parsing ignores the test suffix
BRAND_VERSION := $(PROGRAM_VERSION)

# Android identifiers, maintained by hand in the config file
ANDROID_VERSION_CODE := $(ANDROID_VERSIONCODE)
ANDROID_VERSION_NAME := $(PROGRAM_VERSION)
BRAND_ANDROID_PACKAGE := $(ANDROID_PACKAGE)

# optional: background layer of the Android adaptive launcher icon
# (android/res/values*/ic_launcher.xml).  Upstream paints an opaque
# plate there; "00000000" restores the transparent icon OpenSoar has
# on the other platforms.  The value is AARRGGBB *without* the leading
# "#", which would start a comment in make syntax (and confuse the
# CMake reader of the same file).  Unset keeps the upstream resource.
BRAND_ANDROID_ICON_BACKGROUND := $(ANDROID_ICON_BACKGROUND)

# a ".tN" test version implies the red testing flavor, matching the
# CMake build and the CI tagging scheme (override with TESTING=n)
ifneq ($(findstring .t,$(PROGRAM_VERSION)),)
TESTING ?= y
endif

# branded Android builds use the plain package id from the config by
# default (upstream defaults Android builds to FOSS=y -> ".foss")
FOSS ?= n

# extra compile definitions (consumed via VERSION_CPPFLAGS in
# version.mk): the Android package for src/ProductName.hpp
BRAND_CPPFLAGS = -DANDROID_PACKAGE=\"$(BRAND_ANDROID_PACKAGE)\"

# the brand's own news file, shown as a page of the About dialog
ifneq ($(wildcard $(topdir)/OpenSoar-News.md),)
BRAND_CPPFLAGS += -DHAVE_BRAND_NEWS
endif

# the product's own web site (credits page, --help footer)
ifneq ($(WEB_SITE_URL),)
BRAND_CPPFLAGS += -DPRODUCT_WEB_SITE_URL=\"$(WEB_SITE_URL)\"
endif

# optional sponsor on the credits page: name and link from the config,
# the logo from Data/graphics/logo_sponsor.svg (when the file exists,
# HAVE_SPONSOR_LOGO adds the IDB_LOGO_SPONSOR_* resources)
ifneq ($(SPONSOR_NAME),)
BRAND_CPPFLAGS += -DSPONSOR_NAME=\"$(SPONSOR_NAME)\"
ifneq ($(SPONSOR_URL),)
BRAND_CPPFLAGS += -DSPONSOR_URL=\"$(SPONSOR_URL)\"
endif
ifneq ($(wildcard $(topdir)/Data/graphics/logo_sponsor.svg),)
HAVE_SPONSOR_LOGO = y
BRAND_CPPFLAGS += -DHAVE_SPONSOR_LOGO
endif
endif

endif

endif
