/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC.
 */

/*
 * Copyright 2013 Mo McRoberts.
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <jansson.h>
#include <syslog.h>

#include "libcrawl.h"

static const char *progname;

static void logger(int level, const char *fmt, va_list ap);

/* Locate the cached data for the specified URI using libcrawl */
int
main(int argc, char **argv)
{
	CRAWL *crawl;
	CRAWLOBJ *obj;
	json_t *headers;

	if((progname = strrchr(argv[0], '/')))
	{
		progname++;
	}
	else
	{
		progname = argv[0];
	}	
	if(argc != 2)
	{
		fprintf(stderr, "Usage: %s URI\n", progname);
		exit(EXIT_FAILURE);
	}
	crawl = crawl_create();
	crawl_set_logger(crawl, logger);
	obj = crawl_locate(crawl, argv[1]);
	if(!obj)
	{
		fprintf(stderr, "%s: failed to locate resource: %s\n", progname, strerror(errno));
		crawl_destroy(crawl);
		return 1;
	}
	printf("status: %d\n", crawl_obj_status(obj));
	printf("updated: %ld\n", (long) crawl_obj_updated(obj));
	printf("key: %s\n", crawl_obj_key(obj));
	printf("payload path: %s\n", crawl_obj_payload(obj));
	printf("payload size: %llu\n", (unsigned long long) crawl_obj_size(obj));
	headers = crawl_obj_headers(obj, 0);
	if(headers)
	{		
		printf("headers: ");
		json_dumpf(headers, stdout, 0);
		printf("\n");
		json_decref(headers);
	}
	crawl_obj_destroy(obj);
	crawl_destroy(crawl);
	return 0;
}

static void
logger(int level, const char *fmt, va_list ap)
{
	if(level <= LOG_NOTICE)
	{
		fprintf(stderr, "%s: ", progname);
		vfprintf(stderr, fmt, ap);
	}
}
