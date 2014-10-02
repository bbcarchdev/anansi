/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC
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

#include "libs3client.h"

struct s3cache_data_struct
{
	const char *bucket;
	char *host;
	char *access;
	char *secret;
	/* Temporary state */
	char *url;
	size_t urlsize;
	char *resource;
	size_t resourcesize;
	char *buf;
	size_t size;
	size_t pos;
};

static int urldecode(char *dest, const char *src, size_t len);

static size_t s3cache_write_buf_(char *ptr, size_t size, size_t nmemb, void *userdata);
static size_t s3cache_write_null_(char *ptr, size_t size, size_t nmemb, void *userdata);
static int s3cache_copy_url_(CRAWLCACHE *cache, const CACHEKEY key, const char *suffix);

static unsigned long s3cache_init_(CRAWLCACHE *cache);
static unsigned long s3cache_done_(CRAWLCACHE *cache);
static FILE *s3cache_open_write_(CRAWLCACHE *cache, const CACHEKEY key);
static int s3cache_close_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
static int s3cache_close_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
static int s3cache_info_read_(CRAWLCACHE *cache, const CACHEKEY key, jd_var *info);
static int s3cache_info_write_(CRAWLCACHE *cache, const CACHEKEY key, jd_var *info);
static char *s3cache_uri_(CRAWLCACHE *cache, const CACHEKEY key);
static int s3cache_set_username_(CRAWLCACHE *cache, const char *username);
static int s3cache_set_password_(CRAWLCACHE *cache, const char *password);
static int s3cache_set_endpoint_(CRAWLCACHE *cache, const char *endpoint);

static const CRAWLCACHEIMPL s3cache_impl = {
	NULL,
	s3cache_init_,
	s3cache_done_,
	s3cache_open_write_,
	s3cache_close_rollback_,
	s3cache_close_commit_,
	s3cache_info_read_,
	s3cache_info_write_,
	s3cache_uri_,
	s3cache_set_username_,
	s3cache_set_password_,
	s3cache_set_endpoint_,
};

const CRAWLCACHEIMPL *s3cache = &s3cache_impl;

static unsigned long
s3cache_init_(CRAWLCACHE *cache)
{
	struct s3cache_data_struct *data;
	const char *t;

	data = (struct s3cache_data_struct *) calloc(1, sizeof(struct s3cache_data_struct));
	if(!data)
	{
		return 0;
	}
	data->host = strdup("s3.amazonaws.com");
	if(!data->host)
	{
		free(data);
		return 0;
	}
	if(!cache->crawl->uri->host)
	{
		return 0;
	}
	data->bucket = cache->crawl->uri->host;
	if(cache->crawl->uri->auth)
	{		
		t = strchr(cache->crawl->uri->auth, ':');
		if(t)
		{
			data->access = (char *) calloc(1, t - cache->crawl->uri->auth + 1);
			if(!data->access)
			{
				free(data);
				return 0;
			}
			urldecode(data->access, cache->crawl->uri->auth, t - cache->crawl->uri->auth);
			data->access[t - cache->crawl->uri->auth] = 0;
			t++;
			data->secret = (char *) calloc(1, strlen(t) + 1);
			if(!data->secret)
			{
				free(data->access);
				free(data);
				return 0;
			}
			urldecode(data->secret, t, strlen(t));
		}
		else
		{
			data->access = strdup(cache->crawl->uri->auth);
			data->secret = NULL;
		}
	}
	cache->data = data;
	return 1;
}

unsigned long
s3cache_done_(CRAWLCACHE *cache)
{
	struct s3cache_data_struct *data;

	data = (struct s3cache_data_struct *) cache->data;
	if(data)
	{
		free(data->host);
		free(data->access);
		free(data->secret);
		free(data->buf);
		free(data);
		cache->data = NULL;
	}
	return 0;
}

static FILE *
s3cache_open_write_(CRAWLCACHE *cache, const CACHEKEY key)
{
	(void) cache;
	(void) key;

	return tmpfile();
}

