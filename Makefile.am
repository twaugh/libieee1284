# My First Libtool Project

lib_LTLIBRARIES = libieee1284.la
libieee1284_la_SOURCES = detect.c detect.h ports.c deviceid.c state.c \
	access.h delay.h delay.c default.h default.c access_io.c \
	access_ppdev.c interface.c parport.h ppdev.h
libieee1284_la_LDFLAGS = -export-symbols libieee1284.sym
include_HEADERS = ieee1284.h

noinst_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = libieee1284.la
