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

/* crawld module for using a SQL database as a queue */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define QUEUE_STRUCT_DEFINED           1
#define TXN_MAX_RETRIES                10

#include <stdlib.h>
#include <string.h>

#include "libsupport.h"
#include "libcrawld.h"

#include <libsql.h>

static int db_migrate(SQL *restrict, const char *identifier, int newversion, void *restrict userdata);
static unsigned long db_addref(QUEUE *me);
static unsigned long db_release(QUEUE *me);
static int db_next(QUEUE *me, URI **next, CRAWLSTATE *state);
static int db_add(QUEUE *me, URI *uri, const char *uristr);
static int db_force_add(QUEUE *me, URI *uri, const char *uristr);
static int db_updated_uri(QUEUE *me, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
static int db_updated_uristr(QUEUE *me, const char *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state);
static int db_unchanged_uri(QUEUE *me, URI *uri, int error);
static int db_unchanged_uristr(QUEUE *me, const char *uristr, int error);
static int db_set_crawlers(QUEUE *db, int count);
static int db_set_caches(QUEUE *db, int count);
static int db_set_crawler(QUEUE *db, int id);
static int db_set_cache(QUEUE *db, int id);

static int db_insert_resource(QUEUE *me, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey, int force);
static int db_insert_root(QUEUE *me, const char *rootkey, const char *uri);
static int db_insert_resource_txn(SQL *db, void *userdata);
static int db_insert_root_txn(SQL *db, void *userdata);
static int db_log_query(SQL *restrict db, const char *restrict statement);
static int db_log_error(SQL *restrict db, const char *restrict sqlstate, const char *restrict message);

static struct queue_api_struct db_api = {
	NULL,
	db_addref,
	db_release,
	db_next,
	db_add,
	db_updated_uri,
	db_updated_uristr,
	db_unchanged_uri,
	db_unchanged_uristr,
	db_force_add,
	db_set_crawlers,
	db_set_caches,
	db_set_crawler,
	db_set_cache
};

struct queue_struct
{
	struct queue_api_struct *api;
	unsigned long refcount;
	CONTEXT *ctx;
	CRAWL *crawl;
	SQL *db;
	int crawler_id;
	int cache_id;
	int ncrawlers;
	int ncaches;
	char *buf;
	size_t buflen;
	int oneshot;
	URI *testuri;
};

struct resource_insert
{
	QUEUE *me;
	const char *cachekey;
	uint32_t shortkey;
	const char *uri;
	const char *rootkey;
	int force;
};

struct root_insert
{
	QUEUE *me;
	const char *rootkey;
	const char *uri;
};

QUEUE *
db_create(CONTEXT *ctx)
{
	QUEUE *p;
	char *t;
	const char *dburi;

	p = (QUEUE *) calloc(1, sizeof(QUEUE));
	if(!p)
	{
		return NULL;
	}
	p->api = &db_api;
	p->refcount = 1;
	p->ctx = ctx;
	p->crawl = ctx->api->crawler(ctx);
	p->crawler_id = ctx->api->crawler_id(ctx);
	p->cache_id = ctx->api->cache_id(ctx);
	p->ncrawlers = ctx->api->threads(ctx);
	p->ncaches = ctx->api->caches(ctx);
	dburi = ctx->api->config_get(ctx, "queue:uri", "mysql://localhost/crawl");
	p->db = sql_connect(dburi);
	if(!p->db)
	{
		log_printf(LOG_CRIT, "DB: Failed to connect to database <%s>\n", dburi);
		free(p);
		return NULL;
	}
	if(config_get_bool("queue:debug-queries", 0))
	{
		sql_set_querylog(p->db, db_log_query);
		sql_set_errorlog(p->db, db_log_error);
	}
	else if(config_get_bool("queue:debug-errors", 0))
	{
		sql_set_errorlog(p->db, db_log_error);
	}
	if(sql_migrate(p->db, "com.github.nevali.crawl.db", db_migrate, NULL))
	{
		log_printf(LOG_CRIT, "DB: Database migration failed\n");
		sql_disconnect(p->db);
		free(p);
		return NULL;
	}
	if(config_get_bool("crawler:schema-update", 0))
	{
		log_printf(LOG_NOTICE, "DB: performing schema update only\n");
		p->testuri = NULL;
		p->oneshot = 1;
	}
	else
	{
		t = config_geta("crawler:test-uri", NULL);
		if(t && t[0])
		{
			log_printf(LOG_NOTICE, "DB: using test URI <%s>\n", t);
			p->testuri = uri_create_str(t, NULL);
			if(!p->testuri)
			{
				log_printf(LOG_NOTICE, "DB: failed to parse URI <%s>\n", t);
				sql_disconnect(p->db);
				free(p);
				free(t);
				return NULL;
			}
			p->oneshot = 1;
			db_force_add(p, p->testuri, t);
		}
		free(t);
	}
	return p;
}

static int
db_log_query(SQL *restrict db, const char *restrict statement)
{
	(void) db;

	log_printf(LOG_DEBUG, "DB: %s\n", statement);
	return 0;
}

static int
db_log_error(SQL *restrict db, const char *restrict sqlstate, const char *restrict message)
{
	(void) db;

	log_printf(LOG_DEBUG, "DB Error: %s: %s\n", sqlstate, message);
	return 0;
}


/* Until libsql gets a DDL API, this will be messy */
static int
db_migrate(SQL *restrict sql, const char *identifier, int newversion, void *restrict userdata)
{
	SQL_LANG lang;
	SQL_VARIANT variant;
	const char *ddl;

	(void) userdata;
	(void) identifier;

	ddl = NULL;
	lang = sql_lang(sql);
	variant = sql_variant(sql);
	if(lang != SQL_LANG_SQL)
	{
		return 0;
	}
	if(newversion == 0)
	{
		/* Return target version */
		return 5;
	}
	log_printf(LOG_NOTICE, "DB: Migrating database to version %d\n", newversion);
	if(newversion == 1)
	{
		if(sql_execute(sql, "DROP TABLE IF EXISTS \"crawl_root\""))
		{
			return -1;
		}
		switch(variant)
		{
		case SQL_VARIANT_MYSQL:
			ddl = "CREATE TABLE \"crawl_root\" ("
				"\"hash\" VARCHAR(32) NOT NULL COMMENT 'Hash of canonical root URI',"
				"\"uri\" VARCHAR(255) NOT NULL COMMENT 'Root URI',"
				"\"added\" DATETIME NOT NULL COMMENT 'Timestamp that this root was added',"
				"\"last_updated\" DATETIME DEFAULT NULL COMMENT 'Timestamp of most recent fetch from this root',"
				"\"earliest_update\" DATETIME DEFAULT NULL COMMENT 'Earliest time this root can be fetched from again',"
				"\"rate\" INT NOT NULL DEFAULT 1000 COMMENT 'Minimum wait time in milliseconds between fetches',"
				"PRIMARY KEY (\"hash\"),"
				"KEY \"crawl_root_last_updated\" (\"last_updated\"),"
				"KEY \"crawl_root_earliest_update\" (\"earliest_update\")"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8 DEFAULT COLLATE=utf8_unicode_ci";
			break;
		case SQL_VARIANT_POSTGRES:
			ddl = "CREATE TABLE \"crawl_root\" ("
				"\"hash\" VARCHAR(32) NOT NULL, "
				"\"uri\" VARCHAR(255) NOT NULL, "
				"\"added\" TIMESTAMP NOT NULL, "
				"\"last_updated\" TIMESTAMP DEFAULT NULL, "
				"\"earliest_update\" TIMESTAMP DEFAULT NULL, "
				"\"rate\" INT NOT NULL DEFAULT 1000, "
				"PRIMARY KEY (\"hash\")"
				")";
			break;
		}
		if(sql_execute(sql, ddl))
		{
			return -1;
		}
		if(variant == SQL_VARIANT_POSTGRES)
		{
			if(sql_execute(sql, "CREATE INDEX \"crawl_root_hash\" ON \"crawl_root\" ( \"hash\" )") )
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_root_last_updated\" ON \"crawl_root\" ( \"last_updated\" )") )
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_root_earliest_update\" ON \"crawl_root\" ( \"earliest_update\" )") )
			{
				return -1;
			}
		}
		return 0;
	}
	if(newversion == 2)
	{
		if(sql_execute(sql, "DROP TABLE IF EXISTS \"crawl_resource\""))
		{
			return -1;
		}
		switch(variant)
		{
		case SQL_VARIANT_MYSQL:
			ddl = "CREATE TABLE \"crawl_resource\" ("
				"\"hash\" VARCHAR(32) NOT NULL COMMENT 'Hash of canonical URI',"
				"\"shorthash\" BIGINT UNSIGNED NOT NULL COMMENT 'First 32 bits of hash',"
				"\"crawl_bucket\" INT NOT NULL COMMENT 'Assigned crawler instance',"
				"\"cache_bucket\" INT NOT NULL COMMENT 'Assigned cache instance',"
				"\"crawl_instance\" INT DEFAULT NULL COMMENT 'Active crawler instance',"
				"\"root\" VARCHAR(32) NOT NULL COMMENT 'Hash of canonical root URI',"
				"\"updated\" DATETIME DEFAULT NULL COMMENT 'Timestamp that this resource was updated in the cache',"
				"\"added\" DATETIME NOT NULL COMMENT 'Timestamp that this resource was added',"
				"\"last_modified\" DATETIME DEFAULT NULL COMMENT 'Timestamp that this resource was modified',"
				"\"status\" INT DEFAULT NULL COMMENT 'HTTP status code',"			
				"\"uri\" VARCHAR(255) NOT NULL COMMENT 'Resource URI',"	
				"\"next_fetch\" DATETIME NOT NULL COMMENT 'Earliest time for next fetch of this resource',"
				"\"error_count\" INT NOT NULL DEFAULT 0 COMMENT 'Hard error count',"
				"\"last_ttl\" INT NOT NULL DEFAULT 0 COMMENT 'Last TTL in hours',"
				"\"soft_error_count\" INT NOT NULL DEFAULT 0 COMMENT 'Soft error count',"
				"PRIMARY KEY (\"hash\"),"
				"KEY \"crawl_resource_crawl_bucket\" (\"crawl_bucket\"),"
				"KEY \"crawl_resource_cache_bucket\" (\"cache_bucket\"),"
				"KEY \"crawl_resource_crawl_instance\" (\"crawl_instance\"),"
				"KEY \"crawl_resource_root\" (\"root\"),"
				"KEY \"crawl_resource_next_fetch\" (\"next_fetch\")"
				") ENGINE=InnoDB DEFAULT CHARSET=utf8 DEFAULT COLLATE=utf8_unicode_ci";
			break;
		case SQL_VARIANT_POSTGRES:
			ddl = "CREATE TABLE \"crawl_resource\" ("
				"\"hash\" VARCHAR(32) NOT NULL,"
				"\"shorthash\" BIGINT NOT NULL,"
				"\"crawl_bucket\" INT NOT NULL,"
				"\"cache_bucket\" INT NOT NULL,"
				"\"crawl_instance\" INT DEFAULT NULL,"
				"\"root\" VARCHAR(32) NOT NULL,"
				"\"updated\" TIMESTAMP DEFAULT NULL,"
				"\"added\" TIMESTAMP NOT NULL,"
				"\"last_modified\" TIMESTAMP DEFAULT NULL,"
				"\"status\" INT DEFAULT NULL,"
				"\"uri\" VARCHAR(255) NOT NULL,"
				"\"next_fetch\" TIMESTAMP NOT NULL,"
				"\"error_count\" INT NOT NULL DEFAULT 0,"
				"\"last_ttl\" INT NOT NULL DEFAULT 0,"
				"\"soft_error_count\" INT NOT NULL DEFAULT 0,"
				"PRIMARY KEY (\"hash\")"
				")";
			break;
		}
		if(sql_execute(sql, ddl))
		{
			return -1;
		}
		if(variant == SQL_VARIANT_POSTGRES)
		{
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_hash\" ON \"crawl_resource\" ( \"hash\" )"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_crawl_bucket\" ON \"crawl_resource\" ( \"crawl_bucket\" )"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_cache_bucket\" ON \"crawl_resource\" ( \"cache_bucket\" )"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_crawl_instance\" ON \"crawl_resource\" ( \"crawl_instance\" )"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_root\" ON \"crawl_resource\" ( \"root\" )"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_next_fetch\" ON \"crawl_resource\" ( \"next_fetch\" )"))
			{
				return -1;
			}
		}
		return 0;
	}
	if(newversion == 3)
	{
		switch(variant)
		{
		case SQL_VARIANT_MYSQL:
			if(sql_execute(sql, "ALTER TABLE \"crawl_resource\" "
						   "ADD COLUMN \"tinyhash\" TINYINT UNSIGNED NOT NULL AFTER \"shorthash\", "
						   "ADD INDEX \"tinyhash\" (\"tinyhash\")"))
			{
				return -1;
			}
			break;
		case SQL_VARIANT_POSTGRES:
			if(sql_execute(sql, "ALTER TABLE \"crawl_resource\" ADD COLUMN \"tinyhash\" INTEGER NOT NULL"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_tinyhash\" ON \"crawl_resource\" ( \"tinyhash\" )"))
			{
				return -1;
			}
			break;
		}
		return 0;
	}
	if(newversion == 4)
	{
		switch(variant)
		{
		case SQL_VARIANT_MYSQL:
			if(sql_execute(sql, "ALTER TABLE \"crawl_resource\" "
						   "ADD COLUMN \"state\" ENUM('NEW', 'FAILED', 'REJECTED', 'ACCEPTED', 'COMPLETE') NOT NULL DEFAULT 'NEW', "
						   "ADD INDEX \"state\" (\"state\")"))
			{
				return -1;
			}
			break;
		case SQL_VARIANT_POSTGRES:
			if(sql_execute(sql, "DROP TYPE IF EXISTS \"crawl_resource_state\""))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE TYPE \"crawl_resource_state\" AS ENUM('NEW', 'FAILED', 'REJECTED', 'ACCEPTED', 'COMPLETE')"))
			{
				return -1;
			}
			if(sql_execute(sql, "ALTER TABLE \"crawl_resource\" "
						   "ADD COLUMN \"state\" \"crawl_resource_state\" NOT NULL DEFAULT 'NEW'"))
			{
				return -1;
			}
			if(sql_execute(sql, "CREATE INDEX \"crawl_resource_state\" ON \"crawl_resource\" ( \"state\" )"))
			{
				return -1;
			}
		}
		return 0;
	}
	if(newversion == 5)
	{
		switch(variant)
		{
		case SQL_VARIANT_MYSQL:
			ddl = "ALTER TABLE \"crawl_resource\" "
				"MODIFY COLUMN \"state\" ENUM('NEW', 'FAILED', 'REJECTED', 'ACCEPTED', 'COMPLETE', 'FORCE') NOT NULL DEFAULT 'NEW'";
			break;
		case SQL_VARIANT_POSTGRES:
			/* This is pretty hacky */
			if(sql_commit(sql))
			{
				return -1;
			}
			if(sql_execute(sql, "ALTER TYPE \"crawl_resource_state\" ADD VALUE 'FORCE'"))
			{
				return -1;
			}
			if(sql_begin(sql, SQL_TXN_CONSISTENT))
			{
				return -1;
			}
			return 0;
		}
		if(sql_execute(sql, ddl))
		{
			return -1;
		}
		return 0;
	}
	return -1;
}

