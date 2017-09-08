/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015-2017 BBC
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

#ifndef LIBSPIDER_H_
# define LIBSPIDER_H_                   1

# include "libcrawl.h"
# include "libcluster.h"

/* This header contains the interface used by crawler plug-ins */

typedef struct spider_struct SPIDER;
typedef struct processor_struct PROCESSOR;
typedef struct queue_struct QUEUE;
typedef struct spider_policy_struct SPIDERPOLICY;
typedef struct spider_callbacks_v2_struct SPIDERCALLBACKS;

#ifndef SPIDER_STRUCT_DEFINED
struct spider_struct
{
	struct spider_api_struct *api;
};
#endif

struct spider_api_struct
{
	void *reserved;
	unsigned long (*addref)(SPIDER *me);
	unsigned long (*release)(SPIDER *me);
	/* Set and obtain app-specific data */
	int (*set_userdata)(SPIDER *me, void *data);
	void *(*userdata)(SPIDER *me);
	/* Set the local 0-based thread index for this instance */
	int (*set_local_index)(SPIDER *me, int index);
	/* Set the base thread index for this process */
	int (*set_base)(SPIDER *me, int base);
	/* Inform the instance of the total number of threads in the cluster */
	int (*set_threads)(SPIDER *me, int threads);
	/* Obtain the numeric crawler ID */
	int (*crawler_id)(SPIDER *me);
	/* Obtain the local 0-based thread index */
	int (*local_index)(SPIDER *me);
	/* Obtain the total number of threads */
	int (*threads)(SPIDER *me);
	/* Obtain the encapsulated crawler instance */
	CRAWL *(*crawler)(SPIDER *me);
	/* Set the queue implementation (including by URI or URI-string) */
	int (*set_queue)(SPIDER *me, QUEUE *queue);
	int (*set_queue_uristr)(SPIDER *me, const char *uri);
	int (*set_queue_uri)(SPIDER *me, URI *uri);
	/* Obtain the queue object */
	QUEUE *(*queue)(SPIDER *me);
	/* Set the processor implementation by name */
	int (*set_processor)(SPIDER *me, PROCESSOR *processor);
	int (*set_processor_name)(SPIDER *me, const char *name);
	PROCESSOR *(*processor)(SPIDER *me);
	/* Obtain configuration values (allocated string, integer, boolean) */
	char *(*config_geta)(SPIDER *me, const char *key, const char *defval);
	int (*config_get_int)(SPIDER *me, const char *key, int defval);
	int (*config_get_bool)(SPIDER *me, const char *key, int defval);
	/* Mark this thread as terminated */
	int (*terminate)(SPIDER *me);
	/* Determine if this thread has been terminated */
	int (*terminated)(SPIDER *me);
	/* Set this thread to be in one-shot mode */
	int (*set_oneshot)(SPIDER *me);
	/* Determine if this thread is in one-shot mode */
	int (*oneshot)(SPIDER *me);
	/* Set the cluster object used by this instance (overrides
	 * any previously-set thread indices
	 */
	int (*set_cluster)(SPIDER *me, CLUSTER *cluster);
	/* Obtain the cluster object used by this instance */
	CLUSTER *(*cluster)(SPIDER *me);
	/* Log a formatted message */
	int (*log)(SPIDER *me, int level, const char *format, ...);
	int (*vlog)(SPIDER *me, int level, const char *format, va_list ap);
	/* Perform a single dequeue-fetch-process operation */
	int (*perform)(SPIDER *me);
	/* Attach this instance to a newly-created crawl thread */
	int (*attach)(SPIDER *me);
	/* Detach this instance from a crawl thread which is terminating */
	int (*detach)(SPIDER *me);
	/* Determine whether this instance has been attached or not */
	int (*attached)(SPIDER *me);
	/* Add a policy handler to this instance */
	int (*add_policy)(SPIDER *me, SPIDERPOLICY *policy);
	int (*add_policy_name)(SPIDER *me, const char *policyname);
};

/* A set of callbacks supplied when creating a spider instance */

# define SPIDER_CALLBACKS_VERSION       2

struct spider_callbacks_v1_struct
{
	int version;
	void (*logger)(int level, const char *format, va_list args);
};

struct spider_callbacks_v2_struct
{
	int version;
	void (*logger)(int level, const char *format, va_list args);
	char *(*config_geta)(const char *, const char *);
	int (*config_get_int)(const char *, int);
	int (*config_get_bool)(const char *, int);
};

#ifndef QUEUE_STRUCT_DEFINED
struct queue_struct
{
	struct queue_api_struct *api;
};
#endif

