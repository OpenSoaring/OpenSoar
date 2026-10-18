# "make deb" packs the Linux build into a Debian package that users can
# install with "apt install ./OpenSoar-<version>-<arch>.deb".
#
# It packs the program that "make" built in this output directory, so
# the package holds exactly the binary that was tested, and it does not
# need the full dpkg-buildpackage machinery of debian/ (which builds the
# upstream "xcsoar" package from scratch).  The libraries are not copied
# into the package: dpkg-shlibdeps reads which shared libraries the
# binary needs and writes them as dependencies, and apt installs the
# missing ones together with the package.  That keeps the package small
# and lets the distribution update those libraries.
#
# The package is named after the brand in lower case ("opensoar"), so it
# can be installed next to an "xcsoar" package; the program keeps its
# file name /usr/bin/OpenSoar, and the message catalogues are called
# opensoar.mo, which do not clash with those of XCSoar either.

ifeq ($(TARGET),UNIX)
ifeq ($(TARGET_IS_KOBO),n)

DEB_PACKAGE = $(PROGRAM_NAME_LC)
DEB_ARCH = $(shell dpkg --print-architecture 2>/dev/null)

# A test version "7.45.25.t05" becomes "7.45.25~t05": the tilde sorts
# before everything else in Debian, so the final 7.45.25 counts as newer
# than all of its test versions and apt offers it as an upgrade.
DEB_VERSION = $(subst .t,~t,$(VERSION))

DEB_FILE = $(OUT)/$(PROGRAM_NAME)-$(VERSION)-$(DEB_ARCH).deb
DEB_WORK = $(TARGET_OUTPUT_DIR)/deb
DEB_ROOT = $(DEB_WORK)/root

# who answers for the package (override with "make deb DEB_MAINTAINER=...");
# not in OpenSoar.config, because shell scripts source that file and the
# spaces and angle brackets of an address would break them
DEB_MAINTAINER ?= Uwe Augustin <Info@FlapsOnline.de>

DEB_DEPENDS_EXTRA = ca-certificates, fonts-dejavu | fonts-roboto | fonts-droid-fallback

$(DEB_FILE): $(XCSOAR_BIN) $(MO_FILES) $(topdir)/Data/graphics/logo.svg \
	$(topdir)/COPYING $(topdir)/build/deb.mk
	@$(NQ)echo "  DEB     $@"
	$(Q)rm -rf $(DEB_WORK)
	$(Q)install -d -m 0755 $(DEB_ROOT)/DEBIAN $(DEB_ROOT)/usr/bin \
		$(DEB_ROOT)/usr/share/applications \
		$(DEB_ROOT)/usr/share/icons/hicolor/scalable/apps \
		$(DEB_ROOT)/usr/share/doc/$(DEB_PACKAGE)
	$(Q)install -m 0755 $(XCSOAR_BIN) $(DEB_ROOT)/usr/bin/$(PROGRAM_NAME)
	$(Q)$(STRIP) --strip-unneeded $(DEB_ROOT)/usr/bin/$(PROGRAM_NAME)
	$(Q)for i in $(LINGUAS); do \
		install -d -m 0755 $(DEB_ROOT)/usr/share/locale/$$i/LC_MESSAGES && \
		install -m 0644 $(OUT)/po/$$i.mo $(DEB_ROOT)/usr/share/locale/$$i/LC_MESSAGES/$(PROGRAM_NAME_LC).mo || exit 1; \
	done
	$(Q)install -m 0644 $(topdir)/Data/graphics/logo.svg \
		$(DEB_ROOT)/usr/share/icons/hicolor/scalable/apps/$(DEB_PACKAGE).svg
	$(Q)install -m 0644 $(topdir)/COPYING $(DEB_ROOT)/usr/share/doc/$(DEB_PACKAGE)/copyright
	$(Q)printf '%s\n' '[Desktop Entry]' 'Type=Application' \
		'Name=$(PRODUCT_NAME)' 'Comment=Glide computer' \
		'Exec=$(PROGRAM_NAME)' 'Icon=$(DEB_PACKAGE)' 'Terminal=false' \
		'Categories=Utility;Science;' \
		> $(DEB_ROOT)/usr/share/applications/$(DEB_PACKAGE).desktop
	@# dpkg-shlibdeps insists on a debian/control next to the files it
	@# examines; a two line one is enough for it
	$(Q)install -d -m 0755 $(DEB_WORK)/debian
	$(Q)printf 'Source: %s\n\nPackage: %s\nArchitecture: any\n' \
		$(DEB_PACKAGE) $(DEB_PACKAGE) > $(DEB_WORK)/debian/control
	$(Q)cd $(DEB_WORK) && dpkg-shlibdeps -O -e root/usr/bin/$(PROGRAM_NAME) \
		| sed -n 's/^shlibs:Depends=//p' > shlibs.depends
	$(Q)test -s $(DEB_WORK)/shlibs.depends
	$(Q)printf '%s\n' \
		'Package: $(DEB_PACKAGE)' \
		'Version: $(DEB_VERSION)' \
		'Architecture: $(DEB_ARCH)' \
		'Maintainer: $(DEB_MAINTAINER)' \
		"Installed-Size: $$(du -sk $(DEB_ROOT)/usr | cut -f1)" \
		"Depends: $$(cat $(DEB_WORK)/shlibs.depends), $(DEB_DEPENDS_EXTRA)" \
		'Section: misc' \
		'Priority: optional' \
		'Homepage: $(WEB_SITE_URL)' \
		'Description: $(PRODUCT_NAME) glide computer' \
		' $(PRODUCT_NAME) $(VERSION), a tactical glide computer for glider,' \
		' hang glider and paraglider pilots, based on XCSoar.' \
		> $(DEB_ROOT)/DEBIAN/control
	$(Q)dpkg-deb --root-owner-group --build $(DEB_ROOT) $@ >/dev/null

deb: $(DEB_FILE)

endif
endif
