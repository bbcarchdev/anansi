/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC.
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

static int queue_handler(CRAWL *crawl, URI **next, CRAWLSTATE *state, void *userdata);

static QUEUE *(*queue_constructor)(CONTEXT *ctx);

/* Global initialisation */
int
queue_init(void)
{
	const char *name;

	/* queue_init() is invoked before any other threads are created, so
	 * it's safe to use config_getptr_unlocked().
	 */
	name = config_getptr_unlocked(CRAWLER_APP_NAME ":queue", NULL);
	if(!name)
	{
		log_printf(LOG_CRIT, "no 'queue' configuration option could be found in the [crawl] section\n");
		return -1;
	}
	if(!strcmp(name, "db"))
	{
		queue_constructor = db_create;
	}
	else
	{
		log_printf(LOG_CRIT, "queue engine '%s' is not registered\n", name);
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

/* Initialise a crawl thread */
int
queue_init_crawler(CRAWL *crawler, CONTEXT *ctx)
{
	ctx->queue = queue_constructor(ctx);
	if(!ctx->queue)
	{
		log_printf(LOG_CRIT, "failed to intialise queue\n");
		return -1;
	}
	crawl_set_next(crawler, queue_handler);
	return 0;
}

/* Clean up a crawl thread */
int
queue_cleanup_crawler(CRAWL *crawler, CONTEXT *ctx)
{
	(void) crawler;
	
	if(!ctx->queue)
	{
		return 0;
	}
	ctx->queue->api->release(ctx->queue);
	ctx->queue = NULL;
	return 0;
}

int
queue_add_uristr(CRAWL *crawl, const char *uristr)
{
	CONTEXT *data;
	URI *uri;
	int r;

	data = crawl_userdata(crawl);
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		log_printf(LOG_ERR, "Failed to parse URI <%s>\n", uristr);
		return -1;
	}
	r = policy_uri(crawl, uri, uristr, (void *) data);
	if(r == 1)
	{
		log_printf(LOG_DEBUG, "Adding URI <%s> to crawler queue\n", uristr);
		r = data->queue->api->add(data->queue, uri, uristr);
	}
	uri_destroy(uri);
	return r;
}

int
queue_add_uri(CRAWL *crawl, URI *uri)
{
	CONTEXT *data;
	char *uristr;
	int r;
		
	data = crawl_userdata(crawl);
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		log_printf(LOG_CRIT, "failed to unparse URI\n");
		return -1;
	}
	r = policy_uri(crawl, uri, uristr, (void *) data);
	if(r == 1)
	{
		log_printf(LOG_DEBUG, "Adding URI <%s> to crawler queue\n", uristr);
		r = data->queue->api->add(data->queue, uri, uristr);
	}
	free(uristr);
	return r;	
}

int
queue_updated_uristr(CRAWL *crawl, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->updated_uristr(data->queue, uristr, updated, last_modified, status, ttl, state);
}

int
queue_updated_uri(CRAWL *crawl, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->updated_uri(data->queue, uri, updated, last_modified, status, ttl, state);
}

int
queue_unchanged_uristr(CRAWL *crawl, const char *uristr, int error)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->unchanged_uristr(data->queue, uristr, error);
}

int
queue_unchanged_uri(CRAWL *crawl, URI *uri, int error)
{
	CONTEXT *data;
	
	data = crawl_userdata(crawl);	
	return data->queue->api->unchanged_uri(data->queue, uri, error);
}

static int
queue_handler(CRAWL *crawl, URI **next, CRAWLSTATE *state, void *userdata)
{
	CONTEXT *data;
	
	(void) crawl;
	
	data = (CONTEXT *) userdata;

	if(crawld_terminate)
	{
		return 0;
	}
	return data->queue->api->next(data->queue, next, state);
}
