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

static size_t diskcache_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, char *buf, size_t bufsize, int temporary);
static int diskcache_create_dirs_(CRAWL *crawl, const char *path);
static int diskcache_copy_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, int temporary);

static unsigned long diskcache_init_(CRAWLCACHE *cache);
static unsigned long diskcache_done_(CRAWLCACHE *cache);
static FILE *diskcache_open_write_(CRAWLCACHE *cache, const CACHEKEY key);
static FILE *diskcache_open_read_(CRAWLCACHE *cache, const CACHEKEY key);
static int diskcache_close_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
static int diskcache_close_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f, CRAWLOBJ *obj);
static int diskcache_info_read_(CRAWLCACHE *cache, const CACHEKEY key, json_t **info);
static int diskcache_info_write_(CRAWLCACHE *cache, const CACHEKEY key, const json_t *info);
static FILE *diskcache_open_info_write_(CRAWLCACHE *cache, const CACHEKEY key);
static FILE *diskcache_open_info_read_(CRAWLCACHE *cache, const CACHEKEY key);
static char *diskcache_uri_(CRAWLCACHE *cache, const CACHEKEY key);
static int diskcache_set_username_(CRAWLCACHE *cache, const char *username);
static int diskcache_set_password_(CRAWLCACHE *cache, const char *password);
static int diskcache_set_endpoint_(CRAWLCACHE *cache, const char *endpoint);
static int diskcache_close_info_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
static int diskcache_close_info_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);

static const CRAWLCACHEIMPL diskcache_impl = {
	NULL,
	diskcache_init_,
	diskcache_done_,
	diskcache_open_write_,
	diskcache_open_read_,
	diskcache_close_rollback_,
	diskcache_close_commit_,
	diskcache_info_read_,
	diskcache_info_write_,
	diskcache_uri_,
	diskcache_set_username_,
	diskcache_set_password_,
	diskcache_set_endpoint_
};

const CRAWLCACHEIMPL *diskcache = &diskcache_impl;

static unsigned long
diskcache_init_(CRAWLCACHE *cache)
{
	(void) cache;

	
	crawl_log_(cache->crawl, LOG_DEBUG, "disk: initialising cache at <%s>\n", cache->crawl->cachepath);
	return 1;
}

unsigned long
diskcache_done_(CRAWLCACHE *cache)
{
	(void) cache;

	return 0;
}

static FILE *
diskcache_open_write_(CRAWLCACHE *cache, const CACHEKEY key)
{
	FILE *f;

	if(diskcache_copy_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, 1))
	{
		return NULL;
	}
	if(diskcache_create_dirs_(cache->crawl, cache->crawl->cachefile))
	{
		return NULL;
	}
	f = fopen(cache->crawl->cachefile, "wb");
	if(!f)
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_PAYLOADWRITE ": %s: %s\n", cache->crawl->cachefile, strerror(errno));
		return NULL;
	}
	return f;
}

static FILE *
diskcache_open_read_(CRAWLCACHE *cache, const CACHEKEY key)
{
	FILE *f;

	if(diskcache_copy_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, 0))
	{
		return NULL;
	}
	f = fopen(cache->crawl->cachefile, "rb");
	if(!f)
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_PAYLOADREAD ": %s: %s\n", cache->crawl->cachefile, strerror(errno));
		return NULL;
	}
	return f;
}

static int
diskcache_close_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f)
{
	if(f)
	{
		fclose(f);
		if(diskcache_copy_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, 1))
		{
			return -1;
		}
	}
	if(unlink(cache->crawl->cachefile))
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_PAYLOADRM "%s: %s\n", cache->crawl->cachefile, strerror(errno));
		return -1;
	}
	return 0;
}

static int
diskcache_close_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f, CRAWLOBJ *obj)
{
	(void) obj;

	if(!f)
	{
		errno = EINVAL;
		return -1;
	}	
	fclose(f);	
	if(diskcache_copy_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, 1))
	{
		return -1;
	}
	if(diskcache_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, cache->crawl->cachetmp, cache->crawl->cachefile_len, 0) > cache->crawl->cachefile_len)
	{
		errno = ENOMEM;
		return -1;
	}
	if(rename(cache->crawl->cachefile, cache->crawl->cachetmp))
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_PAYLOADCOMMIT ": %s -> %s: %s\n", cache->crawl->cachefile, cache->crawl->cachetmp, strerror(errno));
		return -1;
	}
	return 0;
}

static int
diskcache_info_read_(CRAWLCACHE *cache, const CACHEKEY key, json_t **dict)
{
	FILE *f;
	json_t *json;

	f = diskcache_open_info_read_(cache, key);
	if(!f)
	{
		return -1;
	}
	json = json_loadf(f, 0, NULL);
	fclose(f);
	if(!json)
	{
		return -1;
	}
	if(*dict)
	{
		json_decref(*dict);
	}
	*dict = json;
	return 0;
}

static int
diskcache_info_write_(CRAWLCACHE *cache, const CACHEKEY key, const json_t *dict)
{
	FILE *f;
	int e;

	f = diskcache_open_info_write_(cache, key);
	if(!f)
	{
		return -1;
	}
	e = json_dumpf(dict, f, JSON_PRESERVE_ORDER);
	if(e)
	{
		diskcache_close_info_rollback_(cache, key, f);
		return e;
	}
	return diskcache_close_info_commit_(cache, key, f);
}

static FILE *
diskcache_open_info_write_(CRAWLCACHE *cache, const CACHEKEY key)
{
	FILE *f;
	
	if(diskcache_copy_filename_(cache->crawl, key, CACHE_INFO_SUFFIX, 1))
	{
		return NULL;
	}
	if(diskcache_create_dirs_(cache->crawl, cache->crawl->cachefile))
	{
		return NULL;
	}
	f = fopen(cache->crawl->cachefile, "w");
	if(!f)
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_PAYLOADWRITE ": %s: %s\n", cache->crawl->cachefile, strerror(errno));
		return NULL;
	}
	return f; 
}

