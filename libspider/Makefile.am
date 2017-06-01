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

SUBDIRS = queues processors policies .

AM_CPPFLAGS = @AM_CPPFLAGS@ @CPPFLAGS@ \
	-I$(top_builddir)/libcrawl -I$(top_srcdir)/libcrawl \
	-I$(top_builddir)/libsupport -I$(top_srcdir)/libsupport

noinst_LTLIBRARIES = libspider.la

libspider_la_SOURCES = p_libcrawld.h libcrawld.h \
	context.c processor.c queue.c policy.c

libspider_la_LIBADD = \
	queues/libqueues.la \
	processors/libprocessors.la \
	policies/libpolicies.la \
	$(top_builddir)/libcrawl/libcrawl.la
