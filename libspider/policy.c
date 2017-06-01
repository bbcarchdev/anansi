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

/* Implements a libcrawl policy handler which implements URI scheme and
 * content type white/blacklisting.
 */

static CRAWLSTATE spider_policy_uri_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);
static int spider_policy_checkpoint_(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata);

SPIDERPOLICY *
spider_policy_create_name(SPIDER *spider, const char *name)
{
	SPIDERPOLICY *p;
	
	if(!strcmp(name, "schemes"))
	{
		p = spider_policy_schemes_create_(spider);
	}
	else if(!strcmp(name, "content-types"))
	{
		p = spider_policy_contenttypes_create_(spider);
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
	if(spider->api->add_policy(spider, p))
	{
		p->api->release(p);
		return NULL;
	}
	return p;
}

/* INTERNAL: Invoked automatically by SPIDER::add_policy()
 * IMPORTANT: the spider must be at least read-locked prior
 * to invocation.
 */
int
spider_policy_attach_(SPIDER *spider, SPIDERPOLICY *policy)
{
	(void) policy;

	crawl_set_uri_policy(spider->crawl, spider_policy_uri_);
	crawl_set_checkpoint(spider->crawl, spider_policy_checkpoint_);
	return 0;
}

/* spider_policy_uri_() is installed as the CRAWL object's uri_policy callback by
 * spider_policy_attach_(), above.
 *
 * The uri_policy callback is invoked before a network fetch is attempted
 * by libcrawl. This function is also invoked directly by the libcrawld
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
spider_policy_uri_(CRAWL *crawl, URI *uri, const char *uristr, void *userdata)
{
	(void) crawl;
	(void) uri;
	(void) uristr;
	(void) userdata;

	return COS_ACCEPTED;
}

/* policy_uri_() is installed as the CRAWL object's checkpoint callback by
 * policy_init_context(), above.
 *
 * The checkpoint callback is invoked during the fetch in order to confirm
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
spider_policy_checkpoint_(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata)
{
	(void) crawl;
	(void) obj;
	(void) status;
	(void) userdata;

	return COS_ACCEPTED;
}
