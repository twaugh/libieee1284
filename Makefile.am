# My First Libtool Project

lib_LTLIBRARIES = libieee1284.la
libieee1284_la_SOURCES = src/detect.c src/detect.h src/ports.c src/deviceid.c \
	src/state.c src/access.h src/delay.h src/delay.c src/default.h \
	src/default.c src/access_io.c src/access_ppdev.c src/access_lpt.c \
	src/interface.c src/parport.h src/ppdev.h src/debug.h src/debug.c \
	src/par_nt.h src/io.h src/conf.h src/conf.c libieee1284.sym
# When rolling a release, remember to adjust the version info.
# It's current:release:age.
libieee1284_la_LDFLAGS = -version-info 5:2:2 \
	-export-symbols $(top_srcdir)/libieee1284.sym
include_HEADERS = include/ieee1284.h
INCLUDES = -I$(top_srcdir)/include

iop_SOURCES=solaris_io/iop.c
ioprtest_SOURCES=solaris_io/ioprtest.c

if SOLARIS_IO
iop_CFLAGS=-D_KERNEL # -DDEBUG
drvexecdir=/platform/i86pc/kernel/drv
drvdatadir=/platform/i86pc/kernel/drv
nobase_drvdata_DATA=solaris_io/iop.conf
nobase_drvexec_PROGRAMS=iop
noinst_PROGRAMS=ioprtest
iop_LINK=$(LD) -r -o $@
endif

bin_PROGRAMS = libieee1284_test
libieee1284_test_SOURCES = tests/test.c
libieee1284_test_LDADD = libieee1284.la

# Python bindings
src_ieee1284module_la_SOURCES = src/ieee1284module.c
if PYTHON
pyexec_LTLIBRARIES = src/ieee1284module.la
src_ieee1284module_la_LDFLAGS = -module -avoid-version
src_ieee1284module_la_LIBADD = libieee1284.la
src_ieee1284module_la_CFLAGS = -I/usr/include/python$(PYTHON_VERSION) -fpic
endif

man3_MANS = doc/libieee1284.3 \
	doc/parport.3 \
	doc/parport_list.3 \
	doc/ieee1284_find_ports.3 \
	doc/ieee1284_free_ports.3 \
	doc/ieee1284_get_deviceid.3 \
	doc/ieee1284_open.3 \
	doc/ieee1284_close.3 \
	doc/ieee1284_ref.3 \
	doc/ieee1284_unref.3 \
	doc/ieee1284_claim.3 \
	doc/ieee1284_release.3 \
	doc/ieee1284_read_data.3 \
	doc/ieee1284_write_data.3 \
	doc/ieee1284_data_dir.3 \
	doc/ieee1284_wait_data.3 \
	doc/ieee1284_read_status.3 \
	doc/ieee1284_wait_status.3 \
	doc/ieee1284_read_control.3 \
	doc/ieee1284_write_control.3 \
	doc/ieee1284_frob_control.3 \
	doc/ieee1284_do_nack_handshake.3 \
	doc/ieee1284_negotiate.3 doc/ieee1284_terminate.3 \
	doc/ieee1284_ecp_fwd_to_rev.3 doc/ieee1284_ecp_rev_to_fwd.3 \
	doc/ieee1284_nibble_read.3 \
	doc/ieee1284_compat_write.3 \
	doc/ieee1284_byte_read.3 \
	doc/ieee1284_epp_read_data.3 doc/ieee1284_epp_write_data.3 \
	doc/ieee1284_epp_read_addr.3 doc/ieee1284_epp_write_addr.3 \
	doc/ieee1284_ecp_read_data.3 doc/ieee1284_ecp_write_data.3 \
	doc/ieee1284_ecp_read_addr.3 doc/ieee1284_ecp_write_addr.3 \
	doc/ieee1284_get_irq_fd.3 \
	doc/ieee1284_set_timeout.3

$(man3_MANS): $(top_srcdir)/doc/interface.xml
	xmlto man -o doc $<

doc/interface.pdf: $(top_srcdir)/doc/interface.xml
	xmlto pdf -o doc --extensions $<

EXTRA_DIST = libieee1284.spec doc/interface.xml doc/interface.txt \
		$(man3_MANS) solaris_io/LICENSE solaris_io/README \
		solaris_io/INSTALL solaris_io/iop.conf \
		README.FreeBSD README.OpenBSD README.Solaris \
		README.win95 README.winNT \
		include/config.h.vc6 README.VC6 Makefile.vc6 ieee1284.def

DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]* \
	doc/manpage.links doc/manpage.refs

MAINTAINERCLEANFILES = $(man3_MANS)

tag:
	cvs tag -c `echo V@VERSION@ | tr . _`
