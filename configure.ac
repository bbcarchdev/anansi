## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright 2014-2017 BBC

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

AC_INIT([anansi],m4_esyscmd([/bin/sh m4/get-version.sh]),[RESFeedback@bbc.co.uk],,[https://github.com/bbcarchdev/anansi])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([scripts])
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

enable_apache_tests=yes

if test $enable_apache_tests = yes ; then
	AC_MSG_CHECKING([which TCP/IP port unit tests should launch an Apache instance on])
	AC_ARG_WITH([test-apache-port],
		[AS_HELP_STRING([--with-test-apache-port=PORT],[specify TCP/IP port that an Apache instance should be launched on for tests])],
		[test_apache_port=$withval],[test_apache_port=no])
	if test $test_apache_port = no ; then
		AC_MSG_RESULT([none (these tests will not be run)])
		enable_apache_tests=no
	else
		AC_MSG_RESULT([$test_apache_port])
	fi
fi

if test $enable_apache_tests = yes ; then
	found=no
	AC_PATH_PROGS([APACHE],[apache24 apache22 apache2 httpd],[false],[/sbin:/usr/sbin:/usr/local/sbin:$PATH])
	if test x$APACHE = xfalse ; then
		found=no
	else
		found=yes
		test_apache_daemon="$APACHE"
	fi
	AC_MSG_CHECKING([for the Apache HTTPD binary])
	if test $found = no ; then
		AC_MSG_RESULT([not found (these tests will not be run)])
		enable_apache_tests=no
	else
		AC_MSG_RESULT([$test_apache_daemon])
	fi
fi

if test $enable_apache_tests = yes ; then
	AC_MSG_CHECKING([for Apache modules path])
	found=no
	for dir in /usr/lib/apache2/modules /usr/libexec/apache2 ; do
		for ext in so dylib dll ; do
			if test -d $dir && test -r $dir/mod_negotiation.$ext ; then
				test_apache_modules=$dir
				test_apache_modext=$ext
				found=yes
				break
			fi
		done
		test $found = yes && break
	done
	if test $found = no ; then
		AC_MSG_RESULT([not found (these tests will not be run)])
		enable_apache_tests=no
	else
		AC_MSG_RESULT([$test_apache_modules])
		AC_MSG_CHECKING([for Apache module file extension])
		AC_MSG_RESULT([$test_apache_modext])
	fi
fi

AC_MSG_CHECKING([whether to enable Apache-based unit tests])
AC_MSG_RESULT([$enable_apache_tests])

AC_SUBST([enable_apache_tests])
AC_SUBST([test_apache_daemon])
AC_SUBST([test_apache_port])
AC_SUBST([test_apache_modules])
AC_SUBST([test_apache_modext])

AC_CONFIG_FILES([
Makefile
libsupport/Makefile
libcrawl/Makefile
libcrawl/caches/Makefile
libcrawl/utils/Makefile
libspider/Makefile
libspider/queues/Makefile
libspider/processors/Makefile
libspider/policies/Makefile
crawler/Makefile
init.d/Makefile
m4/Makefile
docs/Makefile
docbook-html5/Makefile
t/Makefile
t/conf/Makefile
t/conf/apache2.conf
t/wwwroot/Makefile
])

AC_CONFIG_FILES([init.d/crawler:init.d/crawler.sh.in],[chmod +x init.d/crawler])

AC_CONFIG_FILES([t/run-apache-test.sh],[chmod +x t/run-apache-test.sh])

AC_OUTPUT
