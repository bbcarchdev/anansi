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

static int thread_setup(CONTEXT *context, CRAWL *crawler);
static int thread_prefetch(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

static char *cache, *username, *password, *endpoint; 

volatile int crawld_terminate = 0;

int
thread_init(void)
{
	/* Obtain global options which will be applied to each thread */
	cache = config_geta(CRAWLER_APP_NAME ":cache", NULL);
	username = config_geta("cache:username", NULL);
	password = config_geta("cache:password", NULL);
	endpoint = config_geta("cache:endpoint", NULL);
	return 0;
}

int
thread_cleanup(void)
{
	free(cache);
	free(username);
	free(password);
	free(endpoint);
	return 0;
}

int
thread_create(int offset)
{
	CONTEXT *context;
	
	context = context_create(offset);
	if(!context)
	{
		return -1;
	}
	if(cache)
	{
		if(crawl_set_cache_path(context->crawl, cache))
		{
			return -1;
		}
	}
	if(username)
	{
		if(crawl_set_username(context->crawl, username))
		{
			return -1;
		}
	}
	if(password)
	{
		if(crawl_set_password(context->crawl, password))
		{
			return -1;
		}
	}
	if(endpoint)
	{
		if(crawl_set_endpoint(context->crawl, endpoint))
		{
			return -1;
		}
	}
	thread_handler(context);
	return 0;
}

void *
thread_handler(void *arg)
{
	CONTEXT *context;
	CRAWL *crawler;
	int threadcount, crawlercount, cachecount;
	char *env;

	/* no addref() of the context because this thread is given ownership of the
	 * object.
	 */
	context = (CONTEXT *) arg;
	crawler = context->crawl;
	threadcount = config_get_int("instance:threadcount", 1);
	crawlercount = config_get_int("instance:crawlercount", 1);
	cachecount = config_get_int("instance:cachecount", 1);
	env = config_geta("instance:environment", "none");	
	crawl_set_verbose(crawler, config_get_bool(CRAWLER_APP_NAME ":verbose", 0));
	if(!thread_setup(context, crawler))
	{	
		log_printf(LOG_NOTICE, "[%s] crawler %d/%d (thread %d/%d), cache %d/%d ready", env, context->crawler_id, crawlercount, context->thread_id, threadcount, context->cache_id, cachecount);
		while(!crawld_terminate)
		{
			if(crawl_perform(crawler))
			{
				log_printf(LOG_CRIT, "crawl perform operation failed: %s\n", strerror(errno));
				break;
			}
			if(crawld_terminate)
			{
				break;
			}
			sleep(1);
		}
	}
	queue_cleanup_crawler(crawler, context);
	processor_cleanup_crawler(crawler, context);
	log_printf(LOG_NOTICE, "[%s] crawler %d/%d (thread %d/%d), cache %d/%d terminating", env, context->crawler_id, crawlercount, context->thread_id, threadcount, context->cache_id, cachecount);
	free(env);
	context->api->release(context);
	return NULL;
}

static int
thread_setup(CONTEXT *context, CRAWL *crawler)
{
	if(processor_init_crawler(crawler, context))
	{
		return -1;
	}
	if(queue_init_crawler(crawler, context))
	{
		return -1;
	}
	if(policy_init_crawler(crawler, context))
	{
		return -1;
	}
	if(crawl_set_prefetch(crawler, thread_prefetch))
	{
		return -1;
	}
	return 0;
}

static int
thread_prefetch(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	(void) crawl;
	(void) uri;
	(void) userdata;
	
	if(crawld_terminate)
	{
		return -1;
	}
	log_printf(LOG_INFO, "Fetching %s\n", uristr);
	return 0;
}