struct queue_api_struct
{
	void *reserved;
	unsigned long (*addref)(QUEUE *me);
	unsigned long (*release)(QUEUE *me);
	int (*next)(QUEUE *me, URI **next, CRAWLSTATE *state);
	int (*add)(QUEUE *me, URI *uri, const char *uristr);
	int (*updated_uri)(QUEUE *me, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
	int (*updated_uristr)(QUEUE *me, const char *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
	int (*unchanged_uri)(QUEUE *me, URI *uri, int error);
	int (*unchanged_uristr)(QUEUE *me, const char *uri, int error);
	int (*force_add)(QUEUE *me, URI *uri, const char *uristr);
	int (*set_crawlers)(QUEUE *me, int count);
	int (*set_caches)(QUEUE *me, int count);
	int (*set_crawler)(QUEUE *me, int id);
	int (*set_cache)(QUEUE *me, int id);
};

#ifndef PROCESSOR_STRUCT_DEFINED
struct processor_struct
{
	struct processor_api_struct *api;
};
#endif

struct processor_api_struct
{
	void *reserved;
	unsigned long (*addref)(PROCESSOR *me);
	unsigned long (*release)(PROCESSOR *me);
	CRAWLSTATE (*process)(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, const char *content_type);
};

#ifndef SPIDER_POLICY_STRUCT_DEFINED
struct spider_policy_struct
{
	struct spider_policy_api_struct *api;
};
#endif

struct spider_policy_api_struct
{
	void *reserved;
	unsigned long (*addref)(SPIDERPOLICY *me);
	unsigned long (*release)(SPIDERPOLICY *me);
	CRAWLSTATE (*uri)(SPIDERPOLICY *me, URI *uri, const char *uristr);
	CRAWLSTATE (*checkpoint)(SPIDERPOLICY *me, CRAWLOBJ *object, int *status);
};

/* Return values from SPIDER::perform() */

# define SPIDER_PERFORM_SUSPENDED       -2 /* Current crawler is (now) suspended */
# define SPIDER_PERFORM_ERROR           -1 /* A hard error occurred */
# define SPIDER_PERFORM_AGAIN           0  /* No items available to dequeue */
# define SPIDER_PERFORM_COMPLETE        1  /* Deqeued and processed */

/* Log messages */

# define SPIDER_MSG_NAME(type, component, key) \
	MSG_##type##_##component##_##key
# define SPIDER_MSG_PREFIX(type, component, key) \
	"%%ANANSI-" #type "-" #key ": "

# define SPIDER_MSG(type, component, key) \
	SPIDER_MSG_NAME(type, component, key) \
	SPIDER_MSG_PREFIX(type, component, key)

/* crawld and utilities */
# define MSG_E_CRAWL_LINETOOLONG        "%%ANANSI-E-2000: line too long, skipping"
# define MSG_E_CRAWL_URIPARSE           "%%ANANSI-E-2001: failed to parse URI"
# define MSG_C_CRAWL_URIPARSE           "%%ANANSI-C-2001: failed to parse URI"
# define MSG_E_CRAWL_ADDFAILED          "%%ANANSI-E-2002: failed to add URI to crawl queue"
# define MSG_C_CRAWL_ADDFAILED          "%%ANANSI-E-2002: failed to add URI to crawl queue"
# define MSG_N_CRAWL_UPDATED            "%%ANANSI-N-2003: crawler queue updated"
# define MSG_C_CRAWL_FORK               "%%ANANSI-C-2004: failed to fork child process"
# define MSG_C_CRAWL_PIDWRITE           "%%ANANSI-C-2005: failed to open PID file for writing"
# define MSG_C_CRAWL_SETSID             "%%ANANSI-C-2006: failed to create new process group"
# define MSG_C_CRAWL_CWD                "%%ANANSI-C-2007: failed to change working directory"
# define MSG_C_CRAWL_DEVNULL            "%%ANANSI-C-2008: failed to open /dev/null for reading and writing"
# define MSG_N_CRAWL_TERMINATE          "%%ANANSI-N-2009: received request to terminate"
# define MSG_N_CRAWL_TESTMODE           "%%ANANSI-N-2010: test mode enabled: ignoring cluster configuration"
# define MSG_N_CRAWL_REBALANCED         "%%ANANSI-N-2011: cluster has re-balanced"
# define MSG_C_CRAWL_INSTANCE           "%%ANANSI-N-2012: failed to create crawler instance"
# define MSG_I_CRAWL_SCHEMENOTWHITE     "%%ANANSI-I-2013: URI has a scheme which is not whitelisted"
# define MSG_I_CRAWL_SCHEMEBLACK        "%%ANANSI-I-2014: URI has a scheme which is blacklisted"
# define MSG_C_CRAWL_PROCESSORCONFIG    "%%ANANSI-C-2015: the [processor] configuration section is missing a 'name' option"
# define MSG_C_CRAWL_PROCESSORUNKNOWN   "%%ANANSI-C-2016: the specified processing engine is not registered"
# define MSG_C_CRAWL_PROCESSORINIT      "%%ANANSI-C-2017: failed to initialise the processing engine"
# define MSG_E_CRAWL_DEADREDIRECT       "%%ANANSI-E-2018: received a redirect response with no Location header to follow"
# define MSG_I_CRAWL_ACCEPTED           "%%ANANSI-I-2019: ACCEPTED"
# define MSG_C_CRAWL_QUEUECONFIG        "%%ANANSI-C-2020: the [queue] configuration section is missing a 'name' option"
# define MSG_C_CRAWL_QUEUEUNKNOWN       "%%ANANSI-C-2021: the specified queue engine is not registered"
# define MSG_C_CRAWL_QUEUEINIT          "%%ANANSI-C-2022: failed to initialise the queue engine"
# define MSG_C_CRAWL_URIUNPARSE         "%%ANANSI-C-2023: failed to re-compose URI as a string"
# define MSG_C_CRAWL_NOMEM              "%%ANANSI-C-2024: memory allocation failure"
# define MSG_C_CRAWL_THREADCREATE       "%%ANANSI-C-2025: failed to spawn new thread"
# define MSG_N_CRAWL_TERMINATETHREADS   "%%ANANSI-N-2026: terminating crawl threads..."
# define MSG_I_CRAWL_TERMINATEWAIT      "%%ANANSI-I-2027: waiting for crawl threads to terminate..."
# define MSG_N_CRAWL_STOPPED            "%%ANANSI-N-2028: all crawl threads have stopped"
# define MSG_N_CRAWL_READY              "%%ANANSI-N-2029: new crawl thread ready"
# define MSG_N_CRAWL_SUSPENDED          "%%ANANSI-N-2030: crawl thread suspended due to re-balancing"
# define MSG_N_CRAWL_RESUMING           "%%ANANSI-N-2031: crawl thread resuming"
# define MSG_C_CRAWL_FAILED             "%%ANANSI-C-2032: crawl operation failed"
# define MSG_I_CRAWL_THREADBALANCED     "%%ANANSI-I-2033: crawl thread re-balanced"
/*# define SPIDER_MSG(I, CRAWL, THREADBALANCED) "crawl thread re-balanced"*/

# define MSG_N_CRAWL_TERMINATING        "%%ANANSI-N-2034: crawl thread terminating"
# define MSG_C_CRAWL_CLUSTERSTATE       "%%ANANSI-C-2035: failed to obtain current cluster state"
# define MSG_C_CRAWL_NOTATTACHED        "%%ANANSI-C-2036: cannot perform a crawl pass when not attached to a thread"

/* RDBMS queue */
# define MSG_C_DB_CONNECT               "%%ANANSI-C-5000: failed to connect to database"
# define MSG_C_DB_MIGRATE               "%%ANANSI-C-5001: database migration failed"
# define MSG_N_DB_MIGRATEONLY           "%%ANANSI-N-5002: performing schema migration only"
# define MSG_N_DB_TESTURI               "%%ANANSI-N-5003: using test URI"
# define MSG_C_DB_URIPARSE              "%%ANANSI-C-5004: failed to parse URI"
# define MSG_E_DB_URIPARSE              "%%ANANSI-E-5004: failed to parse URI"
# define MSG_E_DB_SQL                   "%%ANANSI-E-5005: SQL error"
# define MSG_C_DB_SQL                   "%%ANANSI-C-5005: SQL error"
# define MSG_N_DB_MIGRATING             "%%ANANSI-N-5006: migrating database schema"
# define MSG_C_DB_INVALIDROOT           "%%ANANSI-C-5007: invalid root key"
# define MSG_E_DB_URIROOT               "%%ANANSI-E-5004: failed to derive root from URI"
# define MSG_I_DB_PARTUPDATED           "%%ANANSI-I-5008: partition updated"

/* Object constructors */

SPIDER *spider_create(const SPIDERCALLBACKS *callbacks);

QUEUE *spider_queue_create_uri(SPIDER *spider, URI *uri);

PROCESSOR *spider_processor_create_name(SPIDER *spider, const char *name);

SPIDERPOLICY *spider_policy_create_name(SPIDER *spider, const char *name);

# ifndef SPIDER_DEPRECATED_APIS
int queue_add_uristr(CRAWL *crawler, const char *str);
int queue_add_uri(CRAWL *crawler, URI *uri);
# endif

#endif /*!LIBSPIDER_H_*/
