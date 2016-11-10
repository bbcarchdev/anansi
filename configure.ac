## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright 2014-2015 BBC

## Copyright 2013 Mo McRoberts.
##
##  Licensed under the Apache License, Version 2.0 (the "License");
##  you may not use this file except in compliance with the License.
##  You may obtain a copy of the License at
##
##      http://www.apache.org/licenses/LICENSE-2.0
##
##  Unless required by applicable law or agreed to in writing, software
##  distributed under the License is distributed on an "AS IS" BASIS,
##  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
##  See the License for the specific language governing permissions and
##  limitations under the License.
##

AC_INIT([anansi],[master])

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign subdir-objects])
AM_MAINTAINER_MODE
AC_CONFIG_HEADER([config.h])

BT_PROG_XCODE
AC_PROG_CC([clang c99 gcc cc])
AC_PROG_CC_C99
BT_PROG_CC_WARN
BT_PROG_CC_DEBUG

BT_ENABLE_POSIX_FULL
AC_SYS_LARGEFILE

LT_INIT
BT_DEFINE_PREFIX

dnl save and restore LIBS -- each set will be used explicitly where needed

BT_REQUIRE_PTHREAD

save_LIBS="$LIBS"
BT_CHECK_COMMONCRYPTO(,[
	BT_REQUIRE_OPENSSL
])
BT_REQUIRE_LIBCURL
BT_REQUIRE_LIBRDF
BT_REQUIRE_LIBUUID
BT_REQUIRE_LIBXML2
BT_REQUIRE_LIBJANSSON

dnl Always check for possibly-included libraries last to prevent link failures
dnl (due to not-yet-built libraries) breaking tests
BT_REQUIRE_LIBAWSCLIENT_INCLUDED
BT_REQUIRE_LIBURI_INCLUDED
BT_REQUIRE_LIBSQL_INCLUDED
BT_REQUIRE_LIBCLUSTER_INCLUDED
LIBS="$save_LIBS"

extra_libs="$OPENSSL_INSTALLED_LIBS $LIBCURL_INSTALLED_LIBS $LIBURI_INSTALLED_LIBS $LIBJANSSON_INSTALLED_LIBS"
BT_DEFINE_PATH([LIBCRAWL_EXTRA_LIBS],[extra_libs],[Define to the additional libraries depended upon by an installed libcrawl])

use_docbook_html5=yes
BT_BUILD_DOCS

XML2HTMLFLAGS="--param \"html.linksfile\" \"'file://\$(abs_top_srcdir)/docbook-html5/res-links.xml'\" \
  --param \"html.navfile\" \"'file://\$(abs_top_srcdir)/docbook-html5/res-nav.xml'\" \
  --param \"html.ie78css\" \"'//bbcarchdev.github.io/painting-by-numbers/ie78.css'\""
AC_SUBST([XML2HTMLFLAGS])

LINKS='${top_srcdir}/docbook-html5/res-links.xml'
AC_SUBST([LINKS])

NAV='${top_srcdir}/docbook-html5/res-nav.xml'
AC_SUBST([NAV])

anansihtdocdir='${docdir}'
AC_SUBST([anansihtdocdir])

BT_DEFINE_PREFIX

AC_CONFIG_FILES([
Makefile
libsupport/Makefile
libcrawl/Makefile
libcrawl/caches/Makefile
util/Makefile
crawler/Makefile
crawler/queues/Makefile
crawler/processors/Makefile
init.d/Makefile
m4/Makefile
docs/Makefile
docbook-html5/Makefile
crawler/plug-ins/Makefile
crawler/plug-ins/logger/Makefile
])

AC_CONFIG_FILES([init.d/crawler:init.d/crawler.sh.in],[chmod +x init.d/crawler])

AC_OUTPUT
