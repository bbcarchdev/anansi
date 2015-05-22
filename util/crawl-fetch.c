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
#include <unistd.h>
#include <errno.h>
#include <jsondata.h>
#include <syslog.h>

#include "libcrawl.h"

static const char *progname = "fetch";
static const char *fetchuri = NULL;
static const char *accept = "*/*";
static const char *cache = NULL;
static const char *username = NULL;
static const char *password = NULL;
static const char *endpoint = NULL;
static int verbose = 0;

static void usage(void);
static int process_args(int argc, char **argv);
static void logger(int level, const char *fmt, va_list ap);

/* Immediately fetch the specified URI using libcrawl */
int
main(int argc, char **argv)
{
	CRAWL *crawl;
	CRAWLOBJ *obj;
	jd_var headers = JD_INIT;
	int r;

	if(process_args(argc, argv))
	{
		exit(EXIT_FAILURE);
	}
	crawl = crawl_create();	
	crawl_set_logger(crawl, logger);
	crawl_set_verbose(crawl, verbose);
	crawl_set_accept(crawl, accept);
	if(cache)
	{
		r = crawl_set_cache_path(crawl, cache);
		if(r)
		{
			fprintf(stderr, "%s: failed to set cache URI to <%s> (%d)\n", progname, cache, r);
			return 1;
		}
	}
	if(username)
	{
		crawl_set_username(crawl, username);
	}
	if(password)
	{
		crawl_set_password(crawl, password);
	}
	if(endpoint)
	{
		crawl_set_endpoint(crawl, endpoint);
	}
	obj = crawl_fetch(crawl, fetchuri, COS_NEW);
	if(!obj)
	{
		fprintf(stderr, "%s: failed to fetch resource: %s\n", progname, strerror(errno));
		crawl_destroy(crawl);
		return 1;
	}
	printf("status: %d\n", crawl_obj_status(obj));
	printf("updated: %ld\n", (long) crawl_obj_updated(obj));
	printf("key: %s\n", crawl_obj_key(obj));
	printf("payload path: %s\n", crawl_obj_payload(obj));
	printf("payload size: %llu\n", (unsigned long long) crawl_obj_size(obj));
	if(!crawl_obj_headers(obj, &headers, 0))
	{
		jd_printf("headers: %lJ\n", &headers);
	}
	crawl_obj_destroy(obj);
	crawl_destroy(crawl);
	return 0;
}

static int
process_args(int argc, char **argv)
{
	int c;

	progname = argv[0];
	while((c = getopt(argc, argv, "A:C:hvu:p:e:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'A':
			accept = optarg;
			break;
		case 'C':
			cache = optarg;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'u':
			username = optarg;
			break;
		case 'p':
			password = optarg;
			break;
		case 'e':
			endpoint = optarg;
			break;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(!argc || argc > 1)
	{
		usage();
		return -1;
	}
	fetchuri = argv[0];
	return 0;
}

static void
usage(void)
{
	printf("Usage: %s [OPTIONS] URI\n"
		   "\n"
		   "OPTIONS is one or more of:\n"
		   "  -h                        Print this notice and exit\n"
		   "  -A TYPES...               Specify the HTTP 'Accept' header\n"
		   "  -C URI                    Specify the cache location URI\n"
		   "  -u USER                   Specify cache username/access key\n"
		   "  -p PASS                   Specify cache password/secret\n"
		   "  -e NAME                   Specify cache endpoint\n"
		   "  -v                        Be verbose\n",
		   progname);
}

static void
logger(int level, const char *fmt, va_list ap)
{
	if(verbose || level <= LOG_NOTICE)
	{
		fprintf(stderr, "%s: ", progname);
		vfprintf(stderr, fmt, ap);
	}
}
