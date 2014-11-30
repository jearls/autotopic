########################################################################
# THIS IS A VERY ROUGH MAKEFILE
# It was handcoded based on an incomplete knowledge of what's needed.
# Eventually this will be replaced by an automake/autoconf system.
# For now, caveat emptor.
########################################################################

# WARNING - This Makefile requires GNU Make for $(shell), ifeq(), etc.

# Define PIDGIN_DEV_ROOT if compiling for Windows using Cygwin.
# This directory should be the PIDGIN_DEV_ROOT as defined in
# https://developer.pidgin.im/wiki/BuildingWinPidgin
PIDGIN_DEV_ROOT = ../../devel

#
# Define this if compiling for Windows using Cygwin.  This is the version
# number of the pidgin source tree located inside PIDGIN_DEV_ROOT.  The
# actual source tree would be $(PIDGIN_DEV_ROOT)/pidgin-$(PIDGIN_VERSION)
PIDGIN_VERSION = 2.10.7

########################################################################
# NO CHANGES should be required after this point
########################################################################

#
# Find or define our list of plugins
#
# PLUGINS := $(patsubst %.c,%,$(wildcard *.c))
PLUGINS = autotopic

# find my OS type and make my definitions based on that
OSTYPE := $(shell uname -s | cut -d- -f1)
# redefine certain OS types
ifeq ($(OSTYPE), CYGWIN_NT)
OSTYPE = Win32
endif

ifeq ($(OSTYPE), Win32)
#
# Compiling for windows under the cygwin environment
# We are compiling a .dll file
#
EXTENSION = dll

# PIDGIN_SRC points to the pidgin tree.
PIDGIN_SRC := $(PIDGIN_DEV_ROOT)/pidgin-$(PIDGIN_VERSION)

#
# Manually define GLIB_INCLUDES and OTHER_CFLAGS for
# compiling under cygwin.
# Eventually this should be better automated.
#

GTKDIR := $(wildcard $(PIDGIN_DEV_ROOT)/win32-dev/gtk_*)

GLIB_INCLUDES = -I$(GTKDIR)/include -I$(GTKDIR)/include/glib-2.0 -I$(GTKDIR)/lib/glib-2.0/include 
OTHER_CFLAGS = -I$(PIDGIN_SRC) --param ssp-buffer-size=1 -mms-bitfields -DHAVE_CYRUS_SASL -DHAVE_CONFIG_H -DWIN32_LEAN_AND_MEAN 

#
# Manually define GLIB_LIBS and OTHER_LIBS for
# compiling under cygwin.
# Eventually this should be better automated.
#

GLIB_LIBS = -L$(GTKDIR)/lib -lglib-2.0 -lgobject-2.0 -lgmodule-2.0
OTHER_LIBS = -L$(PIDGIN_SRC)/win32-install-dir -lintl -lws2_32 -lpurple -Wl,--enable-auto-image-base -Wl,--enable-auto-import -Wl,--dynamicbase -Wl,--nxcompat -lssp

else

#
# Compiling for linux or solaris.
# We are compiling a .so file
#
EXTENSION = so

#
# Define GLIB_INCLUDES via pkg-config
# OTHER_CFLAGS gets -fPIC for compiling a shared object file
#

GLIB_INCLUDES = $(shell pkg-config --cflags pidgin)
OTHER_CFLAGS = -fPIC

#
# Define GLIB_LIBS via pkg-config
# OTHER_LIBS just gets -lpurple for a libpurple plugin.
# Eventually this should get the other pidgin directories
# for compiling pidgin plugins.

GLIB_LIBS = $(shell pkg-config --libs pidgin)
OTHER_LIBS = -lpurple

endif

#
# Default "all" target rebuilds all plugins
#

.PHONY:	all $(PLUGINS)
all:	$(PLUGINS)

$(PLUGINS):
	$(MAKE) $@.$(EXTENSION)

#
# Build the plugin OSTYPE-specific object file
#

%.$(EXTENSION).o:	%.c
	gcc -O2 -Wall -Waggregate-return -Wcast-align -Wdeclaration-after-statement -Werror-implicit-function-declaration -Wextra -Wno-sign-compare -Wno-unused-parameter -Winit-self -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wundef -Wstack-protector -fwrapv -Wno-missing-field-initializers -Wformat-security -fstack-protector-all $(GLIB_INCLUDES) $(OTHER_CFLAGS) -pipe -g -o $@ -c $<

#
# Build the plugin library from its OSTYPE-specific object file
#

%.$(EXTENSION):	%.$(EXTENSION).o
	gcc -shared $< $(GLIB_LIBS) $(OTHER_LIBS) -o $@