static unsigned long
db_addref(QUEUE *me)
{
	me->refcount++;
	return me->refcount;
}

static unsigned long
db_release(QUEUE *me)
{
	me->refcount--;
	if(me->refcount)
	{
		return me->refcount;
	}
	if(me->db)
	{
		sql_disconnect(me->db);
	}
	free(me->buf);
	free(me);
	return 0;
}

static int
db_next(QUEUE *me, URI **next, CRAWLSTATE *state)
{
	SQL_STATEMENT *rs;
	size_t needed;
	char *p;
	char statebuf[32];

	*state = COS_NEW;
	*next = NULL;

	if(me->testuri)
	{
		*next = me->testuri;
		*state = COS_FORCE;
		me->testuri = NULL;
		return 0;
	}
	if(me->oneshot)
	{
		return 0;
	}
	rs = sql_queryf(me->db,
					"SELECT \"res\".\"uri\", \"res\".\"state\" "
					" FROM "
					" \"crawl_resource\" \"res\", \"crawl_root\" \"root\" "
					" WHERE "
					" \"root\".\"rate\" > 0 AND "
					" \"res\".\"tinyhash\" %% %d = %d AND "
					" \"root\".\"hash\" = \"res\".\"root\" AND "
					" \"root\".\"earliest_update\" < NOW() AND "
					" \"res\".\"next_fetch\" < NOW() "
					" ORDER BY \"root\".\"earliest_update\" ASC, \"res\".\"next_fetch\" ASC, \"root\".\"rate\" ASC",
					me->ncrawlers, me->crawler_id);
	if(!rs)
	{
		log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
		exit(EXIT_FAILURE);
	}
	if(sql_stmt_eof(rs))
	{
/*		log_printf(LOG_DEBUG, "db_next: queue query returned no results\n"); */
		sql_stmt_destroy(rs);
		return 0;
	}
	memset(statebuf, 0, sizeof(statebuf));
	sql_stmt_value(rs, 1, statebuf, sizeof(statebuf));
	if(!strcmp(statebuf, "NEW"))
	{
		*state = COS_NEW;
	}
	else if(!strcmp(statebuf, "FAILED"))
	{
		*state = COS_FAILED;
	}
	else if(!strcmp(statebuf, "REJECTED"))
	{
		*state = COS_REJECTED;
	}
	else if(!strcmp(statebuf, "ACCEPTED"))
	{
		*state = COS_ACCEPTED;
	}
	else if(!strcmp(statebuf, "COMPLETE"))
	{
		*state = COS_COMPLETE;
	}
	else if(!strcmp(statebuf, "FORCE"))
	{
		*state = COS_FORCE;
	}
	needed = sql_stmt_value(rs, 0, NULL, 0);
	if(needed > me->buflen)
	{
		p = (char *) realloc(me->buf, needed + 1);
		if(!p)
		{
			sql_stmt_destroy(rs);
			return -1;
		}
		me->buf = p;
		me->buflen = needed + 1;
	}
	if(sql_stmt_value(rs, 0, me->buf, me->buflen) != needed)
	{
		sql_stmt_destroy(rs);
		return -1;
	}
	*next = uri_create_str(me->buf, NULL);
	sql_stmt_destroy(rs);
	if(!*next)
	{
		return -1;
	}
	return 0;
}

