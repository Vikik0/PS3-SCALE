.SUFFIXES:
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment (normally $$PS3DEV).")
endif
TITLE := Crash Bash Online
APPID := CBON00001
CONTENTID := UP0001-$(APPID)_00-0000000000000001
ICON0 := $(CURDIR)/pkgfiles/ICON0.PNG
PKGFILES := $(CURDIR)/pkgfiles
CA_BUNDLE := $(PKGFILES)/USRDIR/cacert.pem
SFOXML := $(CURDIR)/sfo.xml
include $(PSL1GHT)/ppu_rules
TARGET := crashbash
BUILD := build
SOURCES := source
INCLUDES := include source
LIBDIRS := $(PORTLIBS)
LIBS := -lcurl -lya2d -lfont3d -ltiny3d -lsimdmath -lgcm_sys -lrsx -lio -lsysutil -lrt -llv2 -lsysmodule -lsysfs -lnet -lpolarssl -lz -lm
CFLAGS = -O2 -Wall -Wextra -std=gnu99 -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS = $(CFLAGS)
LDFLAGS = $(MACHDEP) -Wl,-Map,$(notdir $@).map
.PHONY: all pkg clean info host-test ca-bundle
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
pkg: ca-bundle $(BUILD)
	@$(MAKE) --no-print-directory $(OUTPUT).pkg
	@command -v package_finalize >/dev/null
	@package_finalize "$(OUTPUT).pkg"
else
DEPENDS := $(OFILES:.o=.d)
$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
-include $(DEPENDS)
endif
ca-bundle:
	@mkdir -p "$(dir $(CA_BUNDLE))"
	@if [ ! -s "$(CA_BUNDLE)" ]; then python3 -c 'import urllib.request; urllib.request.urlretrieve("https://curl.se/ca/cacert.pem", "$(CA_BUNDLE)")'; fi
	@test -s "$(CA_BUNDLE)"
host-test:
	./scripts/test_host.sh
info:
	@echo "Target: $(TARGET)"
	@echo "Title ID: $(APPID)"
	@echo "Network: HTTPS lobby, independent of PSN"
clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg $(OUTPUT).gnpdrm.pkg $(OUTPUT).fake.self
	@rm -f "$(CA_BUNDLE)"
