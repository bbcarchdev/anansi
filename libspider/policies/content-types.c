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

/* A policy handler implementation to white- and black-list by URI scheme */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <syslog.h>

#define SPIDER_POLICY_STRUCT_DEFINED    1

#include "libspider.h"

struct spider_policy_struct
{
	struct spider_policy_api_struct *api;
	unsigned long refcount;
	pthread_rwlock_t lock;
	SPIDER *spider;
	CRAWL *crawler;
	char **whitelist;
	char **blacklist;
};

static unsigned long contenttypes_addref_(SPIDERPOLICY *me);
static unsigned long contenttypes_release_(SPIDERPOLICY *me);
static CRAWLSTATE contenttypes_uri_(SPIDERPOLICY *me, URI *uri, const char *uristr);
static CRAWLSTATE contenttypes_checkpoint_(SPIDERPOLICY *me, CRAWLOBJ *obj, int *status);

static char **contenttypes_list_create_(SPIDERPOLICY *me, const char *name, const char *defval);
static int contenttypes_list_destroy_(SPIDERPOLICY *me, char **list);

static struct spider_policy_api_struct contenttypes_policy_api_ = {
	NULL,
	contenttypes_addref_,
	contenttypes_release_,
	contenttypes_uri_,
	contenttypes_checkpoint_
};

/* Create a new URI scheme policy handler */
SPIDERPOLICY *
spider_policy_contenttypes_create_(SPIDER *spider)
{
	SPIDERPOLICY *p;
	CRAWL *crawler;

	crawler = spider->api->crawler(spider);

	p = (SPIDERPOLICY *) crawl_alloc(crawler, sizeof(SPIDERPOLICY));
	if(!p)
	{
		return NULL;
	}
	p->api = &contenttypes_policy_api_;
	pthread_rwlock_init(&(p->lock), NULL);
	p->spider = spider;
	p->crawler = crawler;
	p->refcount = 1;
	p->whitelist = contenttypes_list_create_(p, "policy:content-types:whitelist", NULL);
	p->blacklist = contenttypes_list_create_(p, "policy:content-types:blacklist", NULL);
	return p;
}

static unsigned long
contenttypes_addref_(SPIDERPOLICY *me)
{
	unsigned long r;

	pthread_rwlock_wrlock(&(me->lock));
	me->refcount++;
	r = me->refcount;
	pthread_rwlock_unlock(&(me->lock));
	return r;
}

static unsigned long
contenttypes_release_(SPIDERPOLICY *me)
{
	unsigned long r;

	pthread_rwlock_wrlock(&(me->lock));
	me->refcount--;
	r = me->refcount;
	pthread_rwlock_unlock(&(me->lock));
	if(r)
	{
		return r;
	}
	contenttypes_list_destroy_(me, me->whitelist);
	contenttypes_list_destroy_(me, me->blacklist);
	pthread_rwlock_destroy(&(me->lock));
	crawl_free(me->crawler, me);
	return 0;
}

/* The uri_policy callback is invoked before a network fetch is attempted
 * by libcrawl. This function is also invoked directly by the libspider
 * queue logic to avoid adding URIs to the queue which do not meet the
 * policy.
 *
 * It should return a valid CRAWLSTATE value based upon its determination.
 * A state of COS_ACCEPTED will cause processing to proceed, any other value
 * will cause failure.
 *
 * As a special case, COS_NEW (not generally a valid return value) will
 * be coerced to COS_FAILED.
 */
static CRAWLSTATE
contenttypes_uri_(SPIDERPOLICY *me, URI *uri, const char *uristr)
{
	(void) me;
	(void) uri;
	(void) uristr;

	return COS_ACCEPTED;
}

/* The checkpoint callback is invoked during the fetch in order to confirm
 * that it should proceed before any (signifcant amounts of) data are
 * downloaded.
 *
 * It should return a valid CRAWLSTATE value based upon its determination.
 * A state of COS_ACCEPTED will cause processing to proceed, any other value
 * will cause failure.
 *
 * As a special case, COS_NEW (not generally a valid return value) will
 * be coerced to COS_FAILED.
 */
