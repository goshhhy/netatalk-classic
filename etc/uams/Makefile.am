# Makefile.am for etc/uams/

#
# conditionally build some modules
#

UAMS_GENERIC = uams_guest.la uams_passwd.la
uams_LINKS = uams_clrtxt.so

# these are simple, though some the last three depend on OpenSSL

if BUILD_PAM
UAMS_PAM = uams_pam.la
endif

if USE_RANDNUM
UAMS_RANDNUM = uams_randnum.la
endif

# links

if USE_PAM_SO
UAMS_CLRTXT_LINK = uams_pam.so
else
UAMS_CLRTXT_LINK = uams_passwd.so
endif

#
# source files
#

uams_guest_la_SOURCES      = uams_guest.c
uams_randnum_la_SOURCES    = uams_randnum.c
uams_passwd_la_SOURCES     = uams_passwd.c
uams_pam_la_SOURCES        = uams_pam.c

#
# flags
#

# these should be sorted out, applying both to AM_CFLAGS is senseless
AM_CFLAGS = @SSL_CFLAGS@

uams_pam_la_CFLAGS         = @PAM_CFLAGS@

uams_guest_la_LDFLAGS      = -module -avoid-version
uams_randnum_la_LDFLAGS    = -module -avoid-version @SSL_LIBS@
uams_passwd_la_LDFLAGS     = -module -avoid-version @CRYPT_LIBS@
uams_pam_la_LDFLAGS        = -module -avoid-version @PAM_LIBS@

#
# module compilation
#

uamsdir = @UAMS_PATH@
uams_LTLIBRARIES =		\
	$(UAMS_GENERIC)		\
	$(UAMS_PAM)		\
	$(UAMS_RANDNUM)

#
# link creation
#

install-data-hook:
	(if [ -d $(DESTDIR)$(uamsdir) ] ; then \
		cd $(DESTDIR)$(uamsdir)					&& \
		rm -f $(uams_LINKS)  					&& \
		$(LN_S) $(UAMS_CLRTXT_LINK) uams_clrtxt.so		&& \
		$(dhx_exec_hook)					&& \
		$(dhx2_exec_hook) ; 					\
	fi)



uninstall-hook:
	(if [ -d $(DESTDIR)$(uamsdir) ] ; then	\
		cd $(DESTDIR)$(uamsdir) 	&& \
		rm -f $(uams_LINKS)	;	   	\
	fi)
