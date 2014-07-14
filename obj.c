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

static int crawl_obj_update_(CRAWLOBJ *obj);

CRAWLOBJ *
crawl_obj_create_(CRAWL *crawl, URI *uri)
{
	CRAWLOBJ *p;
	size_t needed;
	
	p = (CRAWLOBJ *) calloc(1, sizeof(CRAWLOBJ));
	if(!p)
	{
		return NULL;
	}
	p->crawl = crawl;
	p->uri = uri_create_uri(uri, NULL);
	if(!p->uri)
	{
		crawl_obj_destroy(p);
		return NULL;
	}
	needed = uri_str(p->uri, NULL, 0);
	if(!needed ||
		(p->uristr = (char *) malloc(needed)) == NULL ||
		uri_str(p->uri, p->uristr, needed) != needed ||
		crawl_cache_key_(crawl, p->key, p->uristr))
	{
		crawl_obj_destroy(p);
		return NULL;
	}
	needed = cache_filename_(crawl, p->key, CACHE_PAYLOAD_SUFFIX, NULL, 0, 0);
	if(!needed ||
		(p->payload = (char *) malloc(needed)) == NULL ||
		cache_filename_(crawl, p->key, CACHE_PAYLOAD_SUFFIX, p->payload, needed, 0) != needed)
	{
		crawl_obj_destroy(p);
		return NULL;
	}
	return p;
}

/* Open a JSON sidecar and read the information from it */
int
crawl_obj_locate_(CRAWLOBJ *obj)
{
	if(obj->crawl->cache.impl->info_read(&(obj->crawl->cache), obj->key, &(obj->info)))
	{
		return -1;
	}
	crawl_obj_update_(obj);
	return 0;
}

int
crawl_obj_destroy(CRAWLOBJ *obj)
{
	if(obj)
	{
		if(obj->uri)
		{
			uri_destroy(obj->uri);
		}
		free(obj->uristr);
		free(obj->payload);
		jd_release(&(obj->info));
		free(obj);
	}
	return 0;
}

const char *
crawl_obj_key(CRAWLOBJ *obj)
{
	return obj->key;
}

const char *
crawl_obj_payload(CRAWLOBJ *obj)
{
	return obj->payload;
}

int
crawl_obj_status(CRAWLOBJ *obj)
{
	return obj->status;
}

time_t
crawl_obj_updated(CRAWLOBJ *obj)
{
	return obj->updated;
}

uint64_t
crawl_obj_size(CRAWLOBJ *obj)
{
	return obj->size;
}

int
crawl_obj_headers(CRAWLOBJ *obj, jd_var *out, int clone)
{
	jd_var *key;
	int r;
	
	if(obj->info.type == VOID)
	{
		return -1;
	}
	r = 0;
	JD_SCOPE
	{
		key = jd_get_ks(&(obj->info), "headers", 1);
		if(key->type == VOID)
		{
			r = -1;
		}
		else if(clone)
		{
			jd_clone(out, key, 1);
		}
		else
		{
			jd_assign(out, key);
		}
	}
	return r;
}

/* Obtain the crawl object URI */
const URI *
crawl_obj_uri(CRAWLOBJ *obj)
{
	return obj->uri;
}

/* Obtain the crawl object URI as a string */
const char *
crawl_obj_uristr(CRAWLOBJ *obj)
{
	return obj->uristr;
}

const char *
crawl_obj_type(CRAWLOBJ *obj)
{
	jd_var *key;
	const char *str;
	
	if(obj->info.type == VOID)
	{
		return NULL;
	}
	str = NULL;
	JD_SCOPE
	{
		key = jd_get_ks(&(obj->info), "type", 1);
		if(key->type == VOID)
		{
			return NULL;
		}
		str = jd_bytes(key, NULL);
	}
	return str;
}

const char *
crawl_obj_redirect(CRAWLOBJ *obj)
{
	jd_var *key;
	const char *str;
	
	if(obj->info.type == VOID)
	{
		return NULL;
	}
	str = NULL;
	JD_SCOPE
	{
		key = jd_get_ks(&(obj->info), "redirect", 1);
		if(key->type == VOID)
		{
			return NULL;
		}
		str = jd_bytes(key, NULL);
	}
	return str;
}

/* Has this object been freshly-fetched? */
int
crawl_obj_fresh(CRAWLOBJ *obj)
{
	return obj->fresh;
}

/* Replace the information in a crawl object with a new dictionary */
int
crawl_obj_replace_(CRAWLOBJ *obj, jd_var *dict)
{
	jd_release(&(obj->info));
	jd_clone(&(obj->info), dict, 1);
	return crawl_obj_update_(obj);
}

/* Update internal members of the structured based on the info jd_var */
static int
crawl_obj_update_(CRAWLOBJ *obj)
{
	jd_var *key;
	
	obj->updated = 0;
	if(obj->info.type == VOID)
	{
		return -1;
	}
	JD_SCOPE
	{
		key = jd_get_ks(&(obj->info), "updated", 1);
		if(key->type != VOID)
		{
			obj->updated = jd_get_int(key);
		}
		key = jd_get_ks(&(obj->info), "status", 1);
		if(key->type != VOID)
		{
			obj->status = jd_get_int(key);
		}
		key = jd_get_ks(&(obj->info), "size", 1);
		if(key->type != VOID)
		{
			obj->size = jd_get_int(key);
		}
	}
	return 0;
}
