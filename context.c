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

#include "p_libcrawl.h"

CRAWL *
crawl_create(void)
{
	CRAWL *p;

	/* XXX thread-safe curl_global_init() */
	p = (CRAWL *) calloc(1, sizeof(CRAWL));
	/* Use 'diskcache' as the default */
	p->cache.impl = diskcache;
	p->cache.crawl = p;
	p->cachepath = strdup("cache");
	p->ua = strdup("User-Agent: Mozilla/5.0 (compatible; libcrawl; +https://github.com/nevali/crawl)");
	p->accept = strdup("Accept: */*");
	if(!p->cachepath || !p->ua || !p->accept)
	{
		crawl_destroy(p);
		return NULL;
	}
	return p;
}

void
crawl_destroy(CRAWL *p)
{
	if(p)
	{
		free(p->cachepath);
		free(p->cachefile);
		free(p->cachetmp);
		free(p->accept);
		free(p->ua);
		free(p);
	}
}

/* Set the Accept header used in requests */
int
crawl_set_accept(CRAWL *crawl, const char *accept)
{
	char *p;
	
	/* Accept: ... - 9 + strlen(accept) */
	p = (char *) malloc(9 + strlen(accept));
	if(!p)
	{
		return -1;
	}
	sprintf(p, "Accept: %s", accept);
	free(crawl->accept);
	crawl->accept = p;
	return 0;
}

/* Set the User-Agent header used in requests */
int
crawl_set_ua(CRAWL *crawl, const char *ua)
{
	char *p;
	
	/* User-Agent: ... - 13 + strlen(ua) */
	p = (char *) malloc(13 + strlen(ua));
	if(!p)
	{
		return -1;
	}
	sprintf(p, "User-Agent: %s", ua);
	free(crawl->ua);
	crawl->ua = p;
	return 0;
}

/* Set the cache path */
int
crawl_set_cachepath(CRAWL *crawl, const char *path)
{
	char *p;
	
	p = (char *) strdup(path);
	if(!p)
	{
		return -1;
	}
	free(crawl->cachepath);
	crawl->cachepath = p;
	return 0;
}


/* Set the private user-data pointer */
int
crawl_set_userdata(CRAWL *crawl, void *userdata)
{
	crawl->userdata = userdata;
	return 0;
}

/* Retrieve the user-data pointer */
void *
crawl_userdata(CRAWL *crawl)
{
	return crawl->userdata;
}

/* Set the URI policy callback */
int
crawl_set_uri_policy(CRAWL *crawl, crawl_uri_policy_cb cb)
{
	crawl->uri_policy = cb;
	return 0;
}

/* Set the object-updated callback */
int
crawl_set_updated(CRAWL *crawl, crawl_updated_cb cb)
{
	crawl->updated = cb;
	return 0;
}


/* Set the object-update-failed callback */
int
crawl_set_failed(CRAWL *crawl, crawl_failed_cb cb)
{
	crawl->failed = cb;
	return 0;
}

/* Set the verbosity flag */
int
crawl_set_verbose(CRAWL *crawl, int verbose)
{
	crawl->verbose = verbose;
	return 0;
}

/* Set the fetch-next callback */
int
crawl_set_next(CRAWL *crawl, crawl_next_cb cb)
{
	crawl->next = cb;
	return 0;
}

/* Set the checkpoint callback */
int
crawl_set_checkpoint(CRAWL *crawl, crawl_checkpoint_cb cb)
{
	crawl->checkpoint = cb;
	return 0;
}

/* Set the unchanged callback */
int
crawl_set_unchanged(CRAWL *crawl, crawl_unchanged_cb cb)
{
	crawl->unchanged = cb;
	return 0;
}

/* Set the prefetch callback */
int
crawl_set_prefetch(CRAWL *crawl, crawl_prefetch_cb cb)
{
	crawl->prefetch = cb;
	return 0;
}
