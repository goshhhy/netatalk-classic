#Current release: netatalk-classic-20200921

netatalk-classic is an implementation of the AppleTalk Protocol Suite for Linux
and NetBSD.

netatalk-classic was forked from the last release of Netatalk that supported
AppleTalk (2.2.6).

netatalk-classic is maintained by Christopher Kobayashi <software+github@disavowed.jp>

#News for 20200921:

* Support for obsolete platforms and non-AppleTalk functions were removed to keep the codebase manageable (and reduce the potential attack surface).  If you need to serve AppleTalk on TRU64, Solaris, Irix, and so forth, please use Netatalk 2.2.6.  If you need TCP/IP support, please use Netatalk 3.x.

* Many superfluous functions have been removed.  These include authentication modules that are not supported by AppleTalk Client 3.7.4 and earlier, filesystem quota support, and so forth.

* Daemons log unilaterally to /var/log/netatalk/${DAEMON}.log, and ignore loglevel.  Loglevels will be restored when I consider this codebase to be bulletproof.

* Obsolete CNID database backends have been disabled.  DBD and "last" are the only backends supported.

* All compiler warnings have been dealt with.  The codebase compiles with -Wall -Werror (which is now the default compilation setting).  There are a few erroneous warnings which have been mitigated.

* The default AFP umask is now 022 instead of 000.