static int
db_uristr_key_root(QUEUE *me, const char *uristr, char **uri, char *urikey, uint32_t *shortkey, char **root, char *rootkey)
{
	URI *u_resource, *u_root;
	char *str, *t;
	char skey[9];

	str = strdup(uristr);
	if(!str)
	{
		free(str);
		return -1;
	}
	t = strchr(str, '#');
	if(t)
	{
		*t = 0;
	}
	u_resource = uri_create_str(str, NULL);
	if(!u_resource)
	{
		free(str);
		return -1;
	}
	/* Ensure we have the canonical form of the URI */
	free(str);
	str = uri_stralloc(u_resource);
	if(!str)
	{
		uri_destroy(u_resource);
		return -1;
	}
	if(uri)
	{
		*uri = str;
	}
	if(crawl_cache_key(me->crawl, str, urikey, 48))
	{
		uri_destroy(u_resource);
		free(str);
		return -1;
	}
	strncpy(skey, urikey, 8);
	skey[8] = 0;
	*shortkey = (uint32_t) strtoul(skey, NULL, 16);
	
	u_root = uri_create_str("/", u_resource);
	if(crawl_cache_key_uri(me->crawl, u_root, rootkey, 48))
	{
		uri_destroy(u_resource);
		uri_destroy(u_root);
		free(str);
		return -1;		
	}
	if(root)
	{
		*root = uri_stralloc(u_root);
	}
	if(!uri)
	{
		free(str);
	}
	uri_destroy(u_resource);
	uri_destroy(u_root);
	return 0;
}

