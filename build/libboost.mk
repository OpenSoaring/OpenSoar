BOOST_URL = https://archives.boost.io/release/1.87.0/source/boost_1_87_0.tar.bz2
BOOST_ALTERNATIVE_URL = https://sourceforge.net/projects/boost/files/boost/1.87.0/boost_1_87_0.tar.bz2/download
BOOST_MD5 = af57be25cb4c4f4b413ed692fe378affb4352ea50fbe294a11ef548f4d527d89

BOOST_TARBALL_NAME = $(notdir $(BOOST_URL))
BOOST_TARBALL = $(DOWNLOAD_DIR)/$(BOOST_TARBALL_NAME)
BOOST_BASE_NAME = $(patsubst %.tar.bz2,%,$(BOOST_TARBALL_NAME))
BOOST_SRC = $(OUT)/src/$(BOOST_BASE_NAME)
BOOST_PATCHES_DIR = $(topdir)/lib/boost/patches
BOOST_PATCHES = $(addprefix $(BOOST_PATCHES_DIR)/,$(shell cat $(BOOST_PATCHES_DIR)/series))

$(BOOST_TARBALL): | $(DOWNLOAD_DIR)/dirstamp
	@$(NQ)echo "  GET     $@"
	$(Q)./build/download.py $(BOOST_URL) $(BOOST_ALTERNATIVE_URL) $(BOOST_MD5) $(DOWNLOAD_DIR)

BOOST_UNTAR_STAMP = $(OUT)/src/stamp-$(BOOST_BASE_NAME)
$(BOOST_UNTAR_STAMP): $(BOOST_TARBALL) $(BOOST_PATCHES_DIR)/series $(BOOST_PATCHES) | $(OUT)/src/dirstamp
	@$(NQ)echo "  UNTAR   $(BOOST_TARBALL_NAME)"
	$(Q)rm -rf $(BOOST_SRC)
	$(Q)tar xjfC $< $(OUT)/src
	$(Q)cd $(BOOST_SRC) && QUILT_PATCHES=$(abspath $(BOOST_PATCHES_DIR)) quilt push -a -q
	@touch $@

.PHONY: boost
boost: $(BOOST_UNTAR_STAMP)

# We use only the header-only Boost libraries, so no linker flags
# required.
BOOST_LDLIBS =

# reduce Boost header bloat a bit
BOOST_CPPFLAGS = -isystem $(OUT)/src/$(BOOST_BASE_NAME)
BOOST_CPPFLAGS += -DBOOST_NO_IOSTREAM -DBOOST_MATH_NO_LEXICAL_CAST
BOOST_CPPFLAGS += -DBOOST_UBLAS_NO_STD_CERR
BOOST_CPPFLAGS += -DBOOST_ERROR_CODE_HEADER_ONLY
BOOST_CPPFLAGS += -DBOOST_SYSTEM_NO_DEPRECATED
BOOST_CPPFLAGS += -DBOOST_NO_STD_LOCALE -DBOOST_LEXICAL_CAST_ASSUME_C_LOCALE

ifeq ($(HAVE_WIN32),y)
    BOOST_CPPFLAGS += -DBOOST_SYSTEM_DISABLE_THREADS
endif

ifeq ($(TARGET_IS_OSX),y)
    ifeq ($(HOST_TRIPLET), x86_64-apple-darwin)
      BOOST_CPPFLAGS += -D__cpp_sized_deallocation=0
      BOOST_CPPFLAGS += -DBOOST_FT_AUTODETECT_CALLING_CONVENTIONS
      
      BOOST_CPPFLAGS += -Wno-missing-noreturn     
      BOOST_CPPFLAGS += -DBOOST_GCC_VERSION=__GNUC__*10000
      BOOST_CPPFLAGS += -Wno-cast-align

      BOOST_CPPFLAGS += -DBOOST_FT_CC_IMPLICIT=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_CDECL=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_STDCALL=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_PASCAL=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_FASTCALL=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_CLRCALL=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_THISCALL=0
      BOOST_CPPFLAGS += -DBOOST_FT_CC_IMPLICIT_THISCALL=0
    endif
endif

# Prevent Boost from using the deprecated std::unary_function class
BOOST_CPPFLAGS += -DBOOST_NO_CXX98_FUNCTION_BASE
