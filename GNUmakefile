#
# Makefile for Clockperf
#

MAKEFLAGS += -Rr

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')

prefix := /usr/local
bindir := $(prefix)/bin

ifneq ($(findstring MINGW,$(uname_S)),)
win32 = Yep
endif

ifdef win32
EXT := .exe
else
EXT :=
endif

ifneq ($(findstring $(MAKEFLAGS),s),s)
ifndef V
        QUIET_DEPEND    = @echo '   ' DEPEND $@;
        QUIET_CC        = @echo '   ' CC $@;
        QUIET_GEN       = @echo '   ' GEN $@;
        QUIET_LINK      = @echo '   ' LD $@;
        QUIET           = @
        export V
endif
endif

PROJECT := clockperf
BINARY := $(PROJECT)$(EXT)

all: $(BINARY)

ifdef DEBUG
OPTLEVEL := -O0 -ggdb3 -D_DEBUG
else
OPTLEVEL := -O3
endif

CC := gcc
CP := cp -L

COMPILER_ACCEPTS_OPENMP := $(shell $(CC) -c -fopenmp -xc /dev/null -o /dev/null &>/dev/null && echo yes || echo no)

ifeq ($(COMPILER_ACCEPTS_OPENMP),yes)
    OPENMP_ARG := -fopenmp
endif

CFLAGS := \
	$(OPTLEVEL) \
	$(OPENMP_ARG) \
	-fno-strict-aliasing \
	-std=gnu11 \
	-Werror=implicit \
	-Werror=undef \
	-Wall \
	-Wextra \
	-Wdeclaration-after-statement \
	-Wimplicit-function-declaration \
	-Wmissing-declarations \
	-Wmissing-prototypes \
	-Wno-long-long \
	-Wno-overlength-strings \
	-Wold-style-definition \
	-Wstrict-prototypes \
	-Wno-deprecated-declarations

LDFLAGS := -lm
OBJECTS := affinity.o clock.o drift.o main.o tscemu.o util.o winapi.o version.o

ifdef NO_GNU_GETOPT
CFLAGS += -Igetopt
OBJECTS += getopt/getopt_long.o
endif

ifneq ($(CC),clang)
CFLAGS += -fPIC
LDFLAGS += -fPIC
endif

ifeq ($(uname_S),Linux)
CFLAGS += -pthread
LDFLAGS += -pthread -lrt
endif

ifeq ($(uname_S),FreeBSD)
CFLAGS += -pthread
LDFLAGS += -pthread
endif

ifneq ($(findstring MINGW,$(uname_S)),)
LDFLAGS += -lpthread -lwinmm
endif

ifneq ($(findstring CYGWIN,$(uname_S)),)
LDFLAGS += -lwinmm
endif

ifneq ($(findstring -flto,$(CFLAGS)),)
LDFLAGS += $(CFLAGS)
endif

ifeq (,$(findstring clean,$(MAKECMDGOALS)))
ifeq (,$(findstring distclean,$(MAKECMDGOALS)))
DEPS := $(shell ls $(OBJECTS:.o=.d) 2>/dev/null)

ifneq ($(DEPS),)
-include $(DEPS)
endif
endif
endif

.PHONY: all depend clean distclean install

install: $(BINARY)
	install -D -m0755 $(BINARY) $(DESTDIR)$(bindir)/$(BINARY)

depend: $(DEPS)

$(BINARY): $(OBJECTS)
	+$(QUIET_LINK)$(CC) -o $@ $(OBJECTS) $(LDFLAGS) $(CFLAGS)

distclean: clean

clean:
	$(QUIET)rm -f .cflags
	$(QUIET)rm -f $(BINARY)
	$(QUIET)rm -f $(OBJECTS) build.h license.h
	$(QUIET)rm -f $(OBJECTS:.o=.d)

ifdef NO_INLINE_DEPGEN
$(OBJECTS): $(OBJECTS:.o=.d)
endif

%.d: %.c .cflags
	$(QUIET_DEPEND)$(CC) -MM $(CFLAGS) -MT $*.o $< > $*.d

%.o: %.c .cflags
ifdef NO_INLINE_DEPGEN
	$(QUIET_CC)$(CC) $(CFLAGS) -c -o $@ $<
else
	$(QUIET_CC)$(CC) $(CFLAGS) -MD -c -o $@ $<
endif

build.h: .force-regen
	$(QUIET_GEN)tools/build.pl build.h

.PHONY: .force-regen

license.h: COPYING
	$(QUIET_GEN)tools/license.pl COPYING license.h

version.d: license.h build.h

version.o: license.h build.h

ifeq (,$(findstring clean,$(MAKECMDGOALS)))

TRACK_CFLAGS = $(subst ','\'',$(CC) $(CFLAGS) $(uname_S) $(uname_O) $(prefix))

.cflags: .force-cflags
	@FLAGS='$(TRACK_CFLAGS)'; \
	if test x"$$FLAGS" != x"`cat .cflags 2>/dev/null`" ; then \
		echo "    * rebuilding $(PROJECT): new build flags or prefix"; \
		echo "$$FLAGS" > .cflags; \
	fi

.PHONY: .force-cflags

endif