static CRAWLSTATE
contenttypes_checkpoint_(SPIDERPOLICY *me, CRAWLOBJ *obj, int *status)
{
	int n, c;
	const char *type;
	char *s, *t;
	
	if(*status >= 300 && *status < 400)
	{
		return COS_SKIPPED_COMMIT;
	}
	type = crawl_obj_type(obj);
	if(!type)
	{
		type = "";
	}
	s = crawl_strdup(me->crawler, type);
	if(!s)
	{
		return COS_ERR;
	}
	t = strchr(s, ';');
	if(t)
	{
		*t = 0;
		t--;
		while(t > s)
		{
			if(!isspace(*t))
			{
				break;
			}
			*t = 0;
			t--;
		}
	}
	me->spider->api->log(me->spider, LOG_DEBUG, "content-types: type is '%s', status is '%d'\n", s, *status);
	if(me->whitelist && me->whitelist[0])
	{
		n = 0;
		for(c = 0; me->whitelist[c]; c++)
		{
			if(!strcasecmp(me->whitelist[c], s))
			{
				n = c;
				break;
			}
		}
		if(!n)
		{
			me->spider->api->log(me->spider, LOG_DEBUG, "content-types: type '%s' not matched by whitelist\n", s);
			crawl_free(me->crawler, s);
			*status = 406;
			return COS_SKIPPED;
		}
	}
	if(me->blacklist)
	{
		for(c = 0; me->blacklist[c]; c++)
		{
			if(!strcasecmp(me->blacklist[c], s))
			{
				me->spider->api->log(me->spider, LOG_DEBUG, "content-types: type '%s' is blacklisted\n", s);
				crawl_free(me->crawler, s);
				*status = 406;
				return COS_REJECTED;
			}
		}
	}
	crawl_free(me->crawler, s);
	me->spider->api->log(me->spider, LOG_DEBUG, "content-types: checkpoint passed\n");
	return COS_ACCEPTED;
}

static char **
contenttypes_list_create_(SPIDERPOLICY *me, const char *key, const char *defval)
{
	char **list;
	char *s, *t, *p;
	size_t count;
	int prevspace;
	
	if(!defval)
	{
		defval = "";
	}
	s = me->spider->api->config_geta(me->spider, key, defval);
	if(!s)
	{
		return NULL;
	}
	count = 0;
	prevspace = 1;
	for(t = s; *t; t++)
	{
		if(isspace(*t) || *t == ';' || *t == ',')
		{
			if(!prevspace)
			{
				count++;
			}
			prevspace = 1;
			continue;
		}
		prevspace = 0;
	}
	if(!prevspace)
	{
		count++;
	}
	list = (char **) crawl_alloc(me->crawler, (count + 1) * sizeof(char *));
	if(!list)
	{
		crawl_free(me->crawler, s);
		return NULL;
	}
	count = 0;
	p = NULL;
	for(t = s; *t; t++)
	{
		if(isspace(*t) || *t == ';' || *t == ',')
		{
			*t = 0;
			if(p)
			{
				list[count] = crawl_strdup(me->crawler, p);
				if(!list[count])
				{
					contenttypes_list_destroy_(me, list);
					crawl_free(me->crawler, s);
					return NULL;
				}
				count++;
				p = NULL;
			}
			continue;
		}
		if(!p)
		{
			p = t;
		}
	}
	if(p)
	{
		list[count] = crawl_strdup(me->crawler, p);
		if(!list[count])
		{
			contenttypes_list_destroy_(me, list);
			crawl_free(me->crawler, s);
			return NULL;
		}		
		count++;
	}
	list[count] = NULL;
	for(count = 0; list[count]; count++)
	{
		me->spider->api->log(me->spider, LOG_DEBUG, "content-types: %s => '%s'\n", key, list[count]);
	}
	crawl_free(me->crawler, s);
	return list;
}

static int
contenttypes_list_destroy_(SPIDERPOLICY *me, char **list)
{
	size_t c;
	
	for(c = 0; list && list[c]; c++)
	{
		crawl_free(me->crawler, list[c]);
	}
	crawl_free(me->crawler, list);
	return 0;
}
