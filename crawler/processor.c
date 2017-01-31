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

#include "p_libcrawld.h"

/* Implement a libcrawl processor handler which interfaces with the crawld
 * queue modules.
 */

static int processor_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int processor_unchanged_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);
static int processor_failed_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata, CRAWLSTATE state);

static PROCESSOR *(*constructor)(CRAWL *crawler);

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

/* Create a processor instance and attach it to the context */
int
processor_init_context(CONTEXT *context)
{
	CRAWL *crawl;

	crawl = context->crawl;
	context->processor = constructor(crawl);
	if(!context->processor)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_PROCESSORINIT "\n");
		return -1;
	}
	crawl_set_updated(crawl, processor_handler_);
	crawl_set_unchanged(crawl, processor_unchanged_handler_);
	crawl_set_failed(crawl, processor_failed_handler_);
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
	CONTEXT *data;
	PROCESSOR *pdata;
	const char *content_type, *uri, *location;
	int r, status, ttl;
	CRAWLSTATE state;

	(void) prevtime;

	r = 0;
	state = COS_ACCEPTED;
	data = (CONTEXT *) userdata;
	pdata = data->processor;
	uri = crawl_obj_uristr(obj);
	location = crawl_obj_redirect(obj);
	content_type = crawl_obj_type(obj);
	status = crawl_obj_status(obj);
	log_printf(LOG_DEBUG, "processor_handler: URI is '%s', Content-Type is '%s', status is %d\n", uri, content_type, status);
	/* If there's a redirect, ensure the redirect target will be crawled */
	if(status > 300 && status < 304)
	{
		if(!location)
		{
			log_printf(LOG_ERR, MSG_E_CRAWL_DEADREDIRECT " from <%s> (HTTP status %d)\n", uri, status);
		}
		else if(strcmp(location, uri))
		{
			log_printf(LOG_DEBUG, "processor_handler: following %d redirect to <%s>\n", status, location);
			queue_add_uristr(crawl, location);
		}
		status = COS_SKIPPED;
	}
	else
	{
		log_printf(LOG_DEBUG, "processor_handler: object has been updated\n");
	}
	if(state == COS_ACCEPTED)
	{
		r = pdata->api->process(pdata, obj, uri, content_type);
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
		log_printf(LOG_INFO, MSG_I_CRAWL_ACCEPTED " <%s>\n", uri);
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
	
	(void) obj;
	(void) prevtime;
	(void) userdata;

	uri = crawl_obj_uristr(obj);
	
	log_printf(LOG_DEBUG, "processor_unchanged_handler: object has not been updated\n");
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