static int
s3cache_close_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f)
{
	(void) cache;
	(void) key;
	
	fclose(f);
	return 0;
}

static int
s3cache_close_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f)
{
	struct s3cache_data_struct *data;
	int e;
	time_t now;
	struct tm tm;
	char datebuf[64];
	CURL *ch;
	long status;
	struct curl_slist *headers;
	off_t offset;

	if(!f)
	{
		errno = EINVAL;
		return -1;
	}	
	offset = ftello(f);
	rewind(f);
	e = 0;
	data = (struct s3cache_data_struct *) cache->data;
	status = 0;
	s3cache_copy_url_(cache, key, CACHE_PAYLOAD_SUFFIX);
	now = time(NULL);
	gmtime_r(&now, &tm);
	strcpy(datebuf, "Date: ");
	strftime(&(datebuf[6]), 58, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	ch = curl_easy_init();			
	curl_easy_setopt(ch, CURLOPT_URL, data->url);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
	curl_easy_setopt(ch, CURLOPT_READDATA, f);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE_LARGE, (curl_off_t) offset);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
	headers = NULL;
	headers = curl_slist_append(headers, datebuf);
	headers = curl_slist_append(headers, "Expect: 100-continue");
	if(data->access && data->secret)
	{
		headers = s3_sign("PUT", data->resource, data->access, data->secret, headers);
	}
	if(headers)
	{
		curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	}
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, s3cache_write_null_);
	e = curl_easy_perform(ch);
	if(e == CURLE_OK)
	{
		status = 0;
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status != 200)
		{
			e = -1;
		}			
	}
	else
	{
		e = -1;
	}
	curl_easy_cleanup(ch);
	fclose(f);
	return e;
}

static int
s3cache_info_read_(CRAWLCACHE *cache, const CACHEKEY key, jd_var *dict)
{
	struct s3cache_data_struct *data;
	CURL *ch;
	struct curl_slist *headers = NULL;
	time_t now;
	struct tm tm;
	char datebuf[64];
	int e;
	long status;

	data = (struct s3cache_data_struct *) cache->data;
	status = 0;
	s3cache_copy_url_(cache, key, CACHE_INFO_SUFFIX);
	ch = curl_easy_init();
	curl_easy_setopt(ch, CURLOPT_URL, data->url);
	curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
	now = time(NULL);
	gmtime_r(&now, &tm);
	strcpy(datebuf, "Date: ");
	strftime(&(datebuf[6]), 58, "%a, %d %b %Y %H:%M:%S GMT", &tm);
	headers = curl_slist_append(headers, datebuf);
	if(data->access && data->secret)
	{
		headers = s3_sign("GET", data->resource, data->access, data->secret, headers);
	}
	if(headers)
	{
		curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	}
	data->pos = 0;
	curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, s3cache_write_buf_);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) data);
	e = curl_easy_perform(ch);
	if(e != CURLE_OK || !data->buf)
	{
		curl_easy_cleanup(ch);
		return -1;
	}
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status != 200)
	{
		curl_easy_cleanup(ch);
		return -1;
	}
	curl_easy_cleanup(ch);
	JD_SCOPE
	{
		jd_release(dict);
		jd_from_jsons(dict, data->buf);
	}
	return 0;
}

