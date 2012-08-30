# if VARNISHSRC is defined on the command-line, use that. Otherwise, build
# this the same as the modules that come with varnish (i.e. we're building
# within the varnish src dir itself, and $(top_srcdir) is the varnish source).
#
ifdef $(VARNISHSRC):
	SRC = $(VARNISHSRC)
	DIR_PREFIX = /
else:
	SRC = $(top_srcdir)
	DIR_PREFIX = lib/libvmod-timers/

INCLUDES = -I$(SRC)/include -I$(SRC)

ifdef $(VMODDIR):
	vmoddir = $(VMODDIR)
else:
	vmoddir = $(pkglibdir)/vmods

vmod_LTLIBRARIES = libvmod_statsd.la

libvmod_statsd_la_LDFLAGS = -module -export-dynamic -avoid-version

libvmod_statsd_la_SOURCES = \
	vcc_if.c \
	vcc_if.h \
	vmod_statsd.c

vcc_if.c vcc_if.h: $(SRC)/lib/libvmod_std/vmod.py $(top_srcdir)/$(DIR_PREFIX)src/vmod_statsd.vcc
	@PYTHON@ $(SRC)/lib/libvmod_std/vmod.py $(top_srcdir)/$(DIR_PREFIX)src/vmod_statsd.vcc

VMOD_TESTS = tests/*.vtc
.PHONY: $(VMOD_TESTS)

tests/*.vtc:
	$(SRC)/bin/varnishtest/varnishtest -Dvarnishd=$(top_srcdir)/bin/varnishd/varnishd -Dvmod_topbuild=$(abs_top_builddir) $@

check: $(VMOD_TESTS)

EXTRA_DIST = \
	vmod_statsd.vcc \
	$(VMOD_TESTS)

CLEANFILES = $(builddir)/vcc_if.c $(builddir)/vcc_if.h
