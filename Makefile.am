# My First Libtool Project

lib_LTLIBRARIES = libieee1284.la
libieee1284_la_SOURCES = src/detect.c src/detect.h src/ports.c src/deviceid.c \
	src/state.c src/access.h src/delay.h src/delay.c src/default.h \
	src/default.c src/access_io.c src/access_ppdev.c src/interface.c \
	src/parport.h src/ppdev.h src/debug.h src/debug.c \
	libieee1284.sym
# When rolling a release, remember to adjust the version info.
# It's current:release:age.
libieee1284_la_LDFLAGS = -version-info 3:0:0 \
	-export-symbols $(top_srcdir)/libieee1284.sym
include_HEADERS = include/ieee1284.h
INCLUDES = -I$(top_srcdir)/include

man3_MANS = doc/ieee1284_claim.3 doc/ieee1284_close.3 doc/ieee1284_control.3 \
	doc/ieee1284_data.3 doc/ieee1284_ecp_fwd_to_rev.3 \
	doc/ieee1284_find_ports.3 doc/ieee1284_free_ports.3 \
	doc/ieee1284_get_deviceid.3 doc/ieee1284_get_irq_fd.3 \
	doc/ieee1284_negotiation.3 doc/ieee1284_open.3 doc/ieee1284_release.3 \
	doc/ieee1284_set_timeout.3 doc/ieee1284_status.3 \
	doc/ieee1284_transfer.3 doc/libieee1284.3 doc/parport.3 \
	doc/parport_list.3

$(man3_MANS): $(top_srcdir)/doc/interface.sgml
	docbook2man -o doc $<
	docbook2man -o doc $<

noinst_PROGRAMS = test
test_SOURCES = tests/test.c
test_LDADD = libieee1284.la

# Automake bug?
# CLEANFILES = tests/.libs/test

EXTRA_DIST = libieee1284.spec doc/interface.sgml doc/interface.txt

DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]* $(man3_MANS) \
	doc/manpage.links doc/manpage.refs