static int
s3cache_info_write_(CRAWLCACHE *cache, const CACHEKEY key, jd_var *dict)
{
	jd_var json = JD_INIT;
	const char *p;
	size_t len;
	volatile int e;
	long status;
	time_t now;
	struct tm tm;
	char datebuf[64];
	CURL *ch;
	struct curl_slist *headers;
	struct s3cache_data_struct *data;

	e = 0;
	data = (struct s3cache_data_struct *) cache->data;
	s3cache_copy_url_(cache, key, CACHE_INFO_SUFFIX);
	
	JD_SCOPE
	{
		jd_to_json(&json, dict);
		p = jd_bytes(&json, &len);
		len--;
		if(p)
		{
			now = time(NULL);
			gmtime_r(&now, &tm);
			strcpy(datebuf, "Date: ");
			strftime(&(datebuf[6]), 58, "%a, %d %b %Y %H:%M:%S GMT", &tm);
			ch = curl_easy_init();			
			curl_easy_setopt(ch, CURLOPT_URL, data->url);
			curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
			curl_easy_setopt(ch, CURLOPT_POSTFIELDS, p);
			curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, len);
			curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
			headers = NULL;
			headers = curl_slist_append(headers, datebuf);
			headers = curl_slist_append(headers, "Content-Type: application/json");
			if(data->access && data->secret)
			{
				headers = s3_sign("PUT", data->resource, data->access, data->secret, headers);
			}
			if(headers)
			{
				curl_easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
			}
			curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, s3cache_write_null_);
			curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "PUT");
			e = curl_easy_perform(ch);
			if(e == CURLE_OK)
			{
				status = 0;
				curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
				if(status != 200)
				{
					e = -1;
				}			
			}
			else
			{
				e = -1;
			}
			curl_easy_cleanup(ch);
		}
		else
		{
			e = -1;
		}
	}
	return e;
}

static char *s3cache_uri_(CRAWLCACHE *cache, const CACHEKEY key)
{
	char *uri, *p;
	size_t needed;
	struct s3cache_data_struct *data;

	data = (struct s3cache_data_struct *) cache->data;
	/* s3://bucket/path/key.payload */
	if(cache->crawl->cachepath && cache->crawl->cachepath[0])
	{
		needed = 5 + strlen(data->bucket) + strlen(cache->crawl->cachepath) + 2 + strlen(key) + 8 + 1;
	}
	else
	{
		needed = 5 + strlen(data->bucket) + 1 + strlen(key) + 8 + 1;
	}
	uri = (char *) calloc(1, needed);
	if(!uri)
	{
		return NULL;
	}
	p = uri;
	strcpy(p, "s3://");
	p += 5;
	strcpy(p, data->bucket);
	p += strlen(data->bucket);
	if(cache->crawl->cachepath && cache->crawl->cachepath[0])
	{
		if(cache->crawl->cachepath[0] != '/')
		{
			*p = '/';
			p++;
		}
		strcpy(p, cache->crawl->cachepath);
		p += strlen(cache->crawl->cachepath) - 1;
		if(*p != '/')
		{
			p++;
			*p = '/';
		}
		p++;
	}
	else
	{
		*p = '/';
		p++;
	}
	strcpy(p, key);
	p += strlen(key);
	strcpy(p, ".payload");
	return uri;
}

