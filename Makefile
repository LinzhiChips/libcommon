#
# Copyright (C) 2021 Linzhi Ltd.
#
# This work is licensed under the terms of the MIT License.
# A copy of the license can be found in the file COPYING.txt
#

MKTGT = $(MAKE) -f Makefile.target

.PHONY:         all host arm clean spotless

all:		host arm

host:
		$(MKTGT) OBJDIR=./ $(SUB_TARGET)

arm:
		$(MKTGT) OBJDIR=arm/ CROSS=arm-linux- $(SUB_TARGET)

clean:
		$(MKTGT) OBJDIR=./ clean
		$(MKTGT) OBJDIR=arm/ clean

spotless:
		$(MKTGT) OBJDIR=./ spotless
		$(MKTGT) OBJDIR=arm/ spotless

# ----- Install/uninstall -----------------------------------------------------

PREFIX ?= /usr/local
INSTALL ?= install

INSTALL_INCLUDES = alloc.h container.h thread.h format.h dtime.h mqtt.h

install:	install-host install-arm

install-common:
		for n in $(INSTALL_INCLUDES); do \
		    $(INSTALL) -D -m 0644 -t $(PREFIX)/include/linzhi $$n || \
		    exit; \
		done

install-host:	install-common
		$(INSTALL) -D -m 0644 -t $(PREFIX)/lib/ libcommon.a

install-arm:	install-common
		$(INSTALL) -D -m 0644 arm/libcommon.a \
		    $(PREFIX)/lib/libcommon-arm.a

uninstall:	uninstall-host uninstall-arm

uninstall-common:
		for n in $(INSTALL_INCLUDES); do \
		    rm -f $(PREFIX)/include/linzhi/$$n; done

uninstall-host:
		rm -f $(PREFIX)/include/linzhi/libcommon.a

uninstall-arm:
		rm -f $(PREFIX)/include/linzhi/libcommon-arm.a
