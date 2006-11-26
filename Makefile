# Makefile for toybox.
# Copyright 2006 Rob Landley <rob@landley.net>

CFLAGS  = -Wall -Wundef -Os
CC      = $(CROSS_COMPILE)gcc $(CFLAGS) -funsigned-char
STRIP   = $(CROSS_COMPILE)strip
HOST_CC = gcc $(CFLAGS) -funsigned-char

all: toybox

.PHONY: clean

include kconfig/Makefile

.config: Config.in toys/Config.in

# The long and roundabout sed is to make old versions of sed happy.  New ones
# have '\n' so can replace one line with two without all the branches and
# mucking about with hold space.
gen_config.h: .config
	sed -n -e 's/^# CONFIG_\(.*\) is not set.*/\1/' \
	  -e 't notset' -e 'b tryisset' -e ':notset' \
	  -e 'h' -e 's/.*/#define CFG_& 0/p' \
	  -e 'g' -e 's/.*/#define USE_&(...)/p' -e 'd' -e ':tryisset' \
	  -e 's/^CONFIG_\(.*\)=y.*/\1/' -e 't isset' -e 'd' -e ':isset' \
	  -e 'h' -e 's/.*/#define CFG_& 1/p' \
	  -e 'g' -e 's/.*/#define USE_&(...) __VA_ARGS__/p' $< > $@

# Development targets
baseline: toybox_unstripped
	@cp toybox_unstripped toybox_old

bloatcheck: toybox_old toybox_unstripped
	@scripts/bloat-o-meter toybox_old toybox_unstripped

# Actual build

toyfiles = main.c toys/*.c lib/*.c
toybox_unstripped: gen_config.h $(toyfiles) toys/toylist.h lib/lib.h toys.h
	$(CC) $(CFLAGS) -I . $(toyfiles) -o toybox_unstripped \
		-ffunction-sections -fdata-sections -Wl,--gc-sections

toybox: toybox_unstripped
	$(STRIP) toybox_unstripped -o toybox
clean::
	rm -f toybox gen_config.h

distclean: clean
	rm -f .config

help::
	@echo  '  baseline        - Create busybox_old for use by bloatcheck.'
	@echo  '  bloatcheck      - Report size differences between old and current versions'
