/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC
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

static void *thread_handler_(void *arg);
static int thread_setup_(CONTEXT *context, CRAWL *crawler);
static int thread_prefetch_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

static char *cache, *username, *password, *endpoint; 
static CONTEXT **contexts;
static int activethreads;
static pthread_mutex_t lock;
static pthread_cond_t createcond;
static pthread_mutex_t createlock;

volatile int crawld_terminate = 0;

int
thread_init(void)
{
	/* Obtain global options which will be applied to each thread */
	cache = config_geta("cache:uri", NULL);
	username = config_geta("cache:username", NULL);
	password = config_geta("cache:password", NULL);
	endpoint = config_geta("cache:endpoint", NULL);
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&createcond, NULL);
	pthread_mutex_init(&createlock, NULL);
	return 0;
}

int
thread_cleanup(void)
{
	pthread_mutex_destroy(&lock);
	crawl_free(NULL, cache);
	crawl_free(NULL, username);
	crawl_free(NULL, password);
	crawl_free(NULL, endpoint);
	return 0;
}

int
thread_run(void)
{
	int c, nthreads;

	nthreads = crawl_cluster_inst_threads();
	log_printf(LOG_DEBUG, "creating %d crawler threads\n", nthreads);
	contexts = (CONTEXT **) crawl_alloc(NULL, nthreads * sizeof(CONTEXT *));
	if(!contexts)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_NOMEM ": failed to allocate memory for thread contexts\n");
		return -1;
	}

	for(c = 0; c < nthreads; c++)
	{
		log_printf(LOG_DEBUG, "launching thread %d\n", c + 1);
		if(thread_create(c))
		{			
			log_printf(LOG_CRIT, MSG_C_CRAWL_THREADCREATE " (thread #%d)\n", c + 1);
			return -1;
		}
	}
	log_printf(LOG_DEBUG, "main thread is now sleeping\n");
	while(!crawld_terminate)
	{
		pthread_mutex_lock(&lock);
		c = activethreads;
		pthread_mutex_unlock(&lock);
		if(!c)
		{
			break;
		}
		sleep(1);
	}
	thread_terminate();
	return 0;
}

int
thread_terminate(void)
{
	int nthreads, c, alive;

	nthreads = crawl_cluster_inst_threads();
	log_printf(LOG_NOTICE, MSG_N_CRAWL_TERMINATETHREADS "\n");   
	pthread_mutex_lock(&lock);
	crawld_terminate = 1;
	for(c = 0; c < nthreads; c++)
	{
		if(contexts[c])
		{
			contexts[c]->api->terminate(contexts[c]);
		}
	}
	pthread_mutex_unlock(&lock);
	log_printf(LOG_INFO, MSG_I_CRAWL_TERMINATEWAIT "\n");
	while(1)
	{
		alive = 0;
		pthread_mutex_lock(&lock);
		for(c = 0; c < nthreads; c++)
		{
			if(contexts[c])
			{
				alive = 1;
			}
		}
		pthread_mutex_unlock(&lock);
		if(!alive)
		{
			break;
		}
		sleep(1);
	}
	log_printf(LOG_NOTICE, MSG_N_CRAWL_STOPPED "\n");
	return 0;
}

int
thread_create(int offset)
{
	CONTEXT *context;
	CRAWL *crawl;
	pthread_t thread;
	int oneshot;
	
	oneshot = config_get_bool("crawler:oneshot", 0);
	context = context_create(offset);
	crawl = context->api->crawler(context);
	if(!context)
	{
		return -1;
	}
	if(cache)
	{
		if(crawl_set_cache_path(crawl, cache))
		{
			return -1;
		}
	}
	if(username)
	{
		if(crawl_set_username(crawl, username))
		{
			return -1;
		}
	}
	if(password)
	{
		if(crawl_set_password(crawl, password))
		{
			return -1;
		}
	}
	if(endpoint)
	{
		if(crawl_set_endpoint(crawl, endpoint))
		{
			return -1;
		}
	}
	if(oneshot)
	{
		context->api->set_oneshot(context);
	}
	/* Launch the thread and wait for the signal from it that it's been
	 * created.
	 */
	pthread_mutex_lock(&createlock);
	pthread_create(&thread, NULL, thread_handler_, (void *) context);
	pthread_cond_wait(&createcond, &createlock);
	log_printf(LOG_DEBUG, "thread %d has started\n", offset + 1);
	pthread_mutex_unlock(&createlock);
	return 0;
}

