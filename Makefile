#!/usr/bin/make

WEBOS_PDK=/opt/PalmPDK
WEBOS_SDK=/opt/PalmSDK

TOOL_PREFIX=$(WEBOS_PDK)/arm-gcc/bin/arm-none-linux-gnueabi-

CC=$(TOOL_PREFIX)gcc
CXX=$(TOOL_PREFIX)g++
AS=$(TOOL_PREFIX)as
LD=$(TOOL_PREFIX)ld
STRIP=$(TOOL_PREFIX)strip

CPPFLAGS := -I. \
	-I$(WEBOS_PDK)/include -I$(WEBOS_PDK)/include/SDL \
	-Ideps/popt-1.14
LDLIBS := -lz -L$(WEBOS_PDK)/device/lib \
	-lpopt -L$(shell pwd) \
	-lGLESv2 -lpdl -Wl,-rpath=/usr/local/lib \
	-lSDL -lSDL_ttf -lSDL_image \
	-Wl,--allow-shlib-undefined \
	$(WEBOS_PDK)/arm-gcc/arm-none-linux-gnueabi/libc/usr/lib/libstdc++.a
# Above we statically link in libstdc++.
# This is due to a combination of using cs2011.03
# and my desire to use c++ ADT's to make my life easier.

OPTFLAGS += -O3 -mcpu=cortex-a8 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -ffast-math -flto
#OPTFLAGS += -O0 -mcpu=cortex-a8 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -ffast-math -g -flto

-include config.mk

CXXFLAGS += $(OPTFLAGS)
CPPFLAGS += $(OPTFLAGS)
CFLAGS += $(OPTFLAGS)
LDFLAGS += -s #strip the binary
LDFLAGS += -flto

#Enabling this breaks SA-1 support, which is important
#for games such as Kirby Super Star and Super Mario RPG :(
#However, this breaks Donkey Kong Country, and possibly others.
#(fails its copy protection check)
CONF_BUILD_ASM_CPU=0
CONF_BUILD_ASM_SPC700=0

ARCH=armel
# Sane defaults
CONF_GUI?=0
ifeq ($(ARCH),armel)
	CONF_BUILD_ASM_CPU?=1
	CONF_BUILD_ASM_SPC700?=1
	CONF_BUILD_ASM_SA1?=0	# Still not there
	CONF_BUILD_MISC_ROUTINES?=misc_armel
else ifeq ($(ARCH),i386)
	CONF_BUILD_ASM_CPU?=0
	CONF_BUILD_ASM_SPC700?=0
	CONF_BUILD_ASM_SA1?=0
	CONF_BUILD_MISC_ROUTINES?=misc_i386
endif
# Hardware pixel doubling (in N8x0)
CONF_XSP?=0
# Hildon Desktop compositing (in Fremantle)
CONF_HD?=0
# Link to libzeemote
CONF_ZEEMOTE?=0

# SNES stuff
OBJS = apu.o c4.o c4emu.o cheats.o cheats2.o clip.o cpu.o cpuexec.o data.o
OBJS += dma.o dsp1.o font.o fxemu.o fxinst.o gfx.o globals.o loadzip.o memmap.o 
OBJS += ppu.o sa1.o sdd1.o sdd1emu.o snapshot.o soundux.o spc700.o srtc.o tile.o

ifeq ($(CONF_BUILD_ASM_CPU), 1)
	# ASM CPU Core from yoyofr's OpenSnes9X
	OBJS += os9x_asm_cpu.o os9x_65c816.o
	CPPFLAGS += -DCONF_BUILD_ASM_CPU=1
else
	OBJS += cpuops.o
endif

ifeq ($(CONF_BUILD_ASM_SPC700), 1)
	OBJS += spc700a.o
	CPPFLAGS += -DCONF_BUILD_ASM_SPC700=1
endif

ifeq ($(CONF_BUILD_ASM_SA1), 1)
	crash
else
	OBJS += sa1cpu.o
endif

OBJS += $(CONF_BUILD_MISC_ROUTINES).o

# from open-whatever sdk
OBJS += unzip.o ioapi.o
# my extensions to snes9x (speedhacks support)
OBJS += hacks.o
# the glue code that sticks it all together in a monstruous way
OBJS += platform/path.o platform/config.o
OBJS += platform/sdl.o platform/sdlv.o platform/sdla.o platform/sdli.o
#OBJS += platform/sdlvscalers.o

#WebOS port stuff
OBJS += platform/GLUtil.o
OBJS += platform/RomSelector.o
OBJS += platform/OptionMenu.o
OBJS += platform/Keyboard.o
OBJS += platform/Options.o
OBJS += platform/Controller.o
OBJS += platform/resize++.o

ifeq ($(CONF_XSP), 1)
	CPPFLAGS += -DCONF_XSP=1 $(shell pkg-config --cflags xsp)
	LDLIBS += $(shell pkg-config --libs xsp)
endif
ifeq ($(CONF_HD), 1)
	CPPFLAGS += -DCONF_HD=1
	LDLIBS += -lSDL_haa
	CONF_EXIT_BUTTON ?= 1
endif
ifeq ($(CONF_GUI), 1)
	CPPFLAGS += -DCONF_GUI=1 $(shell pkg-config --cflags libosso gconf-2.0)
	LDLIBS += $(shell pkg-config --libs libosso gconf-2.0)
	OBJS += platform/osso.o
endif
ifeq ($(CONF_EXIT_BUTTON), 1)
	CPPFLAGS += -DCONF_EXIT_BUTTON=1
	LDLIBS += -lSDL_image
	OBJS += platform/sdlvexit.o
endif
ifeq ($(CONF_ZEEMOTE), 1)
	CPPFLAGS += -DCONF_ZEEMOTE=1
	LDLIBS += -lzeemote -lzeemote-conf -lbluetooth
	OBJS += platform/zeemote.o
endif

# automatic dependencies
DEPS := $(OBJS:.o=.d)

all: drnoksnes

clean:
	rm -f drnoksnes *.o *.d platform/*.o platform/*.d
	rm -f build-stamp configure-stamp

remake: clean deps all

-include $(DEPS)

drnoksnes: $(OBJS) libpopt.a
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

libpopt.a:
	cd deps/popt-1.14 && \
	PATH=$(WEBOS_PDK)/arm-gcc/bin:$(PATH) ./configure --host=arm-none-linux-gnueabi &&\
	PATH=$(WEBOS_PDK)/arm-gcc/bin:$(PATH) $(MAKE) -j8;
	cp deps/popt-1.14/.libs/libpopt.a $@

#libpopt.so.0: libpopt.so
#	cp $^ $@

install: drnoksnes
	install drnoksnes $(DESTDIR)/usr/games

deps: $(DEPS)
%.d: %.cpp
	@$(CXX) $(CPPFLAGS) -MM $^ -MF $@ -MT $@ -MT $*.o
%.d: %.c
	@$(CC) $(CPPFLAGS) -MM $^ -MF $@ -MT $@ -MT $*.o
%.d: %.s
	@touch $@

# GUI
gui:
	$(MAKE) -C gui all

gui_clean:
	$(MAKE) -C gui clean
	
gui_install:
	$(MAKE) -C gui install DESTDIR="$(DESTDIR)"
	
ifeq ($(CONF_GUI), 1)
all: gui
clean: gui_clean
install: gui_install
endif

profclean: clean
	find . -name '*.gcno' -delete
	find . -name '*.gcda' -delete

distclean: profclean clean
	rm -f config.mk

.PHONY: all clean remake deps install gui gui_clean distclean

