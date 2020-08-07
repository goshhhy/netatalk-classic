netatalk-classic is an implementation of the AppleTalk Protocol Suite for Linux
and NetBSD.

netatalk-classic was forked from the last release of Netatalk that supported
AppleTalk (2.2.6).

The current release contains support for EtherTalk Phase I and II, classic
AppleTalk, NBP, ZIP, AEP, ATP, PAP, ASP, and AFP.

Support for obsolete platforms and non-AppleTalk functions were removed to keep
the codebase manageable (and reduce the potential attack surface).  If you
need to serve AppleTalk on TRU64, Solaris, Irix, and so forth, please use
Netatalk 2.2.6.  If you need TCP/IP support, please use Netatalk 3.x.

netatalk-classic is maintained by Christopher Kobayashi <software+github@disavowed.jp>