static FILE *
diskcache_open_info_read_(CRAWLCACHE *cache, const CACHEKEY key)
{
	FILE *f;

	if(diskcache_copy_filename_(cache->crawl, key, CACHE_INFO_SUFFIX, 0))
	{
		return NULL;
	}
	f = fopen(cache->crawl->cachefile, "r");
	if(!f)
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_INFOREAD ": %s: %s\n", cache->crawl->cachefile, strerror(errno));
		return NULL;
	}
	return f;
}

static int
diskcache_close_info_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f)
{
	if(!f)
	{
		errno = EINVAL;
		return -1;
	}	
	fclose(f);	
	if(diskcache_copy_filename_(cache->crawl, key, CACHE_INFO_SUFFIX, 1))
	{
		return -1;
	}
	if(diskcache_filename_(cache->crawl, key, CACHE_INFO_SUFFIX, cache->crawl->cachetmp, cache->crawl->cachefile_len, 0) > cache->crawl->cachefile_len)
	{
		errno = ENOMEM;
		return -1;
	}
	if(rename(cache->crawl->cachefile, cache->crawl->cachetmp))
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_INFOCOMMIT ": %s -> %s: %s\n", cache->crawl->cachefile, cache->crawl->cachetmp, strerror(errno));
		return -1;
	}
	return 0;
}

static int
diskcache_close_info_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f)
{
	if(f)
	{
		fclose(f);
		if(diskcache_copy_filename_(cache->crawl, key, CACHE_INFO_SUFFIX, 1))
		{
			return -1;
		}
	}
	if(unlink(cache->crawl->cachefile))
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_DISK_INFORM ": %s: %s\n", cache->crawl->cachefile, strerror(errno));
		return -1;
	}
	return 0;
}

static char *
diskcache_uri_(CRAWLCACHE *cache, const CACHEKEY key)
{
	size_t needed;
	char *p;

	needed = diskcache_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, NULL, 0, 0);
	p = (char *) crawl_alloc(cache->crawl, needed);
	if(!p)
	{
		return NULL;
	}
	if(diskcache_filename_(cache->crawl, key, CACHE_PAYLOAD_SUFFIX, p, needed, 0) != needed)
	{
		crawl_free(cache->crawl, p);
		return NULL;
	}
	return p;
}

static int
diskcache_set_username_(CRAWLCACHE *cache, const char *username)
{
	(void) cache;
	(void) username;
   
	return 0;
}

static int
diskcache_set_password_(CRAWLCACHE *cache, const char *password)
{
	(void) cache;
	(void) password;

	return 0;
}

static int
diskcache_set_endpoint_(CRAWLCACHE *cache, const char *endpoint)
{
	(void) cache;
	(void) endpoint;

	return 0;
}

static size_t
diskcache_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, char *buf, size_t bufsize, int temporary)
{
	size_t needed;
	const char *suffix;

	if(buf)
	{
		*buf = 0;
	}
	/* base path + "/" + key[0..1] + "/" + key[2..3] + "/" + key[0..n] + "." + type + ".tmp" */
	needed = strlen(crawl->cachepath) + 1 + 2 + 1 + 2 + 1 + strlen(key) + 1 + (type ? strlen(type) : 0) + 4 + 1;
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
	sprintf(buf, "%s/%c%c/%c%c/%s%s%s%s", crawl->cachepath, key[0], key[1], key[2], key[3], key, (type && type[0] ? "." : ""), (type ? type : ""), suffix);
	return needed;
}

static int
diskcache_create_dirs_(CRAWL *crawl, const char *path)
{
	char *t;
	struct stat sbuf;
	
	t = NULL;
	for(;;)
	{		
		t = strchr((t ? t + 1 : path), '/');
		if(!t)
		{
			break;
		}
		if(t == path)
		{
			/* absolute path: skip the leading slash */
			continue;
		}
		strncpy(crawl->cachetmp, path, (t - path));
		crawl->cachetmp[t - path] = 0;
		if(stat(crawl->cachetmp, &sbuf))
		{
			if(errno != ENOENT)
			{
				return -1;
			}
			crawl_log_(crawl, LOG_ERR, MSG_E_DISK_DIRSTAT "%s: %s\n", crawl->cachetmp, strerror(errno));
		}
		if(mkdir(crawl->cachetmp, 0777))
		{
			if(errno == EEXIST)
			{
				continue;
			}
			crawl_log_(crawl, LOG_ERR, MSG_E_DISK_MKDIR "%s: %s\n", crawl->cachetmp, strerror(errno));
			return -1;
		}
	}
	return 0;
}

static int
diskcache_copy_filename_(CRAWL *crawl, const CACHEKEY key, const char *type, int temporary)
{
	size_t needed;
	char *p;

	needed = diskcache_filename_(crawl, key, type, NULL, 0, 1);
	if(needed > crawl->cachefile_len)
	{
		p = (char *) crawl_realloc(crawl, crawl->cachefile, needed);
		if(!p)
		{
		    return -1;
		}
		crawl->cachefile = p;
		p = (char *) crawl_realloc(crawl, crawl->cachetmp, needed);
		if(!p)
		{
		    return -1;
		}
		crawl->cachetmp = p;
		crawl->cachefile_len = needed;
	}
	if(diskcache_filename_(crawl, key, type, crawl->cachefile, needed, temporary) > needed)
	{
		return -1;
	}
	return 0;
}
