/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014-2015 BBC
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
	S3BUCKET *bucket;
	/* Temporary state */
	char *path;
	size_t pathsize;
	char *buf;
	size_t size;
	size_t pos;
};

static int urldecode(char *dest, const char *src, size_t len);

static size_t s3cache_write_buf_(char *ptr, size_t size, size_t nmemb, void *userdata);
static size_t s3cache_write_null_(char *ptr, size_t size, size_t nmemb, void *userdata);
static int s3cache_copy_path_(CRAWLCACHE *cache, const CACHEKEY key, const char *suffix);

static unsigned long s3cache_init_(CRAWLCACHE *cache);
static unsigned long s3cache_done_(CRAWLCACHE *cache);
static FILE *s3cache_open_write_(CRAWLCACHE *cache, const CACHEKEY key);
static FILE *s3cache_open_read_(CRAWLCACHE *cache, const CACHEKEY key);
static int s3cache_close_rollback_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
static int s3cache_close_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f, CRAWLOBJ *obj);
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
	s3cache_open_read_,
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
	char *p;

	data = (struct s3cache_data_struct *) calloc(1, sizeof(struct s3cache_data_struct));
	if(!data)
	{
		return 0;
	}
	if(!cache->crawl->uri->host)
	{
		return 0;
	}
	crawl_log_(cache->crawl, LOG_DEBUG, "S3: initialising cache at <s3://%s>\n", cache->crawl->uri->host);
	data->bucket = s3_create(cache->crawl->uri->host);
	if(!data->bucket)
	{
		free(data);
		return 0;
	}
	if(cache->crawl->uri->auth)
	{		
		t = strchr(cache->crawl->uri->auth, ':');
		if(t)
		{
			p = (char *) calloc(1, t - cache->crawl->uri->auth + 1);
			if(!p)
			{
				s3_destroy(data->bucket);
				free(data);
				return 0;
			}
			urldecode(p, cache->crawl->uri->auth, t - cache->crawl->uri->auth);
			p[t - cache->crawl->uri->auth] = 0;
			s3_set_access(data->bucket, p);
			free(p);			
			t++;
			p = (char *) calloc(1, strlen(t) + 1);
			if(!p)
			{
				s3_destroy(data->bucket);
				free(data);
				return 0;
			}
			urldecode(p, t, strlen(t));
			s3_set_secret(data->bucket, p);
			free(p);
		}
		else
		{
			s3_set_access(data->bucket, cache->crawl->uri->auth);
		}
	}
	if(cache->crawl->uri->path)
	{
		s3_set_basepath(data->bucket, cache->crawl->uri->path);
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
		if(data->bucket)
		{
			s3_destroy(data->bucket);
		}
		free(data->path);
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

static FILE *
s3cache_open_read_(CRAWLCACHE *cache, const CACHEKEY key)
{
	S3REQUEST *req;
	FILE *f;
	struct s3cache_data_struct *data;
	int e;
	CURL *ch;
	long status;
	
	f = tmpfile();
	if(!f)
	{
		crawl_log_(cache->crawl, LOG_ERR, MSG_E_S3_TMPFILE ": %s\n", strerror(errno));
		return NULL;
	}
	e = 0;
	data = (struct s3cache_data_struct *) cache->data;
	status = 0;
	s3cache_copy_path_(cache, key, CACHE_PAYLOAD_SUFFIX);
	req = s3_request_create(data->bucket, data->path, "GET");
	crawl_log_(cache->crawl, LOG_DEBUG, "S3: fetching <%s>\n", data->path);
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
	if(!s3_request_perform(req))
	{
		status = 0;
		curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
		if(status != 200)
		{
			crawl_log_(cache->crawl, LOG_ERR, MSG_E_S3_HTTP ": <%s>: HTTP status %d\n", data->path, status);
			e = -1;
		}			
	}
	else
	{
		e = -1;
	}
	s3_request_destroy(req);
	if(e)
	{
		fclose(f);
		return NULL;
	}
	rewind(f);
	return f;
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
s3cache_close_commit_(CRAWLCACHE *cache, const CACHEKEY key, FILE *f, CRAWLOBJ *obj)
{
	S3REQUEST *req;
	struct s3cache_data_struct *data;
	int e;
	CURL *ch;
	long status;
	off_t offset;
	struct curl_slist *headers;
	const char *t;
	char *buf;

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
	s3cache_copy_path_(cache, key, CACHE_PAYLOAD_SUFFIX);
	req = s3_request_create(data->bucket, data->path, "PUT");
	crawl_log_(cache->crawl, LOG_DEBUG, "Uploading payload to <%s>\n", data->path);
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_READDATA, f);
	curl_easy_setopt(ch, CURLOPT_INFILESIZE_LARGE, (curl_off_t) offset);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, s3cache_write_null_);
	curl_easy_setopt(ch, CURLOPT_UPLOAD, 1);
    headers = curl_slist_append(s3_request_headers(req), "Expect: 100-continue");
	t = crawl_obj_type(obj);
	if(t)
	{
		buf = (char *) malloc(strlen(t) + 15);
		strcpy(buf, "Content-Type: ");
		strcpy(&(buf[14]), t);
		headers = curl_slist_append(headers, buf);
		free(buf);
	}
	t = crawl_obj_content_location(obj);
	if(t)
	{
		buf = (char *) malloc(strlen(t) + 19);
		strcpy(buf, "Content-Location: ");
		strcpy(&(buf[18]), t);
		headers = curl_slist_append(headers, buf);
		free(buf);
	}
	s3_request_set_headers(req, headers);
	if(!s3_request_perform(req))
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
	s3_request_destroy(req);
	fclose(f);
	return e;
}

