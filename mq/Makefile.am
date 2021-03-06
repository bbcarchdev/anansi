## A libmq plugin for anansi:/// queues
##
## Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
##
## Copyright (c) 2014-2017 BBC
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

AM_CPPFLAGS = @AM_CPPFLAGS@ \
	$(LIBURI_CPPFLAGS) \
	$(LIBMQ_CPPFLAGS) \
	$(LIBCLUSTER_CPPFLAGS) \
	$(LIBSQL_CPPFLAGS)

mqmoduledir = $(libdir)/mq/plugins

mqmodule_LTLIBRARIES = anansi.la

## anansi.so is a plug-in for libmq which registers queue schemes in
## the form anansi+[DBSCHEME]://[DBINFO]
##
## e.g.:
##
## anansi+pgsql://user:pass@host/dbname

anansi_la_SOURCES = p_anansi-plugin.h \
	mq.c

anansi_la_LDFLAGS = -avoid-version -module -no-undefined

anansi_la_LIBADD = $(LIBURI_LOCAL_LIBS) $(LIBURI_LIBS) \
	$(LIBMQ_LOCAL_LIBS) $(LIBMQ_LIBS) \
	$(LIBCLUSTER_LOCAL_LIBS) $(LIBCLUSTER_LIBS) \
	$(LIBSQL_LOCAL_LIBS) $(LIBSQL_LIBS)

reconf:
	autoreconf -i && $(SHELL) ./config.status --recheck && $(SHELL) ./config.status