static int
db_add(QUEUE *me, URI *uri, const char *uristr)
{
	char *canonical, *root;
	char cachekey[48], rootkey[48];
	uint32_t shortkey;
	
	(void) uri;

	if(db_uristr_key_root(me, uristr, &canonical, cachekey, &shortkey, &root, rootkey))
	{
		return -1;
	}
	
	db_insert_resource(me, cachekey, shortkey, canonical, rootkey, 0);
	db_insert_root(me, rootkey, root);
	
	free(root);
	free(canonical);
	return 0;	
}

static int
db_force_add(QUEUE *me, URI *uri, const char *uristr)
{
	char *canonical, *root;
	char cachekey[48], rootkey[48];
	uint32_t shortkey;
	
	(void) uri;

	if(db_uristr_key_root(me, uristr, &canonical, cachekey, &shortkey, &root, rootkey))
	{
		return -1;
	}
	
	db_insert_resource(me, cachekey, shortkey, canonical, rootkey, 1);
	db_insert_root(me, rootkey, root);
	
	free(root);
	free(canonical);
	return 0;	
}


static int
db_updated_uri(QUEUE *me, URI *uri, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state)
{
	char *uristr;
	int r;
	
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		return -1;
	}
	r = db_updated_uristr(me, uristr, updated, last_modified, status, ttl, state);
	free(uristr);
	return r;
}

