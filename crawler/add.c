/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC.
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

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <errno.h>
# include <unistd.h>

# include "libcrawl.h"
# include "libcrawld.h"
# include "libsupport.h"
# include "liburi.h"

static const char *short_program_name = "add";
static const char *uristr;
static int force;
static int loop;

static int config_defaults(void);
static int process_args(int argc, char **argv);
static void usage(void);

/* Add a resource to the crawl queue if it doesn't already exist */

int
main(int argc, char **argv)
{
	CONTEXT *context;
	CRAWL *crawler;
	QUEUE *queue;
	URI *uri;
	int r, err, skip, added, line;
	char *uribuf, *t;
	size_t uribuflen;

	if(process_args(argc, argv))
	{
		return 1;
	}
	log_set_ident(short_program_name);
	log_set_stderr(1);
	log_set_syslog(0);
	log_set_facility(LOG_USER);
	log_set_level(LOG_NOTICE);
	config_init(config_defaults);	
	if(config_load(NULL))
	{
		return 1;
	}
	log_set_use_config(1);
	log_reset();
	if(queue_init())
	{
		return 1;
	}
	context = context_create(0);
	if(!context)
	{
		return 1;
	}
	crawler = context->api->crawler(context);
	crawl_set_verbose(crawler, config_get_int("crawler:verbose", 0));
	if(queue_init_context(context))
	{
		return 1;
	}
	queue = context->api->queue(context);
	err = 0;
	skip = 0;
	line = 0;
	added = 0;
	if(loop)
	{
		uribuflen = 1023;
		uribuf = (char *) crawl_alloc(crawler, uribuflen + 1);
		if(!uribuf)
		{
			log_printf(LOG_CRIT, "failed to allocate %u bytes for URI buffer\n", (unsigned) (uribuflen + 1));
			return 1;
		}
		while((fgets(uribuf, uribuflen, stdin)))
		{
			if(skip)
			{
				if(strchr(uribuf, '\n'))
				{
					skip = 0;
					line++;
				}
				continue;
			}
			t = strchr(uribuf, '\n');
			if(!t)
			{
				log_printf(LOG_ERR, MSG_E_CRAWL_LINETOOLONG " at stdin:%d\n", line + 1);
				skip = 1;
				err = 1;
				continue;
			}
			line++;
			*t = 0;
			uri = uri_create_str(uribuf, NULL);
			if(!uri)
			{				
				log_printf(LOG_ERR, MSG_E_CRAWL_URIPARSE "<%s> at stdin:%d\n", uribuf, line);
				err = 1;
				continue;
			}
			if(force)
			{
				r = queue->api->force_add(queue, uri, uribuf);
			}
			else
			{
				r = queue->api->add(queue, uri, uribuf);
			}
			if(r)
			{
				log_printf(LOG_ERR, MSG_E_CRAWL_ADDFAILED " <%s> at stdin:%d\n", uribuf, line);
				err = 1;
			}
			else
			{
				log_printf(LOG_DEBUG, "stdin:%d: added <%s> to the crawler queue\n", line, uribuf);
				added++;
			}
			uri_destroy(uri);					
		}
		crawl_free(crawler, uribuf);
		log_printf(LOG_NOTICE, MSG_N_CRAWL_UPDATED ": added %d URIs\n", added);
	}
	else
	{
		uri = uri_create_str(uristr, NULL);
		if(!uri)
		{
			log_printf(LOG_CRIT, MSG_C_CRAWL_URIPARSE " <%s>\n", uristr);
			return 1;
		}
		if(force)
		{
			r = queue->api->force_add(queue, uri, uristr);
		}
		else
		{
			r = queue->api->add(queue, uri, uristr);
		}
		if(r)
		{
			log_printf(LOG_CRIT, MSG_C_CRAWL_ADDFAILED " <%s>\n", uristr);
			err = 1;
		}
		else
		{
			
			log_printf(LOG_NOTICE, MSG_N_CRAWL_UPDATED ": added <%s>\n", uristr);
		}
		uri_destroy(uri);
	}
	context->api->release(context);
	queue_cleanup();
	return err;
}

static int
config_defaults(void)
{
	config_set_default("log:ident", short_program_name);
	config_set_default("log:facility", "daemon");
	config_set_default("log:level", "notice");
	config_set_default("log:stderr", "1");
	config_set_default("log:syslog", "0");
	config_set_default("global:configFile", SYSCONFDIR "/crawl.conf");
	config_set_default("instance:crawler", "1");
	config_set_default("instance:crawlercount", "1");
	config_set_default("instance:cache", "1");
	config_set_default("instance:cachecount", "1");
	config_set_default("instance:threadcount", "1");	
	config_set_default("queue:name", "db");
	config_set_default("processor:name", "rdf");
	config_set_default("cache:uri", "cache");	
	return 0;
}

static int
process_args(int argc, char **argv)
{
	int c;

	short_program_name = strrchr(argv[0], '/');
	if(short_program_name)
	{
		short_program_name++;
	}
	else
	{
		short_program_name = argv[0];
	}
	while((c = getopt(argc, argv, "hc:fl")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'c':
			config_set("global:configFile", optarg);
			break;
		case 'f':
			force = 1;
			break;
		case 'l':
			loop = 1;
			break;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if((loop && argc != 0) || (!loop && argc != 1))
	{
		usage();
		return 1;
	}
	if(!loop)
	{
		uristr = argv[0];
	}
	return 0;
}

static void
usage(void)
{
	printf("Usage: %s [OPTIONS] URI\n"
		   "\n"
		   "OPTIONS is one or more of:\n"
		   "  -h                   Print this usage message and exit\n"
		   "  -c PATH              Load PATH as the configuration file\n"
		   "  -f                   Add URI in 'FORCED' state (next fetch will ignore cache)\n"
		   "  -l                   Read a list of URIs from standard input\n",
		   short_program_name);
}


	
