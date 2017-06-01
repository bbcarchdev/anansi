/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015-2017 BBC
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

#include "p_libspider.h"

/* A SPIDER context wraps a CRAWL instance along with the data needed to
 * run a crawler thread with the configured queue, cache and policy
 * handler.
 */

static unsigned long spider_addref_(SPIDER *me);
static unsigned long spider_release_(SPIDER *me);
static int spider_set_userdata_(SPIDER *me, void *data);
static void *spider_userdata_(SPIDER *me);
static int spider_set_local_index_(SPIDER *me, int index);
static int spider_set_base_(SPIDER *me, int base);
static int spider_set_threads_(SPIDER *me, int threads);
static int spider_crawler_id_(SPIDER *me);
static int spider_local_index_(SPIDER *me);
static int spider_threads_(SPIDER *me);
static CRAWL *spider_crawler_(SPIDER *me);
static int spider_set_queue_(SPIDER *me, QUEUE *queue);
static int spider_set_queue_uristr_(SPIDER *me, const char *uri);
static int spider_set_queue_uri_(SPIDER *me, URI *uri);
static QUEUE *spider_queue_(SPIDER *me);
static int spider_set_processor_(SPIDER *me, PROCESSOR *processor);
static int spider_set_processor_name_(SPIDER *me, const char *name);
static PROCESSOR *spider_processor_(SPIDER *me);
static char *spider_config_geta_(SPIDER *me, const char *key, const char *defval);
static int spider_config_get_int_(SPIDER *me, const char *key, int defval);
static int spider_config_get_bool_(SPIDER *me, const char *key, int defval);
static int spider_terminate_(SPIDER *me);
static int spider_terminated_(SPIDER *me);
static int spider_set_oneshot_(SPIDER *me);
static int spider_oneshot_(SPIDER *me);
static int spider_set_cluster_(SPIDER *me, CLUSTER *cluster);
static CLUSTER *spider_cluster_(SPIDER *me);
static int spider_log_(SPIDER *me, int level, const char *format, ...);
static int spider_vlog_(SPIDER *me, int level, const char *format, va_list ap);
static int spider_perform_(SPIDER *me);
static int spider_attach_(SPIDER *me);
static int spider_detach_(SPIDER *me);
static int spider_attached_(SPIDER *me);
static int spider_add_policy_(SPIDER *me, SPIDERPOLICY *policy);
static int spider_add_policy_name_(SPIDER *me, const char *name);

static int spider_sync_(SPIDER *me);

static struct spider_api_struct spider_api = {
	NULL,
	spider_addref_,
	spider_release_,
	spider_set_userdata_,
	spider_userdata_,
	spider_set_local_index_,
	spider_set_base_,
	spider_set_threads_,
	spider_crawler_id_,
	spider_local_index_,
	spider_threads_,
	spider_crawler_,
	spider_set_queue_,
	spider_set_queue_uristr_,
	spider_set_queue_uri_,
	spider_queue_,
	spider_set_processor_,
	spider_set_processor_name_,
	spider_processor_,
	spider_config_geta_,
	spider_config_get_int_,
	spider_config_get_bool_,
	spider_terminate_,
	spider_terminated_,
	spider_set_oneshot_,
	spider_oneshot_,
	spider_set_cluster_,
	spider_cluster_,
	spider_log_,
	spider_vlog_,
	spider_perform_,
	spider_attach_,
	spider_detach_,
	spider_attached_,
	spider_add_policy_,
	spider_add_policy_name_
};

