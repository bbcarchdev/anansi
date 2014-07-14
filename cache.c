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

CRAWLOBJ *
crawl_locate(CRAWL *crawl, const char *uristr)
{
	URI *uri;
	CRAWLOBJ *r;
	
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		return NULL;
	}
	r = crawl_locate_uri(crawl, uri);
	uri_destroy(uri);
	return r;
}

CRAWLOBJ *
crawl_locate_uri(CRAWL *crawl, URI *uri)
{
	CRAWLOBJ *obj;
	
	obj = crawl_obj_create_(crawl, uri);
	if(!obj)
	{
		return NULL;
	}
	if(crawl_obj_locate_(obj))
	{
		crawl_obj_destroy(obj);
		return NULL;
	}
	return obj;
}

int
crawl_cache_key(CRAWL *restrict crawl, const char *restrict uri, char *restrict buf, size_t buflen)
{
	CACHEKEY k;
	
	crawl_cache_key_(crawl, k, uri);
	if(buflen)
	{
		strncpy(buf, k, buflen - 1);
		buf[buflen - 1] = 0;
	}
	return 0;
}

int
crawl_cache_key_uri(CRAWL *restrict crawl, URI *restrict uri, char *restrict buf, size_t buflen)
{
	size_t needed;
	char *uristr;
	int r;
	
	needed = uri_str(uri, NULL, 0);
	uristr = NULL;
	if(!needed ||
		(uristr = (char *) malloc(needed)) == NULL ||
		uri_str(uri, uristr, needed) != needed)
	{
		free(uristr);
		return -1;
	}
	r = crawl_cache_key(crawl, uristr, buf, buflen);
	free(uristr);
	return r;
}

int
crawl_cache_key_(CRAWL *crawl, CACHEKEY dest, const char *uri)
{
	unsigned char buf[SHA256_DIGEST_LENGTH];
	size_t c;
	char *t;
	
	(void) crawl;
    
	/* The cache key is a truncated SHA-256 of the URI */
	c = strlen(uri);
	/* If there's a fragment, remove it */
	t = strchr(uri, '#');
	if(t)
	{
		c = t - uri;
	}
	SHA256((const unsigned char *) uri, c, buf);
	for(c = 0; c < (CACHE_KEY_LEN / 2); c++)
	{
		dest += sprintf(dest, "%02x", buf[c] & 0xff);
	}
	return 0;
}

size_t
cache_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, char *buf, size_t bufsize, int temporary)
{
	size_t needed;
	const char *suffix;

	if(buf)
	{
		*buf = 0;
	}
	/* base path + "/" + key[0..1] + "/" + key[2..3] + "/" + key[0..n] + "." + type + ".tmp" */
	needed = strlen(crawl->cachepath) + 1 + 2 + 1 + 2 + 1 + strlen(key) + 1 + strlen(type) + 4 + 1;
	if(!buf || needed > bufsize)
	{
		return needed;
	}
	if(temporary)
	{
		suffix = CACHE_TMP_SUFFIX;
	}
	else
	{
		suffix = "";
	}
	sprintf(buf, "%s/%c%c/%c%c/%s.%s%s", crawl->cachepath, key[0], key[1], key[2], key[3], key, type, suffix);
	return needed;
}

FILE *
cache_open_payload_write_(CRAWL *crawl, const CACHEKEY key)
{
	return crawl->cache.impl->payload_open_write(&(crawl->cache), key);
}

int
cache_close_payload_rollback_(CRAWL *crawl, const CACHEKEY key, FILE *f)
{
	return crawl->cache.impl->payload_close_rollback(&(crawl->cache), key, f);
}

int
cache_close_payload_commit_(CRAWL *crawl, const CACHEKEY key, FILE *f)
{
	return crawl->cache.impl->payload_close_commit(&(crawl->cache), key, f);
}

int
cache_info_read_(CRAWL *crawl, const CACHEKEY key, jd_var *dict)
{
	return crawl->cache.impl->info_read(&(crawl->cache), key, dict);
}

int
cache_info_write_(CRAWL *crawl, const CACHEKEY key, jd_var *dict)
{
	return crawl->cache.impl->info_write(&(crawl->cache), key, dict);
}

