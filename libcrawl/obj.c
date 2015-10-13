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

static int crawl_obj_update_(CRAWLOBJ *obj);

CRAWLOBJ *
crawl_obj_create_(CRAWL *crawl, URI *uri)
{
	CRAWLOBJ *p;
	size_t needed;
	
	p = (CRAWLOBJ *) crawl_alloc(crawl, sizeof(CRAWLOBJ));
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
	   (p->uristr = (char *) crawl_alloc(crawl, needed)) == NULL ||
	   uri_str(p->uri, p->uristr, needed) != needed ||
	   crawl_cache_key_(crawl, p->key, p->uristr))
	{
		crawl_obj_destroy(p);
		return NULL;
	}
	p->payload = cache_uri_(crawl, p->key);
	if(!p->payload)
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
	if(!obj->crawl->cache.impl)
	{
		if(!crawl_cache_init_(obj->crawl))
		{
			return -1;
		}
	}
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
		crawl_free(obj->crawl, obj->uristr);
		crawl_free(obj->crawl, obj->payload);
		if(obj->info)
		{
			json_decref(obj->info);
		}
		crawl_free(obj->crawl, obj);
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

/* Return either a reference to the headers in the crawl object, or a deep
 * copy of them -- in either case, the caller must json_decref() once it's
 * finished with the returned dictionary.
 */
json_t *
crawl_obj_headers(CRAWLOBJ *obj, int clone)
{
	json_t *headers;
	
	if(!obj->info)
	{
		return NULL;
	}
	headers = json_object_get(obj->info, "headers");
	if(!headers)
	{
		return NULL;
	}
	if(clone)
	{
		return json_deep_copy(headers);
	}
	json_incref(headers);
	return headers;
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
	json_t *str;
	
	if(!obj->info)
	{
		return NULL;
	}
	str = json_object_get(obj->info, "type");
	if(str)
	{
		return json_string_value(str);
	}
	return NULL;
}

const char *
crawl_obj_redirect(CRAWLOBJ *obj)
{
	json_t *str;
	
	if(!obj->info)
	{
		return NULL;
	}
	str = json_object_get(obj->info, "redirect");
	if(str)
	{
		return json_string_value(str);
	}
	return NULL;
}

const char *
crawl_obj_content_location(CRAWLOBJ *obj)
{
	json_t *str;
	
	if(!obj->info)
	{
		return NULL;
	}
	str = json_object_get(obj->info, "content_location");
	if(str)
	{
		return json_string_value(str);
	}
	return NULL;
}

/* Has this object been freshly-fetched? */
int
crawl_obj_fresh(CRAWLOBJ *obj)
{
	return obj->fresh;
}

/* Replace the information in a crawl object with data from a new JSON
 * object
 */
int
crawl_obj_replace_(CRAWLOBJ *obj, const json_t *dict)
{
	json_t *p;

	p = json_deep_copy(dict);
	if(!p)
	{
		return -1;
	}
	if(obj->info)
	{
		json_decref(obj->info);
	}
	obj->info = p;
	return crawl_obj_update_(obj);
}

/* Update internal members of the structured based on the info jd_var */
static int
crawl_obj_update_(CRAWLOBJ *obj)
{
	json_t *p;
	
	obj->updated = 0;
	if(!obj->info)
	{
		return -1;
	}
	p = json_object_get(obj->info, "updated");
	if(p)
	{
		obj->updated = json_integer_value(p);
	}
	p = json_object_get(obj->info, "status");
	if(p)
	{
		obj->status = json_integer_value(p);
	}
	p = json_object_get(obj->info, "size");
	if(p)
	{
		obj->size = json_integer_value(p);
	}
	return 0;
}

/* Open the payload file for a crawl object */
FILE *
crawl_obj_open(CRAWLOBJ *obj)
{
	return obj->crawl->cache.impl->payload_open_read(&(obj->crawl->cache), obj->key);
}