SPIDER *
spider_create(const SPIDERCALLBACKS *callbacks)
{
	SPIDER *p;
	int e;
	CRAWL *crawl;

	crawl = crawl_create();
	if(!crawl)
	{
/* XXX		log_printf(LOG_CRIT, MSG_C_CRAWL_INSTANCE "\n"); */
		return NULL;
	}	
	e = 0;
	p = (SPIDER *) crawl_alloc(crawl, sizeof(SPIDER));
	if(!p)
	{
		crawl_destroy(crawl);
		return NULL;
	}
	if(callbacks)
	{
		switch(callbacks->version)
		{
		case 1:
			memcpy(&(p->cb), callbacks, sizeof(struct spider_callbacks_v1_struct));
			break;
		case 2:
			memcpy(&(p->cb), callbacks, sizeof(struct spider_callbacks_v2_struct));
			break;
		default:
			crawl_free(crawl, p);
			crawl_destroy(crawl);
			errno = EINVAL;
			return NULL;
		}
	}
	pthread_rwlock_init(&(p->lock), NULL);
	p->api = &spider_api;
	p->refcount = 1;
	p->local_index = 0;
	p->total_threads = 0;
	p->crawl = crawl;
	crawl_set_userdata(p->crawl, p);
	/* XXX */
	if(p->cb.logger)
	{
		crawl_set_logger(p->crawl, p->cb.logger);
	}
	/* XXX MOVE TO SPIDER::set_verbose() */
	crawl_set_verbose(p->crawl, 0);
/*	crawl_set_verbose(p->crawl, config_get_bool("crawler:verbose", 0)); */
	return p;
}

static unsigned long
spider_addref_(SPIDER *me)
{
	unsigned long r;

	pthread_rwlock_wrlock(&(me->lock));
	me->refcount++;
	r = me->refcount;
	pthread_rwlock_unlock(&(me->lock));
	return me->refcount;
}

