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

#include "p_libcrawl.h"

#define MAX_HEADERS_SIZE               8192

static size_t crawl_fetch_header_(char *ptr, size_t size, size_t nmemb, void *userdata);
static size_t crawl_fetch_payload_(char *ptr, size_t size, size_t nmemb, void *userdata);
static int crawl_update_info_(struct crawl_fetch_data_struct *data);
static int crawl_generate_info_(struct crawl_fetch_data_struct *data, json_t *dict);

CRAWLOBJ *
crawl_fetch(CRAWL *crawl, const char *uristr, CRAWLSTATE state)
{
	URI *uri;
	CRAWLOBJ *r;
	
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		return NULL;
	}
	r = crawl_fetch_uri(crawl, uri, state);
	uri_destroy(uri);
	return r;
}

CRAWLOBJ *
crawl_fetch_uri(CRAWL *crawl, URI *uri, CRAWLSTATE state)
{
	struct crawl_fetch_data_struct data;
	struct tm tp;
	char modified[64];
	int error;
	json_t *dict;
	struct curl_slist *headers;
	
	memset(&data, 0, sizeof(data));
	headers = NULL;
	dict = NULL;
	data.now = time(NULL);
	data.crawl = crawl;
	data.obj = crawl_obj_create_(crawl, uri);
	if(!data.obj)
	{
		return NULL;
	}
	data.ch = curl_easy_init();
	if(crawl_obj_locate_(data.obj) == 0)
	{
		/* Object was located in the cache */
		data.cachetime = data.obj->updated;
		if(state != COS_FORCE && data.now - data.cachetime < crawl->cache_min)
		{
			/* The object hasn't reached its minimum time-to-live */
			if(crawl->unchanged)
			{
				crawl->unchanged(crawl, data.obj, data.cachetime, crawl->userdata);
			}
		    return data.obj;
		}
		/* Store a copy of the object dictionary to allow rolling it back without
		 * re-reading from disk.
		 */
		dict = json_deep_copy(data.obj->info);
		/* Send an If-Modified-Since header */
		if(state != COS_FORCE)
		{
			gmtime_r(&(data.cachetime), &tp);
			strftime(modified, sizeof(modified), "If-Modified-Since: %a, %d %b %Y %H:%M:%S GMT", &tp);
			headers = curl_slist_append(headers, modified);
		}
	}
	if(crawl->uri_policy)
	{
		state = crawl->uri_policy(crawl, data.obj->uri, data.obj->uristr, crawl->userdata);
		if(state != COS_ACCEPTED)
		{
			if(crawl->failed)
			{
				crawl->failed(crawl, data.obj, data.cachetime, crawl->userdata, state);
			}		
			crawl_obj_destroy(data.obj);
			return NULL;
		}
	}
	/* Set the Accept header */
	if(crawl->accept)
	{
		headers = curl_slist_append(headers, crawl->accept);	
	}
	/* Set the User-Agent header */
	if(crawl->ua)
	{
		curl_slist_append(headers, crawl->ua);
	}
	curl_easy_setopt(data.ch, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(data.ch, CURLOPT_URL, data.obj->uristr);
	curl_easy_setopt(data.ch, CURLOPT_WRITEFUNCTION, crawl_fetch_payload_);
	curl_easy_setopt(data.ch, CURLOPT_WRITEDATA, (void *) &data);
	curl_easy_setopt(data.ch, CURLOPT_HEADERFUNCTION, crawl_fetch_header_);
	curl_easy_setopt(data.ch, CURLOPT_HEADERDATA, (void *) &data);
	curl_easy_setopt(data.ch, CURLOPT_FOLLOWLOCATION, 0);
	curl_easy_setopt(data.ch, CURLOPT_VERBOSE, crawl->verbose);
	curl_easy_setopt(data.ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(data.ch, CURLOPT_CONNECTTIMEOUT, 30);
	curl_easy_setopt(data.ch, CURLOPT_TIMEOUT, 120);
	error = 0;
	data.payload = cache_open_payload_write_(crawl, data.obj->key);
	if(!data.payload)
	{
		json_decref(dict);		
		crawl_obj_destroy(data.obj);
		return NULL;
	}
	if(crawl->prefetch)
	{
		crawl->prefetch(crawl, data.obj->uri, data.obj->uristr, crawl->userdata);
	}
	data.obj->state = COS_NEW;
	if(curl_easy_perform(data.ch))
	{
		if(!data.status)
		{
			/* Use 504 to indicate a low-level fetch error */
			data.status = 504;
			data.obj->state = COS_FAILED;
		}
	}
	else if(!data.status)
	{
		/* In the event that there was no payload written, data.status will be
		 * unset, so ensure that it is
		 */
		curl_easy_getinfo(data.ch, CURLINFO_RESPONSE_CODE, &(data.status));
	}
	if(data.cachetime && data.status == 304)
	{
		/* Not modified; rollback with successful return */
		data.rollback = 1;
	}
	else if(data.status >= 500)
	{
		/* rollback if there's already a cached version */
		if(data.cachetime)
		{
			data.rollback = 1;
		}
	}
	if(!data.rollback)
	{
		if(crawl_update_info_(&data))
		{
			data.rollback = 1;
			error = -1;
			data.obj->state = COS_FAILED;
		}
		else
		{
			if(cache_info_write_(crawl, data.obj->key, data.obj->info))
			{
				data.rollback = 1;
				error = -1;
				data.obj->state = COS_FAILED;
			}
			else
			{
				data.obj->fresh = 1;
			}
		}
		if(data.rollback)
		{
			crawl_obj_replace_(data.obj, dict);
		}
	}
	free(data.headers);
	json_decref(dict);
	dict = NULL;
	if(data.rollback)
	{
		cache_close_payload_rollback_(crawl, data.obj->key, data.payload);
	}
	else
	{
		/* At this point, data.obj->state may already have been set to
		 * COS_SKIPPED_COMMIT, so we shouldn't override that if so
		 */
		if(data.obj->state != COS_SKIPPED_COMMIT)
		{
			data.obj->state = COS_ACCEPTED;
		}
		cache_close_payload_commit_(crawl, data.obj->key, data.payload, data.obj);
	}	
	curl_slist_free_all(headers);
	curl_easy_cleanup(data.ch);
	/* If we rolled back and there was nothing to roll back to, consider
	 * it an error */
	if(data.rollback && !data.cachetime)
	{
		error = -1;
		data.obj->state = COS_FAILED;
	}
	if(error)
	{
		if(data.obj->state == COS_NEW)
		{
			data.obj->state = COS_FAILED;
		}
		if(crawl->failed)
		{			
			crawl->failed(crawl, data.obj, data.cachetime, crawl->userdata, data.obj->state);
		}
		crawl_obj_destroy(data.obj);
		return NULL;
	}
	if(!data.obj->fresh)
	{
		if(crawl->unchanged)
		{
			crawl->unchanged(crawl, data.obj, data.cachetime, crawl->userdata);
		}
		return data.obj;
	}
	if(crawl->updated)
	{
		crawl->updated(crawl, data.obj, data.cachetime, crawl->userdata);
	}
	return data.obj;
}

static size_t
crawl_fetch_header_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct crawl_fetch_data_struct *data;
	size_t n;
	char *p;
	
	data = (struct crawl_fetch_data_struct *) userdata;
	size *= nmemb;
	n = data->headers_size;
	while(n < data->headers_len + size + 1)
	{
		n += HEADER_ALLOC_BLOCK;
	}
	if(n != data->headers_size)
	{
		if(n > MAX_HEADERS_SIZE)
		{
			return 0;
		}
		p = (char *) realloc(data->headers, n);
		if(!p)
		{
			return 0;
		}
		data->headers_size = n;
		data->headers = p;
	}
	memcpy(&(data->headers[data->headers_len]), ptr, size);
	data->headers_len += size;
	data->headers[data->headers_len] = 0;
	return size;
}

static size_t
crawl_fetch_payload_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct crawl_fetch_data_struct *data;
	
	data = (struct crawl_fetch_data_struct *) userdata;    
	if(crawl_update_info_(data))
	{
		return 0;
	}
	if(data->rollback)
	{
		return 0;
	}
	if(!data->have_size)
	{
		data->have_size = 1;
		data->size = 0;
	}
	if(fwrite(ptr, size, nmemb, data->payload) != nmemb)
	{
		return 0;
	}
	size *= nmemb;
	data->size += size;
	return size;
}