/* The body of a single crawl thread */
static void *
thread_handler_(void *arg)
{
	CONTEXT *context;
	CRAWL *crawler;
	int instid, threadid, threadcount, crawlercount, newbase, newthreads;
	const char *env;

	/* no addref() of the context because this thread is given ownership of the
	 * object.
	 */
	context = (CONTEXT *) arg;
	crawler = context->api->crawler(context);
	instid = crawl_cluster_inst_id();
	threadcount = crawl_cluster_inst_threads();
	crawlercount = crawl_cluster_threads();
	env = crawl_cluster_env();
	threadid = context->api->thread_id(context);
	if(thread_setup_(context, crawler))
	{
		context->api->release(context);
		pthread_mutex_lock(&createlock);
		pthread_cond_signal(&createcond);
		pthread_mutex_unlock(&createlock);
		return NULL;
	}
	pthread_mutex_lock(&lock);
	activethreads++;
	contexts[threadid] = context;
	pthread_mutex_unlock(&lock);
	log_printf(LOG_NOTICE, MSG_N_CRAWL_READY " [%s] crawler %d/%d (thread %d/%d)\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount);
	pthread_mutex_lock(&createlock);
	pthread_cond_signal(&createcond);
	pthread_mutex_unlock(&createlock);
	while(!context->api->terminated(context))
	{
		newbase = crawl_cluster_inst_id();
		newthreads = crawl_cluster_threads();
		if(newbase != instid || newthreads != crawlercount)
		{
			/* Cluster has re-balanced */
			context->api->set_threads(context, newthreads);
			context->api->set_base(context, newbase);
			if(newbase == -1 || !newthreads)
			{
				log_printf(LOG_NOTICE, MSG_N_CRAWL_SUSPENDED " [%s] crawler %d/%d (thread %d/%d)\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount);
				instid = newbase;
				crawlercount = newthreads;
				sleep(SUSPEND_WAIT);
				continue;
			}
			if(instid == -1)
			{
				log_printf(LOG_NOTICE, MSG_N_CRAWL_RESUMING " [%s] crawler %d/%d (thread %d/%d) is now %d/%d\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount, newbase + threadid + 1, newthreads);
			}
			else
			{
				log_printf(LOG_INFO, MSG_I_CRAWL_THREADBALANCED " [%s] crawler %d/%d (thread %d/%d) is now %d/%d\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount, newbase + threadid + 1, newthreads);
			}
			crawlercount = newthreads;
			instid = newbase;
		}
		/* Fetch and process a single item from the queue */
		if(crawl_perform(crawler))
		{
			
			log_printf(LOG_CRIT, MSG_C_CRAWL_FAILED ": %s\n", strerror(errno));
			break;
		}
		if(context->api->oneshot(context))
		{
			break;
		}
		if(context->api->terminated(context))
		{
			break;
		}
		sleep(1);
	}
	log_printf(LOG_NOTICE, MSG_N_CRAWL_TERMINATING " [%s] crawler %d/%d (thread %d/%d)\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount);
	context->api->terminate(context);
	pthread_mutex_lock(&lock);
	contexts[threadid] = NULL;
	activethreads--;
	context->api->release(context);
	pthread_mutex_unlock(&lock);
	return NULL;
}

static int
thread_setup_(CONTEXT *context, CRAWL *crawler)
{
	if(processor_init_context(context))
	{
		return -1;
	}
	if(queue_init_context(context))
	{
		return -1;
	}
	if(policy_init_context(context))
	{
		return -1;
	}
	if(crawl_set_prefetch(crawler, thread_prefetch_))
	{
		return -1;
	}
	context->api->set_threads(context, crawl_cluster_threads());
	context->api->set_base(context, crawl_cluster_inst_id());
	return 0;
}

static int
thread_prefetch_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	CONTEXT *context;

	(void) crawl;
	(void) uri;

	context = (CONTEXT *) userdata;

	if(crawld_terminate)
	{
		return -1;
	}
	log_printf(LOG_DEBUG, "Fetching %s\n", uristr);
	return 0;
}
