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

#include "p_crawld.h"

static int process_args(int argc, char **argv);
static int config_defaults(void);
static void usage(void);
static pid_t start_daemon(const char *configkey, const char *pidfile);
static void signal_handler(int signo);

static const char *short_program_name = "crawler";

int
main(int argc, char **argv)
{
	int detach;
	pid_t child;

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
	if(cluster_init())
	{
		return 1;
	}
	detach = config_get_bool("crawler:detach", 1);
	signal(SIGHUP, SIG_IGN);
	if(detach)
	{
		signal(SIGINT, SIG_IGN);
	}
	else
	{
		signal(SIGINT, signal_handler);
	}
	signal(SIGALRM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);
	signal(SIGTERM, signal_handler);
	if(detach)
	{
		child = start_daemon("crawler:pidfile", LOCALSTATEDIR "/run/crawld.pid");
		if(child < 0)
		{
			return 1;
		}
		if(child > 0)
		{
			return 0;
		}
	}

	if(thread_run())
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
	config_set_default("instance:cluster", "anansi");
	config_set_default("instance:crawler", "1");
	config_set_default("instance:crawlercount", "1");
	config_set_default("instance:cache", "1");
	config_set_default("instance:cachecount", "1");
	config_set_default("instance:threadcount", "1");	
	config_set_default("crawler:pidfile", LOCALSTATEDIR "/run/crawld.pid");
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
	while((c = getopt(argc, argv, "hfdc:")) != -1)
	{
		switch(c)
		{
		case 'h':
			usage();
			exit(EXIT_SUCCESS);
		case 'f':
			config_set("crawler:detach", "0");
			break;
		case 'd':
			config_set("crawler:detach", "0");
			config_set("log:level", "debug");
			config_set("log:stderr", "1");
			config_set("queue:debug-queries", "1");
			config_set("queue:debug-errors", "1");
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

pid_t
start_daemon(const char *configkey, const char *pidfile)
{
	pid_t child;    
	char *file;
	FILE *f;
	int fd;

	if(configkey)
	{
		file = config_geta(configkey, pidfile);
	}
	else if(pidfile)
	{
		file = strdup(pidfile);
		if(!file)
		{
			log_printf(LOG_CRIT, "failed to allocate memory: %s\n", strerror(errno));
			return -1;
		}
	}
	else
	{
		file = NULL;
	}
	child = fork();
	if(child == -1)
	{
		log_printf(LOG_CRIT, "failed to fork child process: %s\n", strerror(errno));
		free(file);
		return -1;
	}
	if(child > 0)
	{
		/* Parent process */
		if(file)
		{
			f = fopen(file, "w");
			if(!f)
			{
				log_printf(LOG_CRIT, "failed to open PID file %s: %s\n", file, strerror(errno));
				return child;
			}
			fprintf(f, "%ld\n", (long int) child);
			fclose(f);
		}
		return child;
	}
	/* Child process */
	free(file);
	umask(0);
	log_reset();
	if(setsid() < 0)
	{
		log_printf(LOG_CRIT, "failed to create new process group: %s\n", strerror(errno));
		return -1;
	}
	if(chdir("/"))
	{
		log_printf(LOG_CRIT, "failed to change working directory: %s\n", strerror(errno));
		return -1;
	}
    close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	do
	{
		fd = open("/dev/null", O_RDWR);
	}
	while(fd == -1 && errno == EINTR);
	if(fd == -1)
	{
		log_printf(LOG_CRIT, "failed to open /dev/null: %s\n", strerror(errno));
		return -1;
	}
	if(fd != 0)
	{
		dup2(fd, 0);
	}
	if(fd != 1)
	{
		dup2(fd, 1);
	}
	if(fd != 2)
	{
		dup2(fd, 2);
	}
	return 0;
}

static void
signal_handler(int signo)
{
	(void) signo;

	log_printf(LOG_NOTICE, "received request to terminate\n");
	crawld_terminate = 1;
}