/* Create or update the object's dictionary */
static int
crawl_update_info_(struct crawl_fetch_data_struct *data)
{
	json_t *infoblock;
	int status;
	CRAWLSTATE state;

	if(!data->generated_info)
	{
		infoblock = json_object();

		curl_easy_getinfo(data->ch, CURLINFO_RESPONSE_CODE, &(data->status));
		crawl_generate_info_(data, infoblock);
		crawl_obj_replace_(data->obj, infoblock);
		json_decref(infoblock);
		data->generated_info = 1;
	}
	if(data->obj->status != data->status)
	{
		json_object_set_new(data->obj->info, "status", json_integer(data->status));
		data->obj->status = data->status;
	}
	if(data->have_size)
	{
		json_object_set_new(data->obj->info, "size", json_integer(data->size));
		data->obj->size = data->size;
	}
	/* XXX this block ought to be refactored out into a separate function */
	if(!data->checkpoint_invoked && data->crawl->checkpoint)
	{
		data->checkpoint_invoked = 1;
		status = data->status;		
		state = data->crawl->checkpoint(data->crawl, data->obj, &status, data->crawl->userdata);
		if(state == COS_SKIPPED_COMMIT)
		{
			/* Update the HTTP status and state, but don't roll back the
			 * object, instead allow processing to proceed (such as
			 * following redirects in response headers).
			 */
			data->status = data->obj->status = status;
			data->obj->state = state;
		}
		else if(state != COS_ACCEPTED)
		{
			/* Any other non-ACCEPTED state results in a rollback */
			data->status = data->obj->status = status;
			data->obj->state = state;
			data->rollback = 1;
			return 0;
		}	   
	}
	return 0;
}