static unsigned long
spider_release_(SPIDER *me)
{
	unsigned long r;
	CRAWL *crawl;
	size_t c;

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
	for(c = 0; c < SPIDER_MAX_POLICIES; c++)
	{
		if(me->policies[c])
		{
			me->policies[c]->api->release(me->policies[c]);
		}
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

/* Set and obtain the application-specific data pointer */
static int
spider_set_userdata_(SPIDER *me, void *data)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->userdata = data;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static void *
spider_userdata_(SPIDER *me)
{
	void *p;

	pthread_rwlock_rdlock(&(me->lock));
	p = me->userdata;
	pthread_rwlock_unlock(&(me->lock));
	return p;
}

static int
spider_set_local_index_(SPIDER *me, int index)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->local_index = index;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

/* Set the base thread ID for this instance */
static int
spider_set_base_(SPIDER *me, int base)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->base = base;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

/* Set the total number of threads in the cluster - ignored if
 * associated with a CLUSTER object
 */
static int
spider_set_threads_(SPIDER *me, int threads)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->total_threads = threads;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

/* Return the cluster-global crawler ID for this context */
static int
spider_crawler_id_(SPIDER *me)
{
	int r;
	CLUSTERSTATE state;

	/* If we're not attached to a thread, sync with the
	 * cluster immediately to obtain this. If we are
	 * attached, we use the values from the most recent
	 * call to SPIDER::perform() - this ensures that
	 * the values don't change throughout the duration of
	 * a single dequeue-fetch-update pass.
	 */
	pthread_rwlock_rdlock(&(me->lock));
	if(!me->attached && me->cluster)
	{
		if(cluster_state(me->cluster, &state))
		{
			me->api->log(me, LOG_CRIT, MSG_C_CRAWL_CLUSTERSTATE);
			r = -1;
		}
		me->base = state.index;
		me->total_threads = state.total;		
	}
	/* If the local index or our base are -1, it means
	 * this thread is not currently participating in the
	 * cluster.
	 */
	if(me->local_index == -1)
	{
		r = -1;
	}
	else if(me->base == -1)
	{
		r = -1;
	}
	else
	{
		r = me->base + me->local_index;
	}
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

/* Return the local thread ID (where 0 is the first thread, 1 is the second) */
static int
spider_local_index_(SPIDER *me)
{
	int r;

	pthread_rwlock_rdlock(&(me->lock));
	r = me->local_index;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

/* Return the total number of crawl threads across the cluster */
static int
spider_threads_(SPIDER *me)
{
	int r;
	CLUSTERSTATE state;

	/* If we're not attached to a thread, sync with the
	 * cluster immediately to obtain this. If we are
	 * attached, we use the values from the most recent
	 * call to SPIDER::perform() - this ensures that
	 * the values don't change throughout the duration of
	 * a single dequeue-fetch-update pass.
	 */
	pthread_rwlock_rdlock(&(me->lock));
	if(!me->attached && me->cluster)
	{
		if(cluster_state(me->cluster, &state))
		{
			me->api->log(me, LOG_CRIT, MSG_C_CRAWL_CLUSTERSTATE);
			r = -1;
		}
		me->base = state.index;
		me->total_threads = state.total;		
	}
	r = me->total_threads;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

static CRAWL *
spider_crawler_(SPIDER *me)
{
	CRAWL *p;

	pthread_rwlock_rdlock(&(me->lock));
	/* If the underlying CRAWL object was refcounted, we'd retain
	 * here before returning outside of the lock
	 */
	p = me->crawl;
	pthread_rwlock_unlock(&(me->lock));
	return p;
}

static int
spider_set_queue_(SPIDER *me, QUEUE *queue)
{
	pthread_rwlock_wrlock(&(me->lock));
	if(queue)
	{
		queue->api->addref(queue);
	}
	if(me->queue)
	{
		me->queue->api->release(me->queue);
	}
	me->queue = queue;
	spider_queue_attach_(me, queue);
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
spider_set_queue_uristr_(SPIDER *me, const char *uristring)
{
	URI *uri;
	int r;

	uri = uri_create_str(uristring, NULL);
	if(!uri)
	{
		me->api->log(me, LOG_CRIT, "failed to parse queue URI <%s>\n", uristring);
		return -1;
	}
	r = spider_set_queue_uri_(me, uri);
	uri_destroy(uri);
	return r;
}

static int
spider_set_queue_uri_(SPIDER *me, URI *uri)
{
	QUEUE *p;
	
	p = spider_queue_create_uri(me, uri);
	if(!p)
	{
		return -1;
	}
	/* If the creation was sucessful, it will have
	 * been attached to and retained by this
	 * spider instance, so we can discard this
	 * pointer
	 */
	p->api->release(p);
	return 0;
}

static QUEUE *
spider_queue_(SPIDER *me)
{
	QUEUE *p;

	pthread_rwlock_rdlock(&(me->lock));
	p = me->queue;
	if(p)
	{
		p->api->addref(p);
	}
	pthread_rwlock_unlock(&(me->lock));
	return p;
}

static int
spider_set_processor_(SPIDER *me, PROCESSOR *processor)
{
	
	pthread_rwlock_wrlock(&(me->lock));
	if(processor)
	{
		processor->api->addref(processor);
	}
	if(me->processor)
	{
		me->processor->api->release(me->processor);
	}
	spider_processor_attach_(me, processor);
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
spider_set_processor_name_(SPIDER *me, const char *name)
{
	PROCESSOR *p;

	p = spider_processor_create_name(me, name);
	if(!p)
	{
		return -1;
	}
	/* If successful, the processor will have already been
	 * attached and retained by this instance */
	p->api->release(p);
	return 0;
}

static PROCESSOR *
spider_processor_(SPIDER *me)
{
	PROCESSOR *p;
	
	pthread_rwlock_rdlock(&(me->lock));
	p = me->processor;
	if(p)
	{
		p->api->addref(p);
	}
	pthread_rwlock_unlock(&(me->lock));
	return p;
}

static char *
spider_config_geta_(SPIDER *me, const char *key, const char *defval)
{
	char *p;

	pthread_rwlock_rdlock(&(me->lock));
	if(me->cb.config_geta)
	{
		p = me->cb.config_geta(key, defval);
	}
	else if(defval)
	{
		p = strdup(defval);
	}
	else
	{
		p = NULL;
	}
	pthread_rwlock_unlock(&(me->lock));
	return p;
}

static int
spider_config_get_int_(SPIDER *me, const char *key, int defval)
{
	int r;

	pthread_rwlock_rdlock(&(me->lock));
	if(me->cb.config_get_int)
	{
		r = me->cb.config_get_int(key, defval);
	}
	else
	{
		r = defval;
	}
	pthread_rwlock_unlock(&(me->lock));
	return r;
}


static int
spider_config_get_bool_(SPIDER *me, const char *key, int defval)
{
	int r;

	pthread_rwlock_rdlock(&(me->lock));
	if(me->cb.config_get_bool)
	{
		r = me->cb.config_get_bool(key, defval);
	}
	else
	{
		r = (defval ? 1 : 0);
	}
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

/* Terminate a context: may be invoked by a different thread */
static int
spider_terminate_(SPIDER *me)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->terminated = 1;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
spider_terminated_(SPIDER *me)
{
	int r;

	pthread_rwlock_rdlock(&(me->lock));
	r =  me->terminated;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

static int
spider_oneshot_(SPIDER *me)
{
	int r;
	
	pthread_rwlock_rdlock(&(me->lock));
	r = me->oneshot;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

static int
spider_set_oneshot_(SPIDER *me)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->oneshot = 1;
	pthread_rwlock_unlock(&(me->lock));
	return 1;
}

static int
spider_set_cluster_(SPIDER *me, CLUSTER *cluster)
{
	pthread_rwlock_wrlock(&(me->lock));
	me->cluster = cluster;
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static CLUSTER *
spider_cluster_(SPIDER *me)
{
	CLUSTER *p;
	
	pthread_rwlock_rdlock(&(me->lock));
	p = me->cluster;
	/* If cluster objects were refcounted, we would addref here */
	pthread_rwlock_unlock(&(me->lock));
	return p;
}

static int
spider_log_(SPIDER *me, int level, const char *format, ...)
{
	va_list ap;

	pthread_rwlock_rdlock(&(me->lock));
	if(me->cb.logger)
	{
		va_start(ap, format);
		me->cb.logger(level, format, ap);
		va_end(ap);
	}
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
spider_vlog_(SPIDER *me, int level, const char *format, va_list ap)
{
	pthread_rwlock_rdlock(&(me->lock));
	if(me->cb.logger)
	{
		me->cb.logger(level, format, ap);
	}
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

/* Because we don't use recursive mutexes (they're not portable), the
 * call to crawl_perform() must be invoked while our lock is not held
 * by this thread, otherwise callbacks wouldn't be able to invoke methods
 * on this instance without blocking.
 *
 * For this reason, SPIDER::perform() cannot be invoked without the
 * instance being attached (via SPIDER::attach()). Attachment causes an
 * additional reference to the spider instance to be retained,
 * guaranteeing that this instance won't be destroyed before crawl_perform()
 * returns, even though we don't have a lock held.
 *
 * See the SPIDER_PERFORM_xxx constants for return values.
 */
static int
spider_perform_(SPIDER *me)
{
	int r;

	r = spider_sync_(me);
	if(r >= 0)
	{
		r = crawl_perform(me->crawl);
	}
	return r;
}

static int
spider_sync_(SPIDER *me)
{
	int r, suspended, rebalanced;
	CLUSTERSTATE state;

	/* Sync our view of the cluster state */
	r = SPIDER_PERFORM_AGAIN;
	suspended = 0;
	rebalanced = 0;
	pthread_rwlock_rdlock(&(me->lock));
	if(!me->attached)
	{
		me->api->log(me, LOG_CRIT, MSG_C_CRAWL_NOTATTACHED);
		r = SPIDER_PERFORM_ERROR;
	}
	else if(me->cluster)
	{
		if(cluster_state(me->cluster, &state))
		{
			/* Failed to obtain cluster state */
			me->api->log(me, LOG_CRIT, MSG_C_CRAWL_CLUSTERSTATE);
			r = SPIDER_PERFORM_ERROR;
		}
		else
		{
			/* Compare the cluster state with our stored values */
			if((me->base != state.index) ||
			   (me->total_threads != state.total) ||
			   (me->local_index != me->prev_local_index))
			{
				/* The cluster has changed size/shape */
				rebalanced = 1;
				me->base = state.index;
				me->total_threads = state.total;
				me->prev_local_index = me->local_index;
			}
		}
	}
	if(me->base == -1 || me->total_threads <= 0 || me->local_index == -1 )
	{
		suspended = 1;
	}
	pthread_rwlock_unlock(&(me->lock));		
	if(suspended)
	{
		if(!me->suspended)
		{
			/* Not previously suspended, emit a log message */
			me->api->log(me, LOG_NOTICE, MSG_N_CRAWL_SUSPENDED " [%s] thread #%d/%d\n", cluster_env(me->cluster), me->prev_local_index);
		}
		me->suspended = 1;
		r = SPIDER_PERFORM_SUSPENDED;
	}
	else
	{
		if(me->suspended)
		{
			/* Was suspended, are no longer */
			me->api->log(me, LOG_NOTICE, MSG_N_CRAWL_RESUMING " [%s] thread #%d (%d/%d)\n", cluster_env(me->cluster), me->local_index +1, me->local_index + me->base + 1, me->total_threads);
		}
		else if(rebalanced)
		{
			me->api->log(me, LOG_NOTICE, MSG_I_CRAWL_THREADBALANCED " [%s] thread #%d (%d/%d)\n", cluster_env(me->cluster), me->local_index + 1, me->local_index + me->base + 1, me->total_threads);
		}
		me->suspended = 0;
	}
	return r;
}

/* Attach the SPIDER to a thread: this indicates that the thread will be
 * used to actually crawl, and so certain aspects of state (such as balancing
 * of the cluster) are inhibited until each pass of SPIDER::perform().
 *
 * Calls to SPIDER::attach() must be matched by corresponding calls to
 * SPIDER::detach().
 *
 * Results are undefined if the attach(), perform() and detach() trio crosses
 * thread boundaries: you must ensure the calls are properly serialised
 * if you attempt to do this, and it remains not recommended in either case.
 *
 * A typical thread body is intended to be along the following lines:
 *
 * static void crawlthread(void *arg)
 * {
 *   SPIDER *spider = (SPIDER *) arg;
 *   int r, oneshot;
 *
 *   spider->api->attach(spider);
 *   oneshot = spider->api->oneshot(spider);
 *   while(!spider->api->terminated(spider))
 *   {
 *     r = spider->api->perform(spider);
 *     // Hard error or one-shot mode
 *     if(r < 0 || oneshot)
 *     {
 *        break;
 *     }
 *     // Something was dequeued
 *     if(r)
 *     {
 *       continue;
 *     }
 *     // The queue was empty
 *     sleep(1);
 *   }
 *   spider->api->detach(spider);
 *   return NULL;
 * }
 */
static int
spider_attach_(SPIDER *me)
{
	me->api->addref(me);
	pthread_rwlock_wrlock(&(me->lock));
	if(!me->attached)
	{
		me->attached = 1;
		me->suspended = 0;
	}
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
spider_detach_(SPIDER *me)
{
	pthread_rwlock_wrlock(&(me->lock));
	if(me->attached)
	{
		me->attached = 0;
		me->suspended = 0;
	}
	pthread_rwlock_unlock(&(me->lock));
	me->api->release(me);
	return 0;
}

static int
spider_attached_(SPIDER *me)
{
	int r;

	pthread_rwlock_rdlock(&(me->lock));
	r = me->attached;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

/* Append the supplied policy to the list of policies
 * that will be applied in subsequent fetches; this is
 * a no-op if the policy has already been added.
 */
static int
spider_add_policy_(SPIDER *me, SPIDERPOLICY *policy)
{
	size_t c;

	policy->api->addref(policy);
	pthread_rwlock_wrlock(&(me->lock));
	if(me->npolicies >= SPIDER_MAX_POLICIES)
	{
		pthread_rwlock_unlock(&(me->lock));
		policy->api->release(policy);
		errno = ENOSPC;
		return -1;
	}
	for(c = 0; c < me->npolicies; c++)
	{
		if(me->policies[c] == policy)
		{
			pthread_rwlock_unlock(&(me->lock));
			policy->api->release(policy);
			return 0;
		}
	}
	me->policies[me->npolicies] = policy;
	me->npolicies++;
	spider_policy_attach_(me, policy);
	pthread_rwlock_unlock(&(me->lock));
	return 0;
}

static int
spider_add_policy_name_(SPIDER *me, const char *name)
{
	SPIDERPOLICY *policy;

	policy = spider_policy_create_name(me, name);
	if(!policy)
	{
		return -1;
	}
	/* If the policy was successfully created, it would have been
	 * automatically attached to and retained by this instance,
	 * so we can simply release the returned object.
	 */
	policy->api->release(policy);
	return 0;
}
