## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2017 BBC.

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

LIBS = @LIBS@ \
	$(top_builddir)/libcrawl/libcrawl.la $(top_srcdir)/libcrawl/libcrawl.la

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	-I$(top_builddir)/libcrawl -I$(top_srcdir)/libcrawl

bin_PROGRAMS = crawl-fetch crawl-locate crawl-mirror libcrawl-config

libcrawl_config_SOURCES = crawl-config.c

crawl_fetch_LDADD = $(LIBJANSSON_LOCAL_LIBS) $(LIBJANSSON_LIBS)
crawl_locate_LDADD = $(LIBJANSSON_LOCAL_LIBS) $(LIBJANSSON_LIBS)
crawl_mirror_LDADD = $(LIBXML2_LOCAL_LIBS) $(LIBXML2_LIBS) $(LIBURI_LOCAL_LIBS) $(LIBURI_LIBS)
