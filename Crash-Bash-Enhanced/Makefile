.SUFFIXES:
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment (normally $$PS3DEV).")
endif
TITLE := Crash Bash Enhanced
APPID := CBASHE001
CONTENTID := UP0001-$(APPID)_00-0000000000000000
ICON0 := $(CURDIR)/pkgfiles/ICON0.PNG
PKGFILES := $(CURDIR)/pkgfiles
SFOXML := $(CURDIR)/sfo.xml
include $(PSL1GHT)/ppu_rules
TARGET := crashbash
BUILD := build
SOURCES := source
INCLUDES := include source
LIBDIRS := $(PORTLIBS)
LIBS := -lya2d -lfont3d -ltiny3d -lsimdmath -lgcm_sys -lrsx -lio -lsysutil -lrt -llv2 -lsysmodule -lsysfs -lnet -lm
CFLAGS = -O2 -Wall -Wextra -std=gnu99 -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS = $(CFLAGS)
LDFLAGS = $(MACHDEP) -Wl,-Map,$(notdir $@).map
.PHONY: all pkg clean info host-test
ifneq ($(BUILD),$(notdir $(CURDIR)))
export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export BUILDDIR := $(CURDIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) $(LIBPSL1GHT_INC) -I$(PORTLIBS)/include -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) $(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib
.PHONY: $(BUILD)
all: $(BUILD)
$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
pkg: $(BUILD)
	@$(MAKE) --no-print-directory $(OUTPUT).pkg
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
host-test:
	./scripts/test_host.sh
info:
	@echo "Target: $(TARGET)"
	@echo "Title ID: $(APPID)"
	@echo "Network: independent of PSN"
clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg $(OUTPUT).gnpdrm.pkg $(OUTPUT).fake.self
