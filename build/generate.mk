INCLUDES += -I$(OUT)/include

PERL = perl
RANDOM_NUMBER := $(shell od -vAn -N4 -tu4 < /dev/urandom| tr -d ' ')

$(OUT)/include/MathTables.h: $(HOST_OUTPUT_DIR)/tools/GenerateSineTables$(HOST_EXEEXT) | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	# Generate into a temporary file to avoid leaving a truncated/empty header on interruption
	$(Q)$(HOST_OUTPUT_DIR)/tools/GenerateSineTables$(HOST_EXEEXT) >$@.$(RANDOM_NUMBER).tmp
	# Basic validation: file must be non-empty and contain an expected token.
	@if [ ! -s "$@.$(RANDOM_NUMBER).tmp" ]; then \
	  echo "Generation failed: $@.$(RANDOM_NUMBER).tmp is empty" >&2; \
	  rm -f "$@.$(RANDOM_NUMBER).tmp"; \
	  exit 1; \
	fi
	@if ! grep -q 'THERMALRECENCY{' "$@.$(RANDOM_NUMBER).tmp"; then \
	  echo "Generation failed: expected token THERMALRECENCY{ not found in $@.$(RANDOM_NUMBER).tmp" >&2; \
	  rm -f "$@.$(RANDOM_NUMBER).tmp"; \
	  exit 1; \
	fi
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(call SRC_TO_OBJ,$(SRC)/Math/FastMath.cpp): $(OUT)/include/MathTables.h
$(call SRC_TO_OBJ,$(SRC)/Math/FastTrig.cpp): $(OUT)/include/MathTables.h
$(call SRC_TO_OBJ,$(SRC)/Computer/ThermalRecency.cpp): $(OUT)/include/MathTables.h

# Quick Guide: compile-time Markdown slice of NEWS.txt (headings + list items)
$(OUT)/include/QuickGuideNEWS.hpp: $(topdir)/NEWS.txt \
	$(topdir)/tools/news_to_quickguide_md.py | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)/usr/bin/env python3 $(topdir)/tools/news_to_quickguide_md.py \
		$(topdir)/NEWS.txt >$@.$(RANDOM_NUMBER).tmp
	$(Q)mv $@.$(RANDOM_NUMBER).tmp $@

$(call SRC_TO_OBJ,$(SRC)/Dialogs/dlgQuickGuide.cpp): $(OUT)/include/QuickGuideNEWS.hpp

$(OUT)/include/InputEvents_Text2Event.hpp: $(SRC)/Input/InputEvents.hpp \
	$(topdir)/tools/Text2Event.pl | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) $(topdir)/tools/Text2Event.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(OUT)/include/InputEvents_Text2GCE.hpp: $(SRC)/Input/InputQueue.hpp \
	$(topdir)/tools/Text2GCE.pl | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) $(topdir)/tools/Text2GCE.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(OUT)/include/InputEvents_Text2NE.hpp: $(SRC)/Input/InputQueue.hpp \
	$(topdir)/tools/Text2NE.pl | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) $(topdir)/tools/Text2NE.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(OUT)/include/InputEvents_Char2GCE.hpp: $(SRC)/Input/InputQueue.hpp \
	$(topdir)/tools/Char2GCE.pl | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) $(topdir)/tools/Char2GCE.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(OUT)/include/InputEvents_Char2NE.hpp: $(SRC)/Input/InputQueue.hpp \
	$(topdir)/tools/Char2NE.pl | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) $(topdir)/tools/Char2NE.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

XCI_LIST = default
XCI_HEADERS = $(patsubst %,$(OUT)/include/InputEvents_%.hpp,$(XCI_LIST))

ifeq ($(TARGET_IS_OPENVARIO),y)
  DEFAULT_XCI = Data/Input/defaultOV.xci
else
  DEFAULT_XCI = Data/Input/default.xci
endif

# $(OUT)/include is shared between the targets, but the chosen xci
# file is not: remember it in a stamp, so that switching between
# OPENVARIO and the other targets regenerates the header
XCI_SELECTED = $(OUT)/include/InputEvents_default.selected
.PHONY: update-xci-selected
$(XCI_SELECTED): update-xci-selected | $(OUT)/include/dirstamp
	@echo $(DEFAULT_XCI) | cmp -s - $@ || echo $(DEFAULT_XCI) > $@

$(OUT)/include/InputEvents_default.hpp: $(topdir)/$(DEFAULT_XCI) \
	$(XCI_SELECTED) \
	$(topdir)/tools/xci2cpp.pl \
	| $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) $(topdir)/tools/xci2cpp.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(call SRC_TO_OBJ,$(SRC)/Input/InputDefaults.cpp): $(XCI_HEADERS)
$(call SRC_TO_OBJ,$(SRC)/Input/InputLookup.cpp): $(OUT)/include/InputEvents_Text2Event.hpp $(OUT)/include/InputEvents_Text2GCE.hpp $(OUT)/include/InputEvents_Text2NE.hpp

$(call SRC_TO_OBJ,$(SRC)/lua/InputEvent.cpp): $(OUT)/include/InputEvents_Char2GCE.hpp $(OUT)/include/InputEvents_Char2NE.hpp

$(OUT)/include/Status_defaults.hpp: Data/Status/default.xcs \
	tools/xcs2cpp.pl | $(OUT)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) tools/xcs2cpp.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

SM_OBJ = $(call SRC_TO_OBJ,$(SRC)/StatusMessage.cpp)
$(SM_OBJ): $(OUT)/include/Status_defaults.hpp

generate:: $(OUT)/include/MathTables.h $(XCI_HEADERS) \
	$(OUT)/include/Status_defaults.hpp \
	$(OUT)/include/InputEvents_Text2Event.hpp $(OUT)/include/InputEvents_Text2GCE.hpp $(OUT)/include/InputEvents_Text2NE.hpp \
	$(OUT)/include/InputEvents_Char2GCE.hpp $(OUT)/include/InputEvents_Char2NE.hpp \
	$(OUT)/include/QuickGuideNEWS.hpp

# UNIX resources

ifeq ($(USE_WIN32_RESOURCES),n)

ifeq ($(TARGET_IS_ANDROID),n)

$(TARGET_OUTPUT_DIR)/include/resource_data.h: $(TARGET_OUTPUT_DIR)/resources.txt \
	$(RESOURCE_FILES) \
	tools/GenerateResources.pl | $(TARGET_OUTPUT_DIR)/include/dirstamp
	@$(NQ)echo "  GEN     $@"
	$(Q)$(PERL) tools/GenerateResources.pl $< >$@.$(RANDOM_NUMBER).tmp
	@mv $@.$(RANDOM_NUMBER).tmp $@

$(call SRC_TO_OBJ,$(SRC)/ResourceLoader.cpp): $(TARGET_OUTPUT_DIR)/include/resource_data.h

generate:: $(TARGET_OUTPUT_DIR)/include/resource_data.h

endif # !TARGET_IS_ANDROID

endif