static int
s3cache_info_read_(CRAWLCACHE *cache, const CACHEKEY key, jd_var *dict)
{
	S3REQUEST *req;
	struct s3cache_data_struct *data;
	CURL *ch;
	long status;

	data = (struct s3cache_data_struct *) cache->data;
	status = 0;
	data->pos = 0;
	s3cache_copy_path_(cache, key, CACHE_INFO_SUFFIX);
	req = s3_request_create(data->bucket, data->path, "GET");
	ch = s3_request_curl(req);
	curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
	curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, s3cache_write_buf_);
	curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *) data);
	if(s3_request_perform(req) || !data->buf)
	{
		s3_request_destroy(req);
		return -1;
	}
	curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &status);
	if(status != 200)
	{
		curl_easy_cleanup(ch);
		return -1;
	}
	s3_request_destroy(req);
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
	S3REQUEST *req;
	jd_var json = JD_INIT;
	const char *p;
	size_t len;
	volatile int e;
	long status;
	CURL *ch;
	struct curl_slist *headers;
	struct s3cache_data_struct *data;

	e = 0;
	data = (struct s3cache_data_struct *) cache->data;
	s3cache_copy_path_(cache, key, CACHE_INFO_SUFFIX);
	
	JD_SCOPE
	{
		jd_to_json(&json, dict);
		p = jd_bytes(&json, &len);
		len--;
		if(p)
		{
			req = s3_request_create(data->bucket, data->path, "PUT");
			ch = s3_request_curl(req);
			curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
			curl_easy_setopt(ch, CURLOPT_POSTFIELDS, p);
			curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, len);
			curl_easy_setopt(ch, CURLOPT_VERBOSE, cache->crawl->verbose);
			curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, s3cache_write_null_);
			headers = s3_request_headers(req);
			headers = curl_slist_append(headers, "Content-Type: application/json");
			s3_request_set_headers(req, headers);
			if(!s3_request_perform(req))
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
			s3_request_destroy(req);
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
	const char *bucket, *path;
	char *uri, *p;
	size_t needed;
	struct s3cache_data_struct *data;

	data = (struct s3cache_data_struct *) cache->data;
	bucket = cache->crawl->uri->host;
	path = cache->crawl->uri->path;
	if(path)
	{
		while(*path == '/')
		{
			path++;
		}
		if(!*path)
		{
			path = NULL;
		}
	}
	/* s3://bucket/path/key */
	needed = 5 + strlen(bucket) + (path ? strlen(path) : 0) + 2 + strlen(key) + 1;
	uri = (char *) calloc(1, needed);
	if(!uri)
	{
		return NULL;
	}
	p = uri;
	strcpy(p, "s3://");
	p += 5;
	strcpy(p, bucket);
	p += strlen(bucket);
	*p = '/';
	if(path)
	{
		strcpy(p, path);
		p += strlen(path) - 1;
		if(*p != '/')
		{
			p++;
			*p = '/';
		}
		p++;
	}
	strcpy(p, key);
	p += strlen(key);
	return uri;
}

static int 
s3cache_set_username_(CRAWLCACHE *cache, const char *username)
{
	struct s3cache_data_struct *data;

	data = (struct s3cache_data_struct *) cache->data;
	if(!data->bucket)
	{
		errno = EPERM;
		return -1;
	}
	return s3_set_access(data->bucket, username);
}

static int 
s3cache_set_password_(CRAWLCACHE *cache, const char *password)
{
	struct s3cache_data_struct *data;
	
	data = (struct s3cache_data_struct *) cache->data;
	if(!data->bucket)
	{
		errno = EPERM;
		return -1;
	}
	return s3_set_secret(data->bucket, password);
}

static int 
s3cache_set_endpoint_(CRAWLCACHE *cache, const char *endpoint)
{
	struct s3cache_data_struct *data;
	
	data = (struct s3cache_data_struct *) cache->data;
	if(!data->bucket)
	{
		errno = EPERM;
		return -1;
	}
	return s3_set_endpoint(data->bucket, endpoint);
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
s3cache_copy_path_(CRAWLCACHE *cache, const CACHEKEY key, const char *type)
{
	struct s3cache_data_struct *data;
	size_t needed;
	char *p;

	data = (struct s3cache_data_struct *) cache->data;
	
	/* /key.type */
	needed = 1 + strlen(key) + 1 + strlen(type) + 1;
	if(data->pathsize < needed)
	{
		p = (char *) realloc(data->path, needed);
		if(!p)
		{
			return -1;
		}
		data->pathsize = needed;
		data->path = p;
	}
	else
	{
		p = data->path;
	}
	*p = '/';
	p++;
	strcpy(p, key);
	if(type && *type)
	{
		p = strchr(p, 0);
		*p = '.';
		p++;
		strcpy(p, type);
	}
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
