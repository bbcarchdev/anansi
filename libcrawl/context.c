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

#include "p_libcrawl.h"

CRAWL *
crawl_create(void)
{
	CRAWL *p;

	p = (CRAWL *) calloc(1, sizeof(CRAWL));
	p->ua = strdup("User-Agent: Mozilla/5.0 (compatible; Anansi; libcrawl; +https://bbcarchdev.github.io/res/)");
	p->accept = strdup("Accept: */*");
	if(!p->ua || !p->accept)
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
		if(p->cacheuri)
		{
			uri_destroy(p->cacheuri);
		}
		if(p->uri)
		{
			uri_info_destroy(p->uri);
		}
		free(p->cachepath);
		free(p->cachefile);
		free(p->cachetmp);
		free(p->accept);
		free(p->ua);
		free(p);
	}
}

int
crawl_set_cache(CRAWL *crawl, const CRAWLCACHEIMPL *cache)
{
	crawl->cache.crawl = crawl;
	if(crawl->cache.impl)
	{
		crawl->cache.impl->done(&(crawl->cache));
	}
	crawl->cache.data = NULL;
	if(cache)
	{
		crawl->cache.impl = cache;
		crawl->cache.impl->init(&(crawl->cache));
	}
	return 0;
}

/* Set the username (or access key) used by the cache */
int
crawl_set_username(CRAWL *crawl, const char *username)
{	
	if(!crawl->cache.impl)
	{
		crawl_cache_init_(crawl);
	}
	return crawl->cache.impl->set_username(&(crawl->cache), username);
}

/* Set the password (or secret) used by the cache */
int
crawl_set_password(CRAWL *crawl, const char *password)
{
	if(!crawl->cache.impl)
	{
		crawl_cache_init_(crawl);
	}
	return crawl->cache.impl->set_password(&(crawl->cache), password);
}

/* Set the endpoint used by the cache */
int
crawl_set_endpoint(CRAWL *crawl, const char *endpoint)
{
	if(!crawl->cache.impl)
	{
		crawl_cache_init_(crawl);
	}
	return crawl->cache.impl->set_endpoint(&(crawl->cache), endpoint);
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

/* Set the cache path (or uri string)*/
int
crawl_set_cache_path(CRAWL *crawl, const char *path)
{
	URI *uri, *cwd;
	int r;

	cwd = uri_create_cwd();
	if(!cwd)
	{
		return -1;
	}
	uri = uri_create_str(path, cwd);
	if(!uri)
	{
		uri_destroy(cwd);
		return -1;
	}
	r = crawl_set_cache_uri(crawl, uri);
	uri_destroy(uri);
	uri_destroy(cwd);
	return r;
}

int
crawl_set_cache_uri(CRAWL *crawl, URI *uri)
{
	char *s;
	URI *p;
	URI_INFO *info;
	const CRAWLCACHEIMPL *impl;

	p = uri_create_uri(uri, NULL);
	if(!p)
	{
		return -1;
	}
	info = uri_info(uri);
	if(!info)
	{
		uri_destroy(p);
		return -1;
	}
	impl = crawl_cache_scheme(crawl, info->scheme);
	if(!impl)
	{
		uri_info_destroy(info);
		uri_destroy(p);
		return -1;
	}
	s = strdup(info->path ? info->path : "");
	if(!s)
	{
		uri_info_destroy(info);
		uri_destroy(p);
		return -1;
	}
	crawl_set_cache(crawl, NULL);
	if(crawl->cacheuri)
	{
		uri_destroy(crawl->cacheuri);
	}
	if(crawl->cachepath)
	{
		crawl->cachepath = NULL;
	}
	crawl->cacheuri = p;
	crawl->cachepath = s;
	crawl->uri = info;
	return crawl_set_cache(crawl, impl);
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

int
crawl_set_logger(CRAWL *crawl, void (*logger)(int, const char *, va_list))
{
	crawl->logger = logger;
	return 0;
}

void
crawl_log_(CRAWL *crawl, int priority, const char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	if(crawl && crawl->logger)
	{
		crawl->logger(priority, format, ap);
	}
	else
	{
		vsyslog(priority, format, ap);
	}
	va_end(ap);
}

int
crawl_cache_init_(CRAWL *crawl)
{
	if(!crawl->cachepath || !crawl->cache.impl)
	{
		/* Use 'diskcache' as the default */
		return crawl_set_cache_path(crawl, "cache");
	}
	return 0;
}
