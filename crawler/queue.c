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

/* Implement a libcrawl queue handler which interfaces with crawld queue
 * modules.
 */

static int queue_handler_(CRAWL *crawl, URI **next, CRAWLSTATE *state, void *userdata);

static QUEUE *(*constructor)(CONTEXT *ctx);

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

/* Add a URI to the crawl queue */
int
queue_add_uristr(CRAWL *crawl, const char *uristr)
{
	CONTEXT *data;
	URI *uri;
	CRAWLSTATE state;
	int r;

	data = crawl_userdata(crawl);
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		log_printf(LOG_ERR, MSG_E_CRAWL_URIPARSE " <%s>\n", uristr);
		return -1;
	}
	state = policy_uri_(crawl, uri, uristr, (void *) data);
	if(state == COS_ACCEPTED)
	{
		log_printf(LOG_DEBUG, "Adding URI <%s> to crawler queue\n", uristr);
		r = data->queue->api->add(data->queue, uri, uristr);
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
	CONTEXT *data;
	char *uristr;
	CRAWLSTATE state;
	int r;
		
	data = crawl_userdata(crawl);
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		log_printf(LOG_CRIT, MSG_C_CRAWL_URIUNPARSE "\n");
		return -1;
	}
	state = policy_uri_(crawl, uri, uristr, (void *) data);
	if(state == COS_ACCEPTED)
	{
		log_printf(LOG_DEBUG, "Adding URI <%s> to crawler queue\n", uristr);
		r = data->queue->api->add(data->queue, uri, uristr);
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

/* Implement the libcrawl queue handler */
static int
queue_handler_(CRAWL *crawl, URI **next, CRAWLSTATE *state, void *userdata)
{
	CONTEXT *data;
	
	(void) crawl;
	
	data = (CONTEXT *) userdata;

	if(data->terminate)
	{
		return 0;
	}
	return data->queue->api->next(data->queue, next, state);
}
