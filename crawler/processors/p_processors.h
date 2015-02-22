/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef P_PROCESSORS_H_
# define P_PROCESSORS_H_                1

# define _BSD_SOURCE                   1
# define _FILE_OFFSET_BITS             64

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <ctype.h>
# include <unistd.h>
# include <syslog.h>

# define PROCESSOR_STRUCT_DEFINED       1

# include "libsupport.h"
# include "libcrawld.h"
# include <curl/curl.h>
# include <librdf.h>

typedef int (*rdf_filter_cb)(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model);

struct processor_struct
{
	struct processor_api_struct *api;
	unsigned long refcount;
	CRAWL *crawl;
	librdf_world *world;
	librdf_storage *storage;
	librdf_model *model;
	librdf_uri *uri;
	char *content_type;
	const char *parser_type;
	FILE *fobj;
	rdf_filter_cb filter;
	char **license_predicates;
	char **license_whitelist;
	char **license_blacklist;
};

extern PROCESSOR *rdf_create(CRAWL *crawler);
extern PROCESSOR *lod_create(CRAWL *crawler);

extern librdf_world *rdf_world(PROCESSOR *me);
extern int rdf_set_filter(PROCESSOR *me, rdf_filter_cb filter);

#endif /*!P_PROCESSORS_H_*/
