/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC.
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

#include "p_crawld.h"

static const char *short_program_name = "add";
static const char *uristr;

static int config_defaults(void);
static int process_args(int argc, char **argv);
static void usage(void);

/* Add a resource to the crawl queue if it doesn't already exist */

int
main(int argc, char **argv)
{
	CONTEXT *context;

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
	crawl_set_verbose(context->crawl, config_get_int("crawl:verbose", 0));
	if(queue_init_crawler(context->crawl, context))
	{
		return 1;
	}
	if(context->queue->api->add_uristr(context->queue, argv[1]))
	{
		log_printf(LOG_CRIT, "failed to add <%s> to the crawler queue\n", uristr);
		return 1;
	}
	log_printf(LOG_NOTICE, "added <%s> to the crawler queue\n", uristr);
	context->api->release(context);
	queue_cleanup();
	return 0;
}

static int
config_defaults(void)
{
	config_set_default("log:ident", "crawld-add");
	config_set_default("log:facility", "daemon");
	config_set_default("log:level", "notice");
	config_set_default("log:stderr", "1");
	config_set_default("log:syslog", "0");
	config_set_default("global:configFile", SYSCONFDIR "/crawl.conf");
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
	while((c = getopt(argc, argv, "hc:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'c':
			config_set("global:configFile", optarg);
			break;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if(argc != 1)
	{
		usage();
	}
	uristr = argv[0];
	return 0;
}

static void
usage(void)
{
	printf("Usage: %s [OPTIONS] URI\n"
		   "\n"
		   "OPTIONS is one or more of:\n"
		   "  -h                   Print this usage message and exit\n"
		   "  -c PATH              Load PATH as the configuration file\n",
		   short_program_name);
}


	
