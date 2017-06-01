/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2017 BBC
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

static void thread_init_(void);
static void thread_cleanup_(void);
static void *thread_handler_(void *arg);
static int thread_prefetch_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

static char *cache, *username, *password, *endpoint; 
static SPIDER **spiders;
static int activethreads;
static pthread_once_t thread_once_control = PTHREAD_ONCE_INIT;
static pthread_mutex_t lock;
static pthread_cond_t createcond;
static pthread_mutex_t createlock;

volatile int crawld_terminate = 0;

/* Create the crawl thread with the index 'offset' */
int
thread_create(int offset)
{
	SPIDER *spider;
	SPIDERCALLBACKS callbacks;
	CRAWL *crawl;
	pthread_t thread;
	int oneshot, r;
	char *t;

	pthread_once(&thread_once_control, thread_init_);
	oneshot = config_get_bool("crawler:oneshot", 0);
	memset(&callbacks, 0, sizeof(callbacks));
	callbacks.version = SPIDER_CALLBACKS_VERSION;
	callbacks.logger = log_vprintf;
	callbacks.config_geta = config_geta;
	callbacks.config_get_int = config_get_int;
	callbacks.config_get_bool = config_get_bool;
	spider = spider_create(&callbacks);
	if(!spider)
	{
		return -1;
	}
	spider->api->set_local_index(spider, offset);
	r = 0;
	crawl = spider->api->crawler(spider);
	if(cache)
	{
		if(crawl_set_cache_path(crawl, cache))
		{
			r = -1;
		}
	}
	if(username)
	{
		if(crawl_set_username(crawl, username))
		{
			r = -1;
		}
	}
	if(password)
	{
		if(crawl_set_password(crawl, password))
		{
			r = -1;
		}
	}
	if(endpoint)
	{
		if(crawl_set_endpoint(crawl, endpoint))
		{
			r = -1;
		}
	}
	if(oneshot)
	{
		spider->api->set_oneshot(spider);
	}
	if((t = config_geta("processor:name", NULL)))
	{		
		if(spider->api->set_processor_name(spider, t))
		{
			log_printf(LOG_CRIT, MSG_C_CRAWL_PROCESSORUNKNOWN ": '%s'\n", t);
			r = -1;
		}
		crawl_free(NULL, t);
	}
	spider->api->add_policy_name(spider, "schemes");
	spider->api->add_policy_name(spider, "content-types");
	if((t = config_geta("queue:name", NULL)))
	{
		if(!strcmp(t, "db"))
		{
			crawl_free(NULL, t);
			t = config_geta("queue:uri", "mysql://localhost/crawl");			
		}
	}
	if(t)
	{
		if(spider->api->set_queue_uristr(spider, t))
		{
			log_printf(LOG_CRIT, MSG_C_DB_CONNECT " <%s>\n", t);
			r = -1;
		}
		crawl_free(NULL, t);
	}
	if(r)
	{
		spider->api->release(spider);
		return r;
	}
	/* Launch the thread and wait for the signal from it that it's been
	 * created.
	 */
	pthread_mutex_lock(&createlock);
	log_printf(LOG_DEBUG, "thread #%d is being created; waiting for it to start\n", offset + 1);
	pthread_create(&thread, NULL, thread_handler_, (void *) spider);
	pthread_cond_wait(&createcond, &createlock);
	log_printf(LOG_DEBUG, "thread #%d has been started\n", offset + 1);
	pthread_mutex_unlock(&createlock);
	return 0;
}

/* Launch all of the threads required by this instance */
int
thread_create_all(void)
{
	int c, nthreads;

	nthreads = crawl_cluster_inst_threads();
	log_printf(LOG_DEBUG, "creating %d crawler thread%c\n", nthreads, nthreads == 1 ? ' ' : 's');
	spiders = (SPIDER **) crawl_alloc(NULL, nthreads * sizeof(SPIDER *));
	if(!spiders)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_NOMEM ": failed to allocate memory for thread contexts\n");
		return -1;
	}

	for(c = 0; c < nthreads; c++)
	{
		log_printf(LOG_DEBUG, "launching thread #%d\n", c + 1);
		if(thread_create(c))
		{			
			log_printf(LOG_CRIT, MSG_C_CRAWL_THREADCREATE " (thread #%d)\n", c + 1);
			return -1;
		}
	}
	return 0;
}

/* Sleep, waiting for either a 'terminate' flag to be set, or
 * for all of the crawl threads to exit.
 */
int
thread_wait_all(void)
{
	size_t c;

	log_printf(LOG_DEBUG, "main thread is now sleeping and waiting for termination\n");
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
	thread_terminate_all();
	return 0;
}

/* Ask all crawl threads to terminate, and then wait for them to
 * do so.
 */
