/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC.
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

#ifndef LIBCRAWL_H_
# define LIBCRAWL_H_                    1

# include <stdint.h>
# include <time.h>
# include <jsondata.h>
# include <liburi.h>

/* A crawl context.
 *
 * Note that while libcrawl is thread-safe, a single crawl context cannot be
 * used in multiple threads concurrently. You must either protect the context
 * with a lock, or create separate contexts for each thread which will invoke
 * libcrawl methods.
 */
typedef struct crawl_struct CRAWL;

/* A cache implementation.
 *
 * The populated structure can be passed to crawl_set_cache() to specify the
 * cache implementation that will be used by the context.
 */

# define CACHE_KEY_LEN                 32

typedef char CACHEKEY[CACHE_KEY_LEN+1];

typedef struct crawl_cache_struct CRAWLCACHE;
typedef struct crawl_cache_impl_struct CRAWLCACHEIMPL;

struct crawl_cache_impl_struct
{
	void *reserved;
	unsigned long (*init)(CRAWLCACHE *cache);
	unsigned long (*done)(CRAWLCACHE *cache);
	FILE *(*payload_open_write)(CRAWLCACHE *cache, const CACHEKEY key);
	int (*payload_close_rollback)(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
	int (*payload_close_commit)(CRAWLCACHE *cache, const CACHEKEY key, FILE *f);
	int (*info_read)(CRAWLCACHE *cache, const CACHEKEY key, jd_var *dict);
	int (*info_write)(CRAWLCACHE *cache, const CACHEKEY key, jd_var *dict);
	char *(*uri)(CRAWLCACHE *cache, const CACHEKEY key);
	int (*set_username)(CRAWLCACHE *cache, const char *username);
	int (*set_password)(CRAWLCACHE *cache, const char *password);
	int (*set_endpoint)(CRAWLCACHE *cache, const char *endpoint);
};

struct crawl_cache_struct
{
	/* The cache implementation */
	const CRAWLCACHEIMPL *impl;
	/* The crawl context */
	CRAWL *crawl;
	/* This pointer is owned by (and private to) the cache implementation to
	 * use for any purpose it wishes.
	 */
	void *data;
};
	

/* A crawled object, returned by a cache look-up (crawl_locate) or fetch
 * (crawl_fetch). The same thread restrictions apply to crawled objects
 * as to the context.
 */
typedef struct crawl_object_struct CRAWLOBJ;

/* URI policy callback: invoked before a URI is fetched; returns 1 to proceed,
 * 0 to skip, -1 on error.
 */
typedef int (*crawl_uri_policy_cb)(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

/* Pre-fetch callback: invoked immediately before a URI is fetched (and after
 * the policy callback, if any).
 */
typedef int (*crawl_prefetch_cb)(CRAWL *crawl, URI *uri, const char *uristr, void *userdata);

/* Updated callback: invoked after a resource has been fetched and stored in
 * the cache.
 */
typedef int (*crawl_updated_cb)(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

/* Failed callback: invoked after a resource fetch was attempted but was aborted
 * due to a hard error.
 */
typedef int (*crawl_failed_cb)(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

/* Unchanged callback: invoked after a resource was rolled back because there is no
 * newer version of the resource.
 */
typedef int (*crawl_unchanged_cb)(CRAWL *crawl, CRAWLOBJ *obj, time_t prevtime, void *userdata);

/* Next callback: invoked to obtain the next URI to crawl; if *next is NULL on
 * return, crawling ends. The URI returned via *next will be freed by libcrawl.
 */
typedef int (*crawl_next_cb)(CRAWL *crawl, URI **next, void *userdata);

/* Checkpoint callback: invoked after the response headers have been received
 * but before the payload has been. If the callback returns a nonzero result,
 * the fetch will be aborted.
 */
typedef int (*crawl_checkpoint_cb)(CRAWL *crawl, CRAWLOBJ *obj, int *status, void *userdata);

/* libcrawl-provided cache implementations */

extern const CRAWLCACHEIMPL *diskcache;
extern const CRAWLCACHEIMPL *s3cache;

/* Create a crawl context */
CRAWL *crawl_create(void);
/* Destroy a context created with crawl_create() */
void crawl_destroy(CRAWL *p);
/* Obtain a known cache implementation for a URI scheme */
const CRAWLCACHEIMPL *crawl_cache_scheme(CRAWL *crawl, const char *scheme);
/* Set the cache implementation that will be used by this context */
int crawl_set_cache(CRAWL *crawl, const CRAWLCACHEIMPL *cache);
/* Set the Accept header sent in subsequent requests */
int crawl_set_accept(CRAWL *crawl, const char *accept);
/* Set the User-Agent header sent in subsequent requests */
int crawl_set_ua(CRAWL *crawl, const char *ua);
/* Set the private user-data pointer passed to callback functions */
int crawl_set_userdata(CRAWL *crawl, void *userdata);
/* Set the verbose flag */
int crawl_set_verbose(CRAWL *crawl, int verbose);
/* Set the cache path or URI string */
int crawl_set_cache_path(CRAWL *crawl, const char *path);
/* Set the cache URI */
int crawl_set_cache_uri(CRAWL *crawl, URI *uri);
/* Retrieve the private user-data pointer previously set with crawl_set_userdata() */
void *crawl_userdata(CRAWL *crawl);
/* Set the username (or access key) used by the cache */
int crawl_set_username(CRAWL *crawl, const char *username);
/* Set the password (or secret) used by the cache */
int crawl_set_password(CRAWL *crawl, const char *password);
/* Set the endpoint used by the cache */
int crawl_set_endpoint(CRAWL *crawl, const char *endpoint);
/* Set the callback function used to apply a URI policy */
int crawl_set_uri_policy(CRAWL *crawl, crawl_uri_policy_cb cb);
/* Set the callback function invoked when an object is updated */
int crawl_set_updated(CRAWL *crawl, crawl_updated_cb cb);
/* Set the callback function invoked when an object could not be updated */
int crawl_set_failed(CRAWL *crawl, crawl_failed_cb cb);
/* Set the callback function invoked to get the next URI to crawl */
int crawl_set_next(CRAWL *crawl, crawl_next_cb cb);
/* Set the callback function invoked before the object payload is retrieved */
int crawl_set_checkpoint(CRAWL *crawl, crawl_checkpoint_cb cb);
/* Set the callback function invoked when an object is rolled back */
int crawl_set_unchanged(CRAWL *crawl, crawl_unchanged_cb cb);
/* Set the callback function invoked before an object is fetched */
int crawl_set_prefetch(CRAWL *crawl, crawl_prefetch_cb cb);

/* Open the payload file for a crawl object */
FILE *crawl_obj_open(CRAWLOBJ *obj);
/* Destroy an (in-memory) crawl object */
int crawl_obj_destroy(CRAWLOBJ *obj);
/* Obtain the cache key for a crawl object */
const char *crawl_obj_key(CRAWLOBJ *obj);
/* Obtain the HTTP status for a crawl object */
int crawl_obj_status(CRAWLOBJ *obj);
/* Obtain the storage timestamp for a crawl object */
time_t crawl_obj_updated(CRAWLOBJ *obj);
/* Obtain the headers for a crawl object */
int crawl_obj_headers(CRAWLOBJ *obj, jd_var *out, int clone);
/* Obtain the path to the payload for the crawl object */
const char *crawl_obj_payload(CRAWLOBJ *obj);
/* Obtain the size of the payload */
uint64_t crawl_obj_size(CRAWLOBJ *obj);
/* Obtain the crawl object URI */
const URI *crawl_obj_uri(CRAWLOBJ *obj);
/* Obtain the crawl object URI as a string */
const char *crawl_obj_uristr(CRAWLOBJ *obj);
/* Obtain the MIME type of the payload */
const char *crawl_obj_type(CRAWLOBJ *obj);
/* Obtain the redirect target of the resource */
const char *crawl_obj_redirect(CRAWLOBJ *obj);
/* Has this object been freshly-fetched? */
int crawl_obj_fresh(CRAWLOBJ *obj);

/* Determine the cache key for a resource */
int crawl_cache_key(CRAWL *restrict crawl, const char *restrict uri, char *restrict buf, size_t buflen);
/* Determine the cache key for a resource */
int crawl_cache_key_uri(CRAWL *restrict crawl, URI *restrict uri, char *restrict buf, size_t buflen);

/* Fetch a resource specified as a string containing a URI */
CRAWLOBJ *crawl_fetch(CRAWL *crawl, const char *uri);
/* Fetch a resource specified as a URI */
CRAWLOBJ *crawl_fetch_uri(CRAWL *crawl, URI *uri);

/* Locate a cached resource specified as a string */
CRAWLOBJ *crawl_locate(CRAWL *crawl, const char *uristr);
/* Locate a cached resource specified as a URI */
CRAWLOBJ *crawl_locate_uri(CRAWL *crawl, URI *uri);

/* Perform a crawling cycle */
int crawl_perform(CRAWL *crawl);

#endif /*!LIBCRAWL_H_*/