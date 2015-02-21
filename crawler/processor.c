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
static int processor_failed_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

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
		log_printf(LOG_CRIT, "no 'processor' configuration option could be found in the [crawl] section\n");
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
		log_printf(LOG_CRIT, "processing engine '%s' is not registered\n", name);
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
		log_printf(LOG_CRIT, "failed to initialise processing engine\n");
		return -1;
	}
	crawl_set_updated(crawl, processor_handler_);
	crawl_set_unchanged(crawl, processor_unchanged_handler_);
	crawl_set_failed(crawl, processor_failed_handler_);
	return 0;
}

static int
processor_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	CONTEXT *data;
	PROCESSOR *pdata;
	const char *content_type, *uri, *location;
	int r, status, ttl;
	CRAWLSTATE state;

	(void) prevtime;

	state = COS_FAILED;
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
			log_printf(LOG_NOTICE, "received a %d redirect with no location specified\n");
		}
		else if(strcmp(location, uri))
		{
			log_printf(LOG_DEBUG, "processor_handler: following %d redirect to <%s>\n", status, location);
			queue_add_uristr(crawl, location);
		}
	}
	log_printf(LOG_DEBUG, "processor_handler: object has been updated\n");
	r = pdata->api->process(pdata, obj, uri, content_type);
	if(r < 0)
	{
		state = COS_FAILED;
		ttl = 86400;
	}
	else if(r > 0)
	{
		state = COS_ACCEPTED;
		ttl = 86400;
	}
	else
	{
		state = COS_REJECTED;
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

static int
processor_failed_handler_(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata)
{
	const char *uri;
	
	(void) prevtime;
	(void) userdata;

	uri = crawl_obj_uristr(obj);
	return queue_updated_uristr(crawl, uri, crawl_obj_updated(obj), crawl_obj_updated(obj), crawl_obj_status(obj), 86400, COS_FAILED);
}