static int 
s3cache_set_username_(CRAWLCACHE *cache, const char *username)
{
	struct s3cache_data_struct *data;
	char *p;

	data = (struct s3cache_data_struct *) cache->data;
	if(username)
	{
		p = strdup(username);
		if(!p)
		{
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(data->access);
	data->access = p;
	return 0;
}

static int 
s3cache_set_password_(CRAWLCACHE *cache, const char *password)
{
	struct s3cache_data_struct *data;
	char *p;

	data = (struct s3cache_data_struct *) cache->data;
	if(password)
	{
		p = strdup(password);
		if(!p)
		{
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(data->secret);
	data->secret = p;
	return 0;
}

static int 
s3cache_set_endpoint_(CRAWLCACHE *cache, const char *endpoint)
{
	struct s3cache_data_struct *data;
	char *p;

	data = (struct s3cache_data_struct *) cache->data;
	if(endpoint)
	{
		p = strdup(endpoint);
		if(!p)
		{
			return -1;
		}
	}
	else
	{
		p = NULL;
	}
	free(data->host);
	data->host = p;
	return 0;
}

/* Callback invoked by cURL when data is received */
static size_t
s3cache_write_buf_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	struct s3cache_data_struct *data;
	char *p;

	data = (struct s3cache_data_struct *) userdata;
	size *= nmemb;
	if(data->pos + size >= data->size)
	{
		p = (char *) realloc(data->buf, data->pos + size + 1);
		if(!p)
		{
			return 0;
		}
		data->buf = p;
		data->size = data->pos + size + 1;
	}
	memcpy(&(data->buf[data->pos]), ptr, size);
	data->pos += size;
	data->buf[data->pos] = 0;
	return size;
}

/* Generate the URL and resource path for a given key and type */
static int
s3cache_copy_url_(CRAWLCACHE *cache, const CACHEKEY key, const char *type)
{
	struct s3cache_data_struct *data;
	size_t needed;
	char *p;

	data = (struct s3cache_data_struct *) cache->data;
	
	/* [/path]/key.type */
	if(cache->crawl->cachepath)
	{
		needed = strlen(data->bucket) + 2 + strlen(cache->crawl->cachepath) + strlen(key) + strlen(type) + 3;
	}
	else
	{
		needed = strlen(data->bucket) + 2 + strlen(key) + strlen(type) + 2;
	}
	if(data->resourcesize < needed)
	{
		p = (char *) realloc(data->resource, needed);
		if(!p)
		{
			return -1;
		}
		data->resourcesize = needed;
		data->resource = p;
	}
	else
	{
		p = data->resource;
	}
	*p = '/';
	p++;
	strcpy(p, data->bucket);
	p += strlen(data->bucket);
	if(cache->crawl->cachepath && cache->crawl->cachepath[0])
	{
		if(cache->crawl->cachepath[0] != '/')
		{
			*p = '/';
			p++;
		}
		strcpy(p, cache->crawl->cachepath);
		p += strlen(cache->crawl->cachepath) - 1;
		if(*p != '/')
		{
			p++;
			*p = '/';
		}
		else
		{
			p++;
		}
	}
	else
	{
		*p = '/';
		p++;
	}
	strcpy(p, key);
	p = strchr(p, 0);
	*p = '.';
	p++;
	strcpy(p, type);
	
	needed += 7 + strlen(data->host);
	if(data->urlsize < needed)
	{
		p = (char *) realloc(data->url, needed);
		if(!p)
		{
			return -1;
		}
		data->url = p;
		data->urlsize = needed;
	}
	else
	{
		p = data->url;
	}
	strcpy(p, "http://");
	p += 7;
	strcpy(p, data->host);
	p += strlen(data->host);	
	strcpy(p, data->resource);
	return 0;
}

static size_t 
s3cache_write_null_(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	(void) ptr;
	(void) userdata;

	return size * nmemb;
}

static int
urldecode(char *dest, const char *src, size_t len)
{
	long l;
	char hbuf[3];

	while(*src && len)
	{
		if(*src == '%' && len >= 3 && isxdigit(src[1]) && isxdigit(src[2]))
		{			
			hbuf[0] = src[1];
			hbuf[1] = src[2];
			hbuf[2] = 0;
			l = strtol(hbuf, NULL, 16);
			*dest = (char) ((unsigned char) l);
			dest++;
			src += 3;
			len -= 3;
		}
		else
		{
			*dest = *src;
			dest++;
			src++;
			len--;
		}
	}
	return 0;
}

/*
static int
s3cache_create_dirs_(CRAWL *crawl, const char *path)
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
		}
		if(mkdir(crawl->cachetmp, 0777))
		{
			if(errno == EEXIST)
			{
				continue;
			}
		}
	}
	return 0;
}
*/

/*
static int
s3cache_copy_uri_(CRAWL *crawl, const CACHEKEY key, const char *type, int temporary)
{
	size_t needed;
	char *p;

	needed = s3cache_filename_(crawl, key, type, NULL, 0, 1);
	if(needed > crawl->cachefile_len)
	{
		p = (char *) realloc(crawl->cachefile, needed);
		if(!p)
		{
		    return -1;
		}
		crawl->cachefile = p;
		p = (char *) realloc(crawl->cachetmp, needed);
		if(!p)
		{
		    return -1;
		}
		crawl->cachetmp = p;
		crawl->cachefile_len = needed;
	}
	if(s3cache_filename_(crawl, key, type, crawl->cachefile, needed, temporary) > needed)
	{
		return -1;
	}
	return 0;
}
*/