static int
db_updated_uristr(QUEUE *me, const char *uristr, time_t updated, time_t last_modified, int status, time_t ttl, CRAWLSTATE state)
{
	char *canonical, *root;
	char cachekey[48], rootkey[48], updatedstr[32], lastmodstr[32], nextfetchstr[32];
	uint32_t shortkey;
	struct tm tm;
	time_t now;
	const char *statestr;
	
	if(db_uristr_key_root(me, uristr, &canonical, cachekey, &shortkey, &root, rootkey))
	{
		return -1;
	}
	gmtime_r(&updated, &tm);
	strftime(updatedstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	gmtime_r(&last_modified, &tm);
	strftime(lastmodstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	if(status != 200)
	{
		if(ttl < 86400)
		{
			ttl = 86400;
		}
	}
	else
	{
		if(ttl < 3600)
		{
			ttl = 3600;
		}
	}
	ttl += time(NULL);
	gmtime_r(&ttl, &tm);
	strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	switch(state)
	{
	case COS_NEW:
		statestr = "NEW";
		break;
	case COS_FAILED:
		statestr = "FAILED";
		break;
	case COS_REJECTED:
		statestr = "REJECTED";
		break;
	case COS_ACCEPTED:
		statestr = "ACCEPTED";
		break;
	case COS_COMPLETE:
		statestr = "COMPLETE";
		break;
	case COS_FORCE:
		statestr = "FORCE";
		break;
	}
	if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"updated\" = %Q, \"last_modified\" = %Q, \"status\" = %d, \"crawl_instance\" = NULL, \"state\" = %Q WHERE \"hash\" = %Q",
					updatedstr, lastmodstr, status, statestr, cachekey))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"next_fetch\" = %Q WHERE \"hash\" = %Q AND \"next_fetch\" < %Q",
					nextfetchstr, cachekey, nextfetchstr))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	now = time(NULL);
	gmtime_r(&now, &tm);
	strftime(lastmodstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	now += 2;
	strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	if(sql_executef(me->db, "UPDATE \"crawl_root\" SET \"last_updated\" = %Q WHERE \"hash\" = %Q", lastmodstr, rootkey))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(sql_executef(me->db, "UPDATE \"crawl_root\" SET \"earliest_update\" = %Q WHERE \"hash\" = %Q AND \"earliest_update\" < %Q", nextfetchstr, rootkey, nextfetchstr))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(status >= 400 && status < 499)
	{
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"error_count\" = \"error_count\" + 1 WHERE \"hash\" = %Q", cachekey))
		{
			log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
			exit(1);
		}
	}
	else if(status >= 500 && status < 599)
	{
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"error_count\" = 0, \"soft_error_count\" = \"soft_error_count\" + 1 WHERE \"hash\" = %Q", cachekey))
		{
			log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
			exit(1);
		}
	}
	else
	{
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"error_count\" = 0, \"soft_error_count\" = 0 WHERE \"hash\" = %Q", cachekey))
		{
			log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
			exit(1);
		}	
	}	
	free(root);
	free(canonical);
	return 0;
}

