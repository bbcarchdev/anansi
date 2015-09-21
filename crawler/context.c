/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015 BBC
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

#include "p_libcrawld.h"

/* A crawl context wraps a CRAWL instance along with the data needed to
 * run a crawler thread with the configured queue, cache and policy
 * handler.
 */

static unsigned long context_addref(CONTEXT *me);
static unsigned long context_release(CONTEXT *me);
static int context_crawler_id(CONTEXT *me);
static int context_cache_id(CONTEXT *me);
static int context_thread_id(CONTEXT *me);
static int context_threads(CONTEXT *me);
static int context_caches(CONTEXT *me);
static CRAWL *context_crawler(CONTEXT *me);
static QUEUE *context_queue(CONTEXT *me);
static PROCESSOR *context_processor(CONTEXT *me);
static const char *context_config_get(CONTEXT *me, const char *key, const char *defval);
static int context_config_get_int(CONTEXT *me, const char *key, int defval);
static int context_set_base(CONTEXT *me, int base);
static int context_set_threads(CONTEXT *me, int threads);
static int context_terminate(CONTEXT *me);
static int context_terminated(CONTEXT *me);
static int context_oneshot(CONTEXT *me);
static int context_set_oneshot(CONTEXT *me);

static struct context_api_struct context_api = {
	NULL,
	context_addref,
	context_release,
	context_crawler_id,
	context_cache_id,
	context_thread_id,
	context_threads,
	context_caches,
	context_crawler,
	context_queue,
	context_processor,
	context_config_get,
	context_config_get_int,
	context_set_base,
	context_set_threads,
	context_terminate,
	context_terminated,
	context_oneshot,
	context_set_oneshot,
};

CONTEXT *
context_create(int crawler_offset)
{
	CONTEXT *p;
	int e;
	CRAWL *crawl;

	crawl = crawl_create();
	if(!crawl)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_INSTANCE "\n");
		return NULL;
	}	
	e = 0;
	p = (CONTEXT *) crawl_alloc(crawl, sizeof(CONTEXT));
	pthread_rwlock_init(&(p->lock), NULL);
	p->api = &context_api;
	p->refcount = 1;
	p->thread_id = crawler_offset;
	p->crawler_id = p->thread_offset + p->thread_base;
	p->cache_id = p->crawler_id;
	p->crawl = crawl;
	crawl_set_logger(p->crawl, log_vprintf);
	crawl_set_userdata(p->crawl, p);
	crawl_set_verbose(p->crawl, config_get_bool("crawler:verbose", 0));
	return p;
}

static unsigned long
context_addref(CONTEXT *me)
{
	unsigned long r;

	pthread_rwlock_wrlock(&(me->lock));
	me->refcount++;
	r = me->refcount;
	pthread_rwlock_unlock(&(me->lock));
	return me->refcount;
}

static unsigned long
context_release(CONTEXT *me)
{
	unsigned long r;
	CRAWL *crawl;

	crawl = NULL;
	pthread_rwlock_wrlock(&(me->lock));
	me->refcount--;
	r = me->refcount;
	pthread_rwlock_unlock(&(me->lock));
	if(r)
	{
		return r;
	}
	if(me->processor)
	{
		me->processor->api->release(me->processor);
	}
	if(me->queue)
	{
		me->queue->api->release(me->queue);
	}
	if(me->crawl)
	{
		crawl = me->crawl;
		me->crawl = NULL;
	}
	pthread_rwlock_destroy(&(me->lock));
	crawl_free(crawl, me->cfgbuf);
	crawl_free(crawl, me);
	if(crawl)
	{
		crawl_destroy(crawl);
	}
	return 0;
}

/* Return the cluster-global crawler ID for this context */
static int
context_crawler_id(CONTEXT *me)
{
	return me->crawler_id;
}

/* Return the cluster-global cache ID for this context */
static int
context_cache_id(CONTEXT *me)
{
	return me->cache_id;
}

/* Return the local thread ID (where 0 is the first thread, 1 is the second) */
static int
context_thread_id(CONTEXT *me)
{
	return me->thread_offset;
}

/* Return the total number of crawl threads across the cluster */
static int
context_threads(CONTEXT *me)
{
	return me->thread_count;
}

/* Return the total number of cache buckets across the cluster */
static int
context_caches(CONTEXT *me)
{
	return me->thread_count;
}

static CRAWL *
context_crawler(CONTEXT *me)
{
	return me->crawl;
}

static QUEUE *
context_queue(CONTEXT *me)
{
	return me->queue;
}

static PROCESSOR *
context_processor(CONTEXT *me)
{
	return me->processor;
}

static const char *
context_config_get(CONTEXT *me, const char *key, const char *defval)
{
	size_t needed;
	char *p;
	
	(void) me;
	
	needed = config_get(key, defval, me->cfgbuf, me->cfgbuflen);
	if(needed)
	if(needed > me->cfgbuflen)
	{
		p = (char *) crawl_realloc(me->crawl, me->cfgbuf, needed);
		if(!p)
		{
			return NULL;
		}
		me->cfgbuf = p;
		me->cfgbuflen = needed;
		if(config_get(key, defval, me->cfgbuf, me->cfgbuflen) != needed)
		{
			return NULL;
		}		
	}
	return me->cfgbuf;
}

static int
context_config_get_int(CONTEXT *me, const char *key, int defval)
{
	(void) me;
	
	return config_get_int(key, defval);
}

/* Set the base thread ID for this instance */
static int
context_set_base(CONTEXT *me, int base)
{
	me->thread_base = base;
	me->crawler_id = me->thread_offset + me->thread_base;
	me->cache_id = me->crawler_id;
	if(me->queue)
	{
		me->queue->api->set_crawler(me->queue, me->crawler_id);
		me->queue->api->set_cache(me->queue, me->cache_id);
	}
	return 0;
}

/* Set the total number of threads in the cluster */
static int
context_set_threads(CONTEXT *me, int threads)
{
	me->thread_count = threads;
	if(me->queue)
	{
		me->queue->api->set_crawlers(me->queue, threads);
		me->queue->api->set_caches(me->queue, threads);
	}
	return 0;
}

/* Terminate a context: may be invoked by a different thread */
static int
context_terminate(CONTEXT *me)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->terminate = 1;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
context_terminated(CONTEXT *me)
{
	int r;

	pthread_rwlock_rdlock(&(me->lock));
	r =  me->terminate;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

static int
context_oneshot(CONTEXT *me)
{
	return me->oneshot;
}

static int
context_set_oneshot(CONTEXT *me)
{
	me->oneshot = 1;
	return 1;
}