static int
is_same_origin(URI_INFO *a, URI_INFO *b)
{
	if((a->scheme && !b->scheme) ||
	   (!a->scheme && b->scheme) ||
	   (a->scheme && b->scheme && strcasecmp(a->scheme, b->scheme)))
	{
		return 0;
	}
	if(!a->port && a->scheme)
	{
		if(!strcasecmp(a->scheme, "http"))
		{
			a->port = 80;
		}
		else if(!strcasecmp(a->scheme, "https"))
		{
			a->port = 443;
		}
	}
	if(!b->port && b->scheme)
	{
		if(!strcasecmp(b->scheme, "http"))
		{
			b->port = 80;
		}
		else if(!strcasecmp(b->scheme, "https"))
		{
			b->port = 443;
		}
	}
	if(a->port != b->port)
	{
		return 0;
	}		
	if((a->host && !b->host) ||
	   (!a->host && b->host) ||
	   (a->host && b->host && strcasecmp(a->host, b->host)))
	{
		return 0;
	}
	return 1;
}

/* Add the canonical form of a URL (provided as a string) to a JSON object,
 * possibly applying a same-origin check
 */
static int
set_dict_url(CRAWLOBJ *obj, json_t *dict, const char *key, const char *location, int same_origin)
{
	URI *uri;
	URI_INFO *a, *b;
	char *p;
	int r;

	uri = uri_create_str(location, obj->uri);
	if(!uri)
	{
		return -1;
	}
	if(same_origin)
	{
		a = uri_info(obj->uri);
		b = uri_info(uri);
		r = is_same_origin(a, b);
		uri_info_destroy(a);
		uri_info_destroy(b);
		if(!r)
		{
			uri_destroy(uri);
			return -1;
		}
	}
	p = uri_stralloc(uri);
	json_object_set_new(dict, key, json_string(p));
	free(p);
	uri_destroy(uri);
	return 0;
}

/* Populate a JSON object contaning information about the request
 * that was preformed.
 */
static int
crawl_generate_info_(struct crawl_fetch_data_struct *data, json_t *dict)
{
	json_t *headers, *values;
	char *ptr, *s, *p;
	const char *t;

	json_object_set_new(dict, "status", json_integer(data->status));
	if(data->have_size)
	{
		json_object_set_new(dict, "status", json_integer(data->size));
	}
	json_object_set_new(dict, "updated", json_integer(data->now));
	ptr = NULL;
	curl_easy_getinfo(data->ch, CURLINFO_EFFECTIVE_URL, &ptr);
	if(ptr)
	{
		t = strchr(ptr, '#');
		if(t)
		{
			/* Older versions of libjansson don't include json_stringn() */
			s = strdup(ptr);
			s[t - ptr] = 0;
			/* If there's a fragment, store only the characters prior to it
			 * (because fragments are a facet of user-agent behaviour, they
			 * don't make any sense in Location or Content-Location headers)
			 */
			json_object_set_new(dict, "location", json_string(s));
			json_object_set_new(dict, "content_location", json_string(s));
			free(s);
		}
		else
		{		   
			json_object_set_new(dict, "location", json_string(ptr));
			json_object_set_new(dict, "content_location", json_string(ptr));
		}
	}
	ptr = NULL;
	curl_easy_getinfo(data->ch, CURLINFO_CONTENT_TYPE, &ptr);
	if(ptr)
	{
		json_object_set_new(dict, "type", json_string(ptr));
	}
	headers = json_object();
	for(s = data->headers; s; )
	{
		p = strchr(s, '\n');
		if(!p)
		{
			break;
		}
		if(s == p)
		{
			s = p + 1;
			continue;
		}
		*p = 0;
		p--;
		if(*p == '\r')
		{
			*p = 0;
		}
		if(s == data->headers)
		{
			/* The special key ':' is used for the HTTP status line */
			json_object_set_new(headers, ":", json_string(s));
			s = p + 2;
			continue;
		}
		ptr = strchr(s, ':');
		if(!ptr)
		{
			s = p + 2;
			continue;
		}			
		*ptr = 0;
		ptr++;
		if(isspace(*ptr))
		{
			ptr++;
		}
		if(!strcasecmp(s, "location"))
		{			
			set_dict_url(data->obj, dict, "redirect", ptr, 0);
		}
		if(!strcasecmp(s, "content-location"))
		{
			set_dict_url(data->obj, dict, "content_location", ptr, 1);
		}
		values = json_object_get(headers, s);
		if(!values)
		{
			values = json_array();
			json_object_set_new(headers, s, values);
		}
		json_array_append_new(values, json_string(ptr));
		s = p + 2;
	}
	json_object_set_new(dict, "headers", headers);
	return 0;
}
