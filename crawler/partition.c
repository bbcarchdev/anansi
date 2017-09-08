/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2017 BBC.
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
# include "libspider.h"
# include "libsupport.h"
# include "liburi.h"
# include "libsql.h"

static const char *short_program_name = "add";
static const char *uristr;
static const char *partition;
static int clear, verbose;

static int config_defaults(void);
static int process_args(int argc, char **argv);
static void usage(void);

/* Set (or unset) the partition a particular root, and all of its resources,
 * should be placed in.
 */

int
main(int argc, char **argv)
{
	SQL *db;
	int err;
	const char *t;

	err = 0;
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
	/* Connect the spider to the queue that we're going to use */
	t = config_getptr_unlocked("queue:uri", "mysql://localhost/crawl");
	db = sql_connect(t);
	if(!db)
	{
		log_printf(LOG_CRIT, MSG_C_DB_CONNECT " <%s>\n", t);
		err = 1;
	}
	if(!err)
	{		
		if(sql_executef(db, "UPDATE \"crawl_root\" SET \"partition\" = %Q WHERE \"uri\" = %Q",
						partition, uristr))
		{
			log_printf(LOG_CRIT, MSG_C_DB_SQL);
			err = 1;
		}
	}
	if(!err)
	{
		log_printf(LOG_NOTICE, MSG_I_DB_PARTUPDATED);
	}
	sql_disconnect(db);
	if(err)
	{
		return err;
	}
	return 0;
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
	while((c = getopt(argc, argv, "hc:rv")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'c':
			config_set("global:configFile", optarg);
			break;
		case 'r':
			clear = 1;
			break;
		case 'v':
			verbose = 1;
			log_set_level(LOG_INFO);			
			break;
		default:
			usage();
			return -1;
		}
	}
	argc -= optind;
	argv += optind;
	if((clear && argc != 1) || (!clear && argc != 2))
	{
		usage();
		return 1;
	}
	uristr = argv[0];
	if(!clear)
	{
		partition = argv[1];
	}
	return 0;
}

static void
usage(void)
{
	printf("Usage: %s [OPTIONS] ROOT-URI PARTITION\n"
		   "Usage: %s -r [OPTIONS] ROOT-URI\n"
		   "\n"
		   "OPTIONS is one or more of:\n"
		   "  -h                   Print this usage message and exit\n"
		   "  -c PATH              Load PATH as the configuration file\n"
		   "  -r                   Reset the partition on a root\n"
		   "  -v                   Produce verbose output\n",
		   short_program_name, short_program_name);
}


	
