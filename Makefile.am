# My First Libtool Project

lib_LTLIBRARIES = libieee1284.la
libieee1284_la_SOURCES = detect.c detect.h ports.c deviceid.c state.c \
	registers.c blockio.c parport.h ppdev.h
include_HEADERS = ieee1284.h

noinst_PROGRAMS = test
test_SOURCES = test.c
test_LDADD = libieee1284.la
