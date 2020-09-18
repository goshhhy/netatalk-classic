
# Makefile.am for libatalk/

SUBDIRS = acl adouble asp atp bstring compat cnid nbp netddp tdb util unicode vfs

lib_LTLIBRARIES = libatalk.la

libatalk_la_SOURCES = dummy.c

libatalk_la_LIBADD  = \
	acl/libacl.la \
	adouble/libadouble.la	\
	asp/libasp.la		\
	atp/libatp.la		\
	bstring/libbstring.la \
	compat/libcompat.la	\
	nbp/libnbp.la		\
	netddp/libnetddp.la \
	tdb/libtdb.la       \
	unicode/libunicode.la \
	util/libutil.la		\
	vfs/libvfs.la

libatalk_la_DEPENDENCIES = \
	acl/libacl.la \
	adouble/libadouble.la	\
	asp/libasp.la		\
	atp/libatp.la		\
	bstring/libbstring.la \
	compat/libcompat.la	\
	nbp/libnbp.la		\
	netddp/libnetddp.la \
	tdb/libtdb.la       \
	unicode/libunicode.la \
	util/libutil.la		\
	vfs/libvfs.la

libatalk_la_LDFLAGS = -static