int
thread_terminate_all(void)
{
	int nthreads, c, alive;

	nthreads = crawl_cluster_inst_threads();
	log_printf(LOG_NOTICE, MSG_N_CRAWL_TERMINATETHREADS "\n");   
	pthread_mutex_lock(&lock);
	crawld_terminate = 1;
	for(c = 0; c < nthreads; c++)
	{
		if(spiders[c])
		{
			spiders[c]->api->terminate(spiders[c]);
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
			if(spiders[c])
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

static void
thread_init_(void)
{
	log_printf(LOG_DEBUG, "[thread] global init\n");
	/* Obtain global options which will be applied to each thread */
	pthread_mutex_init(&lock, NULL);
	atexit(thread_cleanup_);
	cache = config_geta("cache:uri", NULL);
	username = config_geta("cache:username", NULL);
	password = config_geta("cache:password", NULL);
	endpoint = config_geta("cache:endpoint", NULL);
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&createcond, NULL);
	pthread_mutex_init(&createlock, NULL);
}

/* Global thread-related clean-up performed at process exit */
static void
thread_cleanup_(void)
{
	log_printf(LOG_DEBUG, "[thread] global cleanup\n");
	pthread_mutex_destroy(&lock);
	pthread_mutex_destroy(&createlock);
	pthread_cond_destroy(&createcond);
	crawl_free(NULL, cache);
	crawl_free(NULL, username);
	crawl_free(NULL, password);
	crawl_free(NULL, endpoint);
}

/* The body of a single crawl thread */
static void *
thread_handler_(void *arg)
{
	SPIDER *spider;
	CLUSTER *cluster;
	CRAWL *crawler;
	int threadid;
	const char *env;
	int r;

	/* no addref() of the spider because this thread is given ownership of the
	 * object.
	 */
	spider = (SPIDER *) arg;
	cluster = crawl_cluster();
	env = cluster_env(cluster);
	spider->api->set_cluster(spider, cluster);
	crawler = spider->api->crawler(spider);
	/* XXX this should really be handled by libspider */
	crawl_set_prefetch(crawler, thread_prefetch_);
	threadid = spider->api->local_index(spider);
	pthread_mutex_lock(&lock);
	activethreads++;	
	spiders[threadid] = spider;
	pthread_mutex_unlock(&lock);
	spider->api->attach(spider);
	
	log_printf(LOG_NOTICE, MSG_N_CRAWL_READY " [%s] crawler thread #%d\n", env, threadid + 1);
	/*
	  log_printf(LOG_NOTICE, MSG_N_CRAWL_READY " [%s] crawler %d/%d (thread %d/%d)\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount); */
	pthread_mutex_lock(&createlock);
	pthread_cond_signal(&createcond);
	pthread_mutex_unlock(&createlock);
	while(!spider->api->terminated(spider))
	{
#if 0
		newbase = crawl_cluster_inst_id();
		newthreads = crawl_cluster_threads();
		if(newbase != instid || newthreads != crawlercount)
		{
			/* Cluster has re-balanced */
			spider->api->set_threads(spider, newthreads);
			spider->api->set_base(spider, newbase);

			crawlercount = newthreads;
			instid = newbase;
		}
#endif
		/* Fetch and process a single item from the queue */
		r = spider->api->perform(spider);
		if(r == SPIDER_PERFORM_ERROR)
		{
			log_printf(LOG_CRIT, MSG_C_CRAWL_FAILED ": %s\n", strerror(errno));
			break;
		}
		if(spider->api->oneshot(spider) || spider->api->terminated(spider))
		{
			log_printf(LOG_DEBUG, "thread #%d: either one-shot or already terminated, shutting down\n", threadid + 1);
			break;
		}
		if(r == SPIDER_PERFORM_SUSPENDED || r == SPIDER_PERFORM_AGAIN)
		{
			/* Nothing was available to be dequeued */
			sleep(SUSPEND_WAIT);
		}
		else
		{
			/* A normal fetch occurred */
			sleep(1);
		}
	}
	log_printf(LOG_NOTICE, MSG_N_CRAWL_TERMINATING " [%s] crawler thread #%d\n", env, threadid + 1);
	/* 	log_printf(LOG_NOTICE, MSG_N_CRAWL_TERMINATING " [%s] crawler %d/%d (thread %d/%d)\n", env, instid + threadid + 1, crawlercount, threadid + 1, threadcount); */
	spider->api->terminate(spider);
	spider->api->detach(spider);
	/* Remove the the thread from our list of running crawlers */
	pthread_mutex_lock(&lock);
	spiders[threadid] = NULL;
	activethreads--;
	spider->api->release(spider);
	pthread_mutex_unlock(&lock);
	return NULL;
}

static int
thread_prefetch_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	(void) crawl;
	(void) uri;
	(void) userdata;
	
	/* Don't attempt to fetch if the global 'terminate' flag has been set */
	if(crawld_terminate)
	{
		return -1;
	}
	log_printf(LOG_DEBUG, "Fetching %s\n", uristr);
	return 0;
}
