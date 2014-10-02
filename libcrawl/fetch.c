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
static int crawl_generate_info_(struct crawl_fetch_data_struct *data, jd_var *dict);

CRAWLOBJ *
crawl_fetch(CRAWL *crawl, const char *uristr)
{
	URI *uri;
	CRAWLOBJ *r;
	
	uri = uri_create_str(uristr, NULL);
	if(!uri)
	{
		return NULL;
	}
	r = crawl_fetch_uri(crawl, uri);
	uri_destroy(uri);
	return r;
}

CRAWLOBJ *
crawl_fetch_uri(CRAWL *crawl, URI *uri)
{
	struct crawl_fetch_data_struct data;
	struct tm tp;
	char modified[64];
	int error;
	jd_var dict = JD_INIT;
	struct curl_slist *headers;
	
	memset(&data, 0, sizeof(data));
	headers = NULL;

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
		if(data.now - data.cachetime < crawl->cache_min)
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
		jd_clone(&dict, &(data.obj->info), 1);
		/* Send an If-Modified-Since header */
		gmtime_r(&(data.cachetime), &tp);
		strftime(modified, sizeof(modified), "If-Modified-Since: %a, %d %b %Y %H:%M:%S GMT", &tp);
		headers = curl_slist_append(headers, modified);		
	}
	if(crawl->uri_policy)
	{
		if(crawl->uri_policy(crawl, data.obj->uri, data.obj->uristr, crawl->userdata) < 1)
		{
			if(crawl->failed)
			{
				crawl->failed(crawl, data.obj, data.cachetime, crawl->userdata);
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
		jd_release(&dict);
		crawl_obj_destroy(data.obj);
		return NULL;
	}
	if(crawl->prefetch)
	{
		crawl->prefetch(crawl, data.obj->uri, data.obj->uristr, crawl->userdata);
	}
	if(curl_easy_perform(data.ch))
	{
		if(!data.status)
		{
			/* Use 504 to indicate a low-level fetch error */
			data.status = 504;
		}
	}
	else
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
		JD_SCOPE
		{
			if(crawl_update_info_(&data))
			{
				data.rollback = 1;
				error = -1;
			}
			else
			{
				if(cache_info_write_(crawl, data.obj->key, &(data.obj->info)))
				{
					data.rollback = 1;
					error = -1;
				}
				else
				{
					data.obj->fresh = 1;
				}
			}
			if(data.rollback)
			{
				crawl_obj_replace_(data.obj, &dict);
			}
		}
	}
	free(data.headers);
	jd_release(&dict);
	if(data.rollback)
	{
		cache_close_payload_rollback_(crawl, data.obj->key, data.payload);
		/* restore info */
	}
	else
	{
		cache_close_payload_commit_(crawl, data.obj->key, data.payload);	
	}	
	curl_slist_free_all(headers);
	curl_easy_cleanup(data.ch);
	/* If we rolled back and there was nothing to roll back to, consider
	 * it an error */
	if(data.rollback && !data.cachetime)
	{
		error = -1;
	}
	if(error)
	{
		if(crawl->failed)
		{
			crawl->failed(crawl, data.obj, data.cachetime, crawl->userdata);
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
	jd_var infoblock = JD_INIT;
	jd_var *key, *value;
	int status;
	
	JD_SCOPE
	{
		if(!data->generated_info)
		{
			curl_easy_getinfo(data->ch, CURLINFO_RESPONSE_CODE, &(data->status));			
			crawl_generate_info_(data, &infoblock);
			crawl_obj_replace_(data->obj, &infoblock);
			jd_release(&infoblock);
			data->generated_info = 1;
		}
		if(data->obj->status != data->status)
		{
			key = jd_get_ks(&(data->obj->info), "status", 1);
			value = jd_niv(data->status);
			data->obj->status = data->status;
		}
		if(data->have_size)
		{
			key = jd_get_ks(&(data->obj->info), "size", 1);
			value = jd_niv(data->size);
			jd_assign(key, value);
			data->obj->size = data->size;
		}
	}
	if(!data->checkpoint_invoked && data->crawl->checkpoint)
	{
		data->checkpoint_invoked = 1;
		status = data->status;		
		if(data->crawl->checkpoint(data->crawl, data->obj, &status, data->crawl->userdata))
		{
			data->status = data->obj->status = status;
			data->rollback = 1;
			return 0;
		}
		
	}
	return 0;
}

static int
crawl_generate_info_(struct crawl_fetch_data_struct *data, jd_var *dict)
{
	jd_var *key, *value, *headers;
	char *ptr, *s, *p;
	
	jd_set_hash(dict, 8);
	JD_SCOPE
	{
		jd_retain(dict);
		key = jd_get_ks(dict, "status", 1);
		value = jd_niv(data->status);		
		jd_assign(key, value);
		if(data->have_size)
		{
			key = jd_get_ks(dict, "size", 1);
			value = jd_niv(data->size);
			jd_assign(key, value);
		}
		key = jd_get_ks(dict, "updated", 1);
		value = jd_niv(data->now);
		jd_assign(key, value);
		ptr = NULL;
		curl_easy_getinfo(data->ch, CURLINFO_EFFECTIVE_URL, &ptr);
		if(ptr)
		{
			key = jd_get_ks(dict, "location", 1);
			value = jd_nsv(ptr);
			jd_assign(key, value);
		}
		ptr = NULL;
		curl_easy_getinfo(data->ch, CURLINFO_REDIRECT_URL, &ptr);
		if(ptr)
		{
			key = jd_get_ks(dict, "redirect", 1);
			value = jd_nsv(ptr);
			jd_assign(key, value);
		}
		ptr = NULL;
		curl_easy_getinfo(data->ch, CURLINFO_CONTENT_TYPE, &ptr);
		if(ptr)
		{
			key = jd_get_ks(dict, "type", 1);
			value = jd_nsv(ptr);
			jd_assign(key, value);	
		}
		headers = jd_nhv(32);
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
				jd_assign(jd_get_ks(headers, ":", 1), jd_nsv(s));
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
			key = jd_get_ks(headers, s, 1);
			if(key->type == VOID)
			{
				jd_assign(key, jd_nav(1));
			}
			value = jd_push(key, 1);
			jd_assign(value, jd_nsv(ptr));
			s = p + 2;
		}
		key = jd_get_ks(dict, "headers", 1);
		jd_assign(key, headers);
	}
	return 0;
}
