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

/* Implement a libcrawl processor handler which interfaces with the crawld
 * queue modules.
 */

static int processor_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int processor_unchanged_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int processor_failed_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata, CRAWLSTATE state);

static PROCESSOR *(*constructor)(CRAWL *crawler);

/* Create a processor and attach it to the supplied spider instance */
PROCESSOR *
spider_processor_create_name(SPIDER *spider, const char *name)
{
	PROCESSOR *p;

	if(!strcmp(name, "rdf"))
	{
		p = spider_processor_rdf_create_(spider);
	}
	else if(!strcmp(name, "lod"))
	{
		p = spider_processor_lod_create_(spider);
	}
	else
	{
		errno = ENOENT;
		return NULL;
	}
	if(!p)
	{
		return NULL;
	}
	if(spider->api->set_processor(spider, p))
	{
		p->api->release(p);
		return NULL;
	}
	return p;
}

/* INTERNAL: invoked automatically by SPIDER::set_processor()
 * IMPORTANT: the spider instance must be write-locked before invoking this
 * function.
 */
int
spider_processor_attach_(SPIDER *spider, PROCESSOR *processor)
{
	spider->processor = processor;
	crawl_set_updated(spider->crawl, processor_handler_);
	crawl_set_unchanged(spider->crawl, processor_unchanged_handler_);
	crawl_set_failed(spider->crawl, processor_failed_handler_);	
	return 0;
}

#if 0
/* XXX: this should be replaced by spider->api->set_processor(spider, "name"); */

int
processor_init(void)
{
	const char *name;

	/* processor_init() is invoked before any other threads are created, so
	 * it's safe to use config_getptr_unlocked().
	 */
	name = config_getptr_unlocked("processor:name", NULL);
	if(!name)
	{		
		log_printf(LOG_CRIT, MSG_C_CRAWL_PROCESSORCONFIG "\n");
		return -1;
	}
	if(!strcmp(name, "rdf"))
	{
		constructor = rdf_create;
	}
	else if(!strcmp(name, "lod"))
	{
		constructor = lod_create;
	}
	else
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_PROCESSORUNKNOWN ": '%s'\n", name);
		return -1;
	}
	return 0;
}

int
processor_cleanup(void)
{
	return 0;
}
#endif

/* Create a processor instance and attach it to the context */
/* XXX this ought to be moved to the parent app */
int
processor_init_context(SPIDER *context)
{
	CRAWL *crawl;
   	PROCESSOR *processor;

	crawl = context->crawl;
	processor = constructor(crawl);	
	if(!processor)
	{
		context->api->log(context, LOG_CRIT, MSG_C_CRAWL_PROCESSORINIT "\n");
		return -1;
	}
	context->api->set_processor(context, processor);
	processor->api->release(processor);
	return 0;
}

/* processor_handler_() is installed as the 'updated' callback on the CRAWL
 * object by processor_init_context() above. It's invoked when libcrawl
 * determines that an object has been successfully crawled and is either new or
 * has been updated since the last time it was fetched.
 *
 * A CRAWLSTATE isn't passed as a parameter, because the state must be the
 * equivalent of COS_ACCEPTED in order for this callback to be invoked.
 *
 * A CRAWLSTATE isn't returned by this callback, as it's our responsibility
 * to update the queue accordingly.
 *
 * Return zero on success or -1 on failure.
 */
static int
processor_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	SPIDER *me;
	PROCESSOR *processor;
	const char *content_type, *uri, *location;
	int r, status, ttl;
	CRAWLSTATE state;

	(void) prevtime;

	r = 0;
	state = COS_ACCEPTED;
	me = (SPIDER *) userdata;
	processor = me->processor;
	uri = crawl_obj_uristr(obj);
	location = crawl_obj_redirect(obj);
	content_type = crawl_obj_type(obj);
	status = crawl_obj_status(obj);
	me->api->log(me, LOG_DEBUG, "processor_handler: URI is '%s', Content-Type is '%s', status is %d\n", uri, content_type, status);
	/* If there's a redirect, ensure the redirect target will be crawled */
	if(status > 300 && status < 304)
	{
		/* If there's a redirect location (and it's not the same as the source URI,
		 * enqueue the target, skipping further processing
		 */
		if(!location)
		{
			me->api->log(me, LOG_ERR, MSG_E_CRAWL_DEADREDIRECT " from <%s> (HTTP status %d)\n", uri, status);
		}
		else if(strcmp(location, uri))
		{
			me->api->log(me, LOG_DEBUG, "processor_handler: following %d redirect to <%s>\n", status, location);
			queue_add_uristr(crawl, location);
		}
		state = COS_SKIPPED;
	}
	else
	{
		me->api->log(me, LOG_DEBUG, "processor_handler: object has been updated\n");
	}
	if(state == COS_ACCEPTED)
	{
		r = processor->api->process(processor, obj, uri, content_type);
		if(r < 0)
		{
			state = COS_FAILED;
		}
		else if(r > 0)
		{
			state = (CRAWLSTATE) r;		
		}
		else
		{
			state = COS_REJECTED;
		}
	}		
	if(state == COS_ACCEPTED)
	{
		me->api->log(me, LOG_INFO, MSG_I_CRAWL_ACCEPTED " <%s>\n", uri);
		ttl = 86400;
	}
	else
	{
		ttl = 604800;
	}
	queue_updated_uristr(crawl, uri, crawl_obj_updated(obj), crawl_obj_updated(obj), crawl_obj_status(obj), ttl, state);
	return r;
}

static int
processor_unchanged_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	const char *uri;
	SPIDER *me;

	(void) obj;
	(void) prevtime;

	me = (SPIDER *) userdata;
	uri = crawl_obj_uristr(obj);

	me->api->log(me, LOG_DEBUG, "processor_unchanged_handler: object has not been updated\n");
	return queue_unchanged_uristr(crawl, uri, 0);
}

/* processor_failed_handler_() is installed as the CRAWL object's 'failed'
 * handler and is invoked when a fetch does not complete successfully, either
 * because of an underlying fetch failure, or because a policy callback
 * prevented it.
 */
static int
processor_failed_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata, CRAWLSTATE state)
{
	const char *uri;
	
	(void) prevtime;
	(void) userdata;

	if(state != COS_FAILED && state != COS_REJECTED && state != COS_SKIPPED)
	{
		state = COS_FAILED;
	}
	uri = crawl_obj_uristr(obj);
	return queue_updated_uristr(crawl, uri, crawl_obj_updated(obj), crawl_obj_updated(obj), crawl_obj_status(obj), 86400, state);
}
