.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment (normally $$PS3DEV).")
endif

TITLE := PS3 SCALE
APPID := PS3SCALE1
CONTENTID := UP0001-$(APPID)_00-0000000000000000
ICON0 := $(CURDIR)/pkgfiles/ICON0.PNG
PKGFILES := $(CURDIR)/pkgfiles
SFOXML := $(CURDIR)/sfo.xml

include $(PSL1GHT)/ppu_rules

TARGET := ps3scale
BUILD := build
SOURCES := source/core source/ps3
DATA := data
INCLUDES := include source/ps3

LIBDIRS := $(PORTLIBS)

LIBS := -lya2d -lfont3d -ltiny3d -lsimdmath -lgcm_sys -lrsx -lio -lsysutil \
        -lpngdec -ljpgdec -lpng -lz -lfreetype -lpixman-1 \
        -lmpg123 -laudio -lrt -llv2 -lsysmodule -lsysfs \
        -lcurl -lpolarssl -lnetctl -lnet -lz -lm

CFLAGS = -O2 -Wall -Wextra -std=gnu99 -mcpu=cell $(MACHDEP) $(INCLUDE) \
         -I$(PORTLIBS)/include/soundlib
CXXFLAGS = $(CFLAGS)
LDFLAGS = $(MACHDEP) -Wl,-Map,$(notdir $@).map

.PHONY: all pkg clean host-test info

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                $(foreach dir,$(DATA),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)
export BUILDDIR := $(CURDIR)/$(BUILD)

CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.bin)))
PNGFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.png)))
JPGFILES := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.jpg)))

ifeq ($(strip $(CPPFILES)),)
export LD := $(CC)
else
export LD := $(CXX)
endif

export OFILES := $(addsuffix .o,$(BINFILES)) \
                 $(addsuffix .o,$(PNGFILES)) \
                 $(addsuffix .o,$(JPGFILES)) \
                 $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) \
                 $(sFILES:.s=.o) $(SFILES:.S=.o)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  $(LIBPSL1GHT_INC) -I$(PORTLIBS)/include \
                  -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
                   $(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib

.PHONY: $(BUILD)
all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

# Build the ELF/SELF first in the recursive build directory, then package it.
# The recursive build's default goal is $(OUTPUT).self (defined below).
# This recipe ensures packaging cannot race object compilation/linking under -j.
pkg: $(BUILD)
	@python3 scripts/prepare_theme.py
	@cp data/online_profiles.tsv pkgfiles/USRDIR/graphics_catalog.tsv
	@test -s pkgfiles/USRDIR/cacert.pem
	@$(MAKE) --no-print-directory $(OUTPUT).pkg

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

%.bin.o: %.bin
	@$(bin2o)

%.png.o: %.png
	@$(bin2o)

%.jpg.o: %.jpg
	@$(bin2o)

-include $(DEPENDS)

endif

# Keep utility targets after the conditional so the recursive build's first
# explicit target remains $(OUTPUT).self rather than host-test or info.
host-test:
	./scripts/test_host.sh

info:
	@echo "Target: $(TARGET)"
	@echo "Package ID: $(APPID)"
	@echo "Hardware: auto-detect Platform ID / CEX-DEX-Tool type at runtime; Slim priority, FAT and Super Slim supported"

clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg \
	        $(OUTPUT).gnpdrm.pkg $(OUTPUT).fake.self
