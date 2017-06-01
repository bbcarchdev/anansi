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

#include "p_libspider.h"

#ifdef WITH_LIBSQL
# include <libsql.h>
#endif

/* Implement a libcrawl queue handler which interfaces with crawld queue
 * modules.
 */

static int queue_handler_(CRAWL *crawl, URI **next, CRAWLSTATE *state, void *userdata);

/* Create a new queue instance, associated with a spider, for the supplied URI */
QUEUE *
spider_queue_create_uri(SPIDER *spider, URI *uri)
{
	URI_INFO *info;
	QUEUE *p;

	info = uri_info(uri);
	if(!info)
	{
		errno = ENOENT;
		return NULL;
	}
	p = NULL;
	if(!info->scheme || !info->scheme[0])
	{
		/* No scheme at all */
	}
#if WITH_LIBSQL
	else if(sql_scheme_exists(info->scheme))
	{
		/* libsql URI */
		p = spider_queue_db_create_(spider, uri);
	}
#endif
	else
	{
		/* Unknown scheme */
		errno = ENOENT;
		spider->api->log(spider, LOG_CRIT, MSG_C_CRAWL_QUEUEUNKNOWN ": '%s'\n", info->scheme);
		return NULL;
	}
	uri_info_destroy(info);
	if(p)
	{
		if(spider_queue_attach_(spider, p))
		{
			p->api->release(p);
			return NULL;
		}
	}
	return p;
}

/* INTERNAL: Attach a queue to a context
 * IMPORTANT: Assumes the SPIDER write-lock is held by this thread
 */
int
spider_queue_attach_(SPIDER *spider, QUEUE *queue)
{
	/* Don't use SPIDER::set_queue() because it will invoke
	 * this function and infinte recursion will occur
	 */
	if(queue)
	{
		queue->api->addref(queue);
	}
	if(spider->queue)
	{
		spider->queue->api->release(spider->queue);
	}
	spider->queue = queue;   
	/* Note that the CRAWL object's userdata pointer will
	 * already point to the spider
	 */
	if(queue)
	{
		return crawl_set_next(spider->api->crawler(spider), queue_handler_);
	}
	return 0;
}

#if 0
/* Global initialisation */
int
queue_init(void)
{
	const char *name;

	/* queue_init() is invoked before any other threads are created, so
	 * it's safe to use config_getptr_unlocked().
	 */
	name = config_getptr_unlocked("queue:name", NULL);
	if(!name)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_QUEUECONFIG "\n");
		return -1;
	}
	if(!strcmp(name, "db"))
	{
		constructor = db_create;
	}
	else
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_QUEUEUNKNOWN ": '%s'\n", name);
		return -1;
	}
	return 0;
}

/* Global cleanup */
int
queue_cleanup(void)
{
	return 0;
}

/* Create a queue instance and attach it to the context */
int
queue_init_context(CONTEXT *context)
{
	CRAWL *crawl;

	crawl = context->crawl;
	context->queue = constructor(context);
	if(!context->queue)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_QUEUEINIT "\n");
		return -1;
	}
	crawl_set_next(crawl, queue_handler_);
	return 0;
}
#endif

/* Add a URI to the crawl queue */
int
queue_add_uristr(CRAWL *crawl, const char *uristr)
{
	SPIDER *spider;
	URI *uri;
	CRAWLSTATE state;
	int r;

	spider = (SPIDER *) crawl_userdata(crawl);
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		spider->api->log(spider, LOG_ERR, MSG_E_CRAWL_URIPARSE " <%s>\n", uristr);
		return -1;
	}
	/* XXX spider->api->apply_policy_uri(spider, uri, uristr); */
/*	state = spider_policy_uri_(crawl, uri, uristr, (void *) spider); */
	state = COS_ACCEPTED;
	if(state == COS_ACCEPTED)
	{
		spider->api->log(spider, LOG_DEBUG, "Adding URI <%s> to crawler queue\n", uristr);
		r = spider->queue->api->add(spider->queue, uri, uristr);
	}
	else if(state == COS_ERR)
	{
		r = -1;
	}
	else
	{
		r = 0;
	}
	uri_destroy(uri);
	return r;
}

/* Add a URI to the crawl queue */
int
queue_add_uri(CRAWL *crawl, URI *uri)
{
	SPIDER *spider;
	char *uristr;
	CRAWLSTATE state;
	int r;
		
	spider = (SPIDER *) crawl_userdata(crawl);
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		spider->api->log(spider, LOG_CRIT, MSG_C_CRAWL_URIUNPARSE "\n");
		return -1;
	}
	/* XXX spider->api->apply_policy_uri(spider, uri, uristr); */
	/* state = policy_uri_(crawl, uri, uristr, (void *) data); */
	state = COS_ACCEPTED;
	if(state == COS_ACCEPTED)
	{
		spider->api->log(spider, LOG_DEBUG, "Adding URI <%s> to crawler queue\n", uristr);
		r = spider->queue->api->add(spider->queue, uri, uristr);
	}
	else if(state == COS_ERR)
	{
		r = -1;
	}
	else
	{
		r = 0;
	}
	crawl_free(crawl, uristr);
	return r;	
}

/* Mark a URI as having been updated */
int
queue_updated_uristr(CRAWL *crawl, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->updated_uristr(data->queue, uristr, updated, last_modified, status, ttl, state);
}

/* Mark a URI as having been updated */
int
queue_updated_uri(CRAWL *crawl, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->updated_uri(data->queue, uri, updated, last_modified, status, ttl, state);
}

/* Mark a URI as unchanged */
int
queue_unchanged_uristr(CRAWL *crawl, const char *uristr, int error)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->unchanged_uristr(data->queue, uristr, error);
}

/* Mark a URI as unchanged */
int
queue_unchanged_uri(CRAWL *crawl, URI *uri, int error)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->unchanged_uri(data->queue, uri, error);
}

/* Implement the libcrawl queue handler; userdata points to our SPIDER */
static int
queue_handler_(CRAWL *crawl, URI **next, CRAWLSTATE *state, void *userdata)
{
	SPIDER *spider;
	QUEUE *queue;
	int r;

	(void) crawl;
	
	spider = (SPIDER *) userdata;

	/* If the spider has already been terminated, return immediately */
	if(spider->api->terminated(spider))
	{
		return 0;
	}
	/* Also bail out if the spider doesn't have a queue to fetch from */
	queue = spider->api->queue(spider);
	if(!queue)
	{
		return 0;
	}
	r = queue->api->next(queue, next, state);
	/* SPIDER::queue() will have invoked QUEUE::addref() before returning it
	 * to us, so we must release it here before returning
	 */
	queue->api->release(queue);
	return r;
}
