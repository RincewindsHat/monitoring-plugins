## Process this file with automake to produce Makefile.in

SUBDIRS = . tests

noinst_LIBRARIES = libmonitoringplug.a

AM_CPPFLAGS = -DNP_STATE_DIR_PREFIX=\"$(localstatedir)\" \
	-I$(srcdir) -I$(top_srcdir)/gl -I$(top_srcdir)/intl -I$(top_srcdir)/plugins

libmonitoringplug_a_SOURCES = utils_base.c utils_tcp.c utils_cmd.c maxfd.c output.c perfdata.c output.c thresholds.c vendor/cJSON/cJSON.c

EXTRA_DIST = utils_base.h \
	utils_tcp.h \
	utils_cmd.h \
	parse_ini.h \
	extra_opts.h \
	maxfd.h \
	perfdata.h \
	output.h \
	thresholds.h \
	states.h \
	vendor/cJSON/cJSON.h \
	monitoringplug.h

if USE_PARSE_INI
libmonitoringplug_a_SOURCES += parse_ini.c extra_opts.c
endif USE_PARSE_INI

test test-debug:
	cd tests && make $@