static int
db_unchanged_uri(QUEUE *me, URI *uri, int error)
{
	char *uristr;
	int r;
	
	uristr = uri_stralloc(uri);
	if(!uristr)
	{
		return -1;
	}
	r = db_unchanged_uristr(me, uristr, error);
	free(uristr);
	return r;
}

static int
db_unchanged_uristr(QUEUE *me, const char *uristr, int error)
{
	char *canonical, *root;
	char cachekey[48], rootkey[48], updatedstr[32], nextfetchstr[32];
	uint32_t shortkey;
	struct tm tm;
	time_t now, ttl;
	
	if(db_uristr_key_root(me, uristr, &canonical, cachekey, &shortkey, &root, rootkey))
	{
		return -1;
	}	
	now = time(NULL);
	gmtime_r(&now, &tm);
	strftime(updatedstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	now += 2;
	strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
	if(sql_executef(me->db, "UPDATE \"crawl_root\" SET \"last_updated\" = %Q, \"earliest_update\" = %Q WHERE \"hash\" = %Q", updatedstr, nextfetchstr, rootkey))
	{
		log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
		exit(1);
	}
	if(error)
	{
		ttl = (86400 * 7) + now;
		gmtime_r(&ttl, &tm);
		strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"updated\" = %Q, \"next_fetch\" = %Q, \"crawl_instance\" = NULL, \"error_count\" = \"error_count\" + 1 WHERE \"hash\" = %Q",
			updatedstr, nextfetchstr, cachekey))
		{
			log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
			exit(1);
		}
	}
	else
	{
		ttl = (3600 * 2) + now;
		gmtime_r(&ttl, &tm);
		strftime(nextfetchstr, 32, "%Y-%m-%d %H:%M:%S", &tm);
		if(sql_executef(me->db, "UPDATE \"crawl_resource\" SET \"updated\" = %Q, \"next_fetch\" = %Q, \"crawl_instance\" = NULL, \"error_count\" = 0 WHERE \"hash\" = %Q",
			updatedstr, nextfetchstr, cachekey))
		{
			log_printf(LOG_CRIT, "%s\n", sql_error(me->db));
			exit(1);
		}		
	}
	return 0;
}

