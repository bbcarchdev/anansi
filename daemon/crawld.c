/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC.
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

#include "p_crawld.h"

static int process_args(int argc, char **argv);
static int config_defaults(void);
static void usage(void);

static const char *short_program_name = "crawler";

int
main(int argc, char **argv)
{
	log_set_stderr(1);
	log_set_facility(LOG_DAEMON);
	log_set_level(LOG_NOTICE);
	config_init(config_defaults);
	if(process_args(argc, argv))
	{
		exit(EXIT_FAILURE);
	}
	log_set_ident(short_program_name);
	if(config_load(NULL))
	{
		return 1;
	}
	log_set_use_config(1);
	log_reset();
	if(thread_init())
	{
		return 1;
	}
	if(policy_init())
	{
		return 1;
	}
	if(queue_init())
	{
		return 1;
	}
	if(processor_init())
	{
		return 1;
	}

	/* Perform a single thread's crawl actions */
	if(thread_create(0))
	{
		return 1;
	}
	
	processor_cleanup();
	queue_cleanup();
	thread_cleanup();
	return 0;
}

static int
config_defaults(void)
{
	config_set_default("log:ident", "crawld");
	config_set_default("log:facility", "daemon");
	config_set_default("log:level", "notice");
	/* Log to stderr initially, until daemonized */
	config_set_default("log:stderr", "1");
	config_set_default("global:configFile", SYSCONFDIR "/crawl.conf");
	config_set_default("instance:crawler", "1");
	config_set_default("instance:crawlercount", "1");
	config_set_default("instance:cache", "1");
	config_set_default("instance:cachecount", "1");
	config_set_default("instance:threadcount", "1");	
	config_set_default("crawl:pidfile", LOCALSTATEDIR "/run/crawld.pid");
	config_set_default("crawl:queue", "db");
	config_set_default("crawl:processor", "rdf");
	config_set_default("crawl:cache", "cache");
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
	while((c = getopt(argc, argv, "hfdc:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'f':
			config_set("crawl:detach", "0");
			break;
		case 'd':
			config_set("log:level", "debug");
			config_set("log:stderr", "1");
			config_set("db:debug-queries", "1");
			config_set("db:debug-errors", "1");
			config_set("crawl:detach", "0");
			break;
		case 'c':
			config_set("global:configFile", optarg);
			break;
		default:
			usage();
			return -1;
		}
	}
	argv += optind;
	argc -= optind;
	if(argc)
	{
		usage();
		return -1;
	}
	return 0;
}

static void
usage(void)
{
	printf("Usage: %s [OPTIONS]\n"
		   "\n"
		   "OPTIONS is one or more of:\n"
		   "  -h                   Print this notice and exit\n"
		   "  -f                   Don't detach and run in the background\n"
		   "  -d                   Enable debug output to standard error\n"
		   "  -c FILE              Specify path to configuration file\n",
		   short_program_name);
}
