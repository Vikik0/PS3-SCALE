.SUFFIXES:

ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment (normally $$PS3DEV).")
endif

TITLE := DSF MOD MENU
APPID := DSFMOD01
CONTENTID := UP0001-$(APPID)_00-0000000000000000
ICON0 := $(CURDIR)/pkgfiles/ICON0.PNG
PKGFILES := $(CURDIR)/pkgfiles
SFOXML := $(CURDIR)/sfo.xml

include $(PSL1GHT)/ppu_rules

TARGET := dsfmod
BUILD := build
SOURCES := source
INCLUDES := include source

LIBDIRS := $(PORTLIBS)
LIBS := -lya2d -lfont3d -ltiny3d -lgcm_sys -lrsx -lio -lsysutil -lsysfs -llv2 -lm

CFLAGS = -O2 -Wall -Wextra -std=gnu99 -mcpu=cell $(MACHDEP) $(INCLUDE)
LDFLAGS = $(MACHDEP) -Wl,-Map,$(notdir $@).map

.PHONY: all pkg clean

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export VPATH := $(CURDIR)/source
export DEPSDIR := $(CURDIR)/$(BUILD)
export BUILDDIR := $(CURDIR)/$(BUILD)

CFILES := $(notdir $(wildcard source/*.c))
export OFILES := $(CFILES:.c=.o)
export LD := $(CC)

export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  $(LIBPSL1GHT_INC) -I$(PORTLIBS)/include -I$(CURDIR)/$(BUILD)

export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
                   $(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib

.PHONY: $(BUILD)
all: $(BUILD)

$(BUILD):
	@mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

pkg: $(BUILD)
	@python3 scripts/generate_icon.py
	@test -s pkgfiles/USRDIR/simple-cheat-menu.sprx
	@$(MAKE) --no-print-directory $(OUTPUT).pkg

else

DEPENDS := $(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf

$(OUTPUT).elf: $(OFILES)

-include $(DEPENDS)

endif

clean:
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).pkg \
	        $(OUTPUT).gnpdrm.pkg $(OUTPUT).fake.self
