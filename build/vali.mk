# Rules for VALI-XCS.exe, the non-interactive G record validation tool


# Each app (target) can and should build and install `vali-xcs`; the issue 
# of multiple installations on OpenVario is resolved using Yocto's magic 
# "alternatives" mechanism.
VALI_XCS_SOURCES = \
	$(SRC)/Logger/GRecord.cpp \
	$(SRC)/util/MD5.cpp \
	$(SRC)/Version.cpp \
	$(SRC)/VALI-XCS.cpp
VALI_XCS_DEPENDS = IO OS UTIL
VALI_XCS_STRIP = y

$(eval $(call link-program,vali-xcs,VALI_XCS))