static int
db_set_crawlers(QUEUE *me, int count)
{
	me->ncrawlers = count;
	return 0;
}

static int
db_set_caches(QUEUE *me, int count)
{
	me->ncaches = count;
	return 0;
}

static int
db_set_crawler(QUEUE *me, int id)
{
	me->crawler_id = id;
	return 0;
}

static int
db_set_cache(QUEUE *me, int id)
{
	me->cache_id = id;
	return 0;
}


static int
db_insert_resource(QUEUE *me, const char *cachekey, uint32_t shortkey, const char *uri, const char *rootkey, int force)
{
	struct resource_insert data;
	
	data.me = me;
	data.cachekey = cachekey;
	data.shortkey = shortkey;
	data.uri = uri;
	data.rootkey = rootkey;
	data.force = force;

	if(sql_perform(me->db, db_insert_resource_txn, &data, TXN_MAX_RETRIES, SQL_TXN_CONSISTENT))
	{
		log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
		exit(1);
		return -1;
	}
	return 0;
}

static int
db_insert_root(QUEUE *me, const char *rootkey, const char *uri)
{
	struct root_insert data;
	
	if(!rootkey || strlen(rootkey) != 32)
	{
		log_printf(LOG_CRIT, "DB: invalid root key '%s'\n", rootkey);
		abort();
	}
	data.me = me;
	data.rootkey = rootkey;
	data.uri = uri;
		
	if(sql_perform(me->db, db_insert_root_txn, &data, TXN_MAX_RETRIES, SQL_TXN_CONSISTENT))
	{
		log_printf(LOG_CRIT, "DB: %s\n", sql_error(me->db));
		exit(1);
		return -1;
	}
	return 0;
}

