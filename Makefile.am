# My First Libtool Project

lib_LTLIBRARIES = libieee1284.la
libieee1284_la_SOURCES = detect.c detect.h ports.c deviceid.c state.c \
	access.h delay.h delay.c default.h default.c access_io.c \
	access_ppdev.c interface.c parport.h ppdev.h debug.h debug.c \
	libieee1284.sym
# When rolling a release, remember to adjust the version info.
# It's current:release:age.
libieee1284_la_LDFLAGS = -version-info 1:0:0 \
	-export-symbols ${srcdir}/libieee1284.sym
include_HEADERS = ieee1284.h

noinst_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = libieee1284.la
EXTRA_DIST = libieee1284.spec doc
