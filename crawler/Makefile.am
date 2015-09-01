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

SUBDIRS = queues processors .

dist_doc_DATA = crawl.conf

AM_CPPFLAGS = @AM_CPPFLAGS@ @CPPFLAGS@ \
	-I$(top_builddir)/libcrawl -I$(top_srcdir)/libcrawl \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport

noinst_LTLIBRARIES = libcrawld.la

sbin_PROGRAMS = crawld

bin_PROGRAMS = crawler-add

crawld_SOURCES = p_crawld.h crawld.c cluster.c thread.c
crawld_LDADD = libcrawld.la $(LIBCLUSTER_LOCAL_LIBS) $(LIBCLUSTER_LIBS)

crawler_add_SOURCES = p_crawld.h add.c
crawler_add_LDADD = libcrawld.la $(LIBURI_LOCAL_LIBS) $(LIBURI_LIBS)

libcrawld_la_SOURCES = p_libcrawld.h libcrawld.h \
	context.c processor.c queue.c policy.c

libcrawld_la_LIBADD = \
	queues/libqueues.la \
	processors/libprocessors.la \
	$(top_builddir)/libcrawl/libcrawl.la \
	$(top_builddir)/libsupport/libsupport.la

install-data-hook:
	$(INSTALL) -d $(DESTDIR)$(sysconfdir)
	test -f $(DESTDIR)$(sysconfdir)/crawl.conf || $(INSTALL) -m 644 $(srcdir)/crawl.conf $(DESTDIR)$(sysconfdir)/crawl.conf