/* A transaction callback returns:
 *
 *  1 => commit
 *  0 => rollback successfully
 * -1 => rollback and retry
 * -2 => rollback and fail
 */
static int
db_insert_resource_txn(SQL *db, void *userdata)
{
	struct resource_insert *data;
	SQL_STATEMENT *rs;
	int crawl_bucket, cache_bucket;
	
	data = (struct resource_insert *) userdata;
	
	rs = sql_queryf(db, "SELECT * FROM \"crawl_resource\" WHERE \"hash\" = %Q", data->cachekey);
	if(!rs)
	{
		/* rollback with error */
		return -2;
	}
	if(!sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		if(data->force)
		{
			if(sql_executef(db, "UPDATE \"crawl_resource\" SET \"next_fetch\" = NOW(), \"state\" = %Q WHERE \"hash\" = %Q", "FORCE", data->cachekey))
			{
				/* INSERT failed */
				if(sql_deadlocked(db))
				{
					/* rollback and retry */
					return -1;
				}
				/* non-deadlock error */
				return -2;
			}
			/* commit */
			return 1;
		}
		/* rollback with success */
		return 0;
	}
	if(data->me->ncrawlers)
	{
		crawl_bucket = data->shortkey % data->me->ncrawlers;
	}
	else
	{
		crawl_bucket = 0;
	}
	if(data->me->ncaches)
	{
		cache_bucket = data->shortkey % data->me->ncaches;
	}
	else
	{
		cache_bucket = 0;
	}
	/* resource isn't present in the table */
	if(sql_executef(db, "INSERT INTO \"crawl_resource\" (\"hash\", \"shorthash\", \"tinyhash\", \"crawl_bucket\", \"cache_bucket\", \"root\", \"uri\", \"added\", \"next_fetch\", \"state\") VALUES (%Q, %lu, %d, %d, %d, %Q, %Q, NOW(), NOW(), %Q)", data->cachekey, data->shortkey, (data->shortkey % 256), crawl_bucket, cache_bucket, data->rootkey, data->uri, "NEW"))
	{
		/* INSERT failed */
		if(sql_deadlocked(db))
		{
			/* rollback and retry */
			return -1;
		}
		/* non-deadlock error */
		return -2;
	}
	sql_stmt_destroy(rs);
	/* commit */
	return 1;
}

/* A transaction callback returns 0 for commit, -1 for rollback and retry, 1 for rollback successfully */
static int
db_insert_root_txn(SQL *db, void *userdata)
{
	struct root_insert *data;
	SQL_STATEMENT *rs;
	
	data = (struct root_insert *) userdata;
	
	rs = sql_queryf(db, "SELECT * FROM \"crawl_root\" WHERE \"hash\" = %Q", data->rootkey);
	if(!rs)
	{
		return -2;
	}
	if(!sql_stmt_eof(rs))
	{
		sql_stmt_destroy(rs);
		return 0;
	}
	sql_stmt_destroy(rs);
	if(sql_executef(db, "INSERT INTO \"crawl_root\" (\"hash\", \"uri\", \"added\", \"earliest_update\", \"rate\") VALUES (%Q, %Q, NOW(), NOW(), 1000)", data->rootkey, data->uri))
	{
		if(sql_deadlocked(db))
		{
			return -1;
		}
		return -2;
	}
	return 1;
}
