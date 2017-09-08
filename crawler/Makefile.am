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

dist_doc_DATA = crawl.conf

AM_CPPFLAGS = @AM_CPPFLAGS@ @CPPFLAGS@ \
	-I$(top_builddir)/libcrawl -I$(top_srcdir)/libcrawl \
	-I$(top_builddir)/libspider -I$(top_srcdir)/libspider \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport

sbin_PROGRAMS = crawld

bin_PROGRAMS = anansi-add anansi-partition

crawld_SOURCES = p_crawld.h crawld.c cluster.c thread.c
crawld_LDADD = $(top_builddir)/libspider/libspider.la \
	$(top_builddir)/libsupport/libsupport.la \
	$(LIBCLUSTER_LOCAL_LIBS) $(LIBCLUSTER_LIBS)

anansi_add_SOURCES = p_crawld.h add.c
anansi_add_LDADD = $(top_builddir)/libspider/libspider.la \
	$(top_builddir)/libsupport/libsupport.la \
	$(LIBCLUSTER_LOCAL_LIBS) $(LIBCLUSTER_LIBS) \
	$(LIBURI_LOCAL_LIBS) $(LIBURI_LIBS)

anansi_partition_SOURCES = p_crawld.h partition.c
anansi_partition_LDADD = $(top_builddir)/libspider/libspider.la \
	$(top_builddir)/libsupport/libsupport.la \
	$(LIBCLUSTER_LOCAL_LIBS) $(LIBCLUSTER_LIBS) \
	$(LIBURI_LOCAL_LIBS) $(LIBURI_LIBS)

install-data-hook:
	$(INSTALL) -d $(DESTDIR)$(sysconfdir)
	test -f $(DESTDIR)$(sysconfdir)/crawl.conf || $(INSTALL) -m 644 $(srcdir)/crawl.conf $(DESTDIR)$(sysconfdir)/crawl.conf
