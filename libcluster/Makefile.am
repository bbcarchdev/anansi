## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright 2015 BBC.
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

EXTRA_DIST = README.md LICENSE-2.0

noinst_LTLIBRARIES = libcluster.la

noinst_PROGRAMS = cluster-test

AM_CPPFLAGS = @AM_CPPFLAGS@ @CPPFLAGS@ @LIBUUID_CPPFLAGS@ \
	-I$(top_builddir)/libetcd -I$(top_builddir)/libetcd

libcluster_la_SOURCES = libcluster.h \
	p_libcluster.h \
	cluster.c static.c etcd.c

libcluster_la_LDFLAGS = @AM_LDFLAGS@ \
	@LIBURI_LDFLAGS@ \
	-noinst -avoid-version

libcluster_la_LIBADD = \
	@LIBURI_LOCAL_LIBS@ @LIBURI_LIBS@ \
	${top_builddir}/libetcd/libetcd.la \
	@LIBUUID_LOCAL_LIBS@ @LIBUUID_LIBS@ \
	@PTHREAD_LOCAL_LIBS@ @PTHREAD_LIBS@

cluster_test_LDADD = libcluster.la
