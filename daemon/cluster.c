/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2015 BBC
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

#include "p_crawld.h"

static pthread_rwlock_t lock;
static pthread_t thread;
static char *clustername, *clusterenv, *registry;
static uuid_t inst_uuid;
static char inst_uuidstr[40];
static int inst_id, inst_threads;
static int clusterthreads;
static ETCD *etcd, *clusterdir, *envdir;

static int cluster_static_init_(void);

static int cluster_etcd_init_(void);
static int cluster_etcd_balance_(ETCD *dir);
static int cluster_etcd_ping_(ETCD *dir);
static void *cluster_etcd_balance_thread_(void *arg);
static void *cluster_etcd_ping_thread_(void *arg);

int
cluster_init(void)
{
	pthread_rwlock_init(&lock, NULL);
	uuid_generate(inst_uuid);
	uuid_unparse_lower(inst_uuid, inst_uuidstr);
	clustername = config_geta("cluster:name", NULL);
	clusterenv = config_geta("cluster:environment", "production");
	registry = config_geta("cluster:registry", NULL);
	if(registry)
	{
		return cluster_etcd_init_();
	}
	if(clustername)
	{
		log_printf(LOG_WARNING, "a cluster name has been set but no registry service specified; the name will be ignored\n");
	}
	return cluster_static_init_();
}

/* Return the number of threads this instance will start */
int
cluster_inst_threads(void)
{
	int count;

	pthread_rwlock_rdlock(&lock);
    count = inst_threads;
	pthread_rwlock_unlock(&lock);
	return count;
}

/* Return the number of crawler threads in total */
int
cluster_threads(void)
{
	int count;

	pthread_rwlock_rdlock(&lock);
    count = clusterthreads;
	pthread_rwlock_unlock(&lock);
	return count;
}

/* Return the base numeric ID of this crawler */
int
cluster_inst_id(void)
{
	int id;

	pthread_rwlock_rdlock(&lock);
    id = inst_id;
	pthread_rwlock_unlock(&lock);
	return id;
}

/* Return the name of the environment this cluster resides in */
const char *
cluster_env(void)
{
	return clusterenv;
}

/* Initialise a statically-configured cluster */
static int
cluster_static_init_(void)
{
	inst_id = config_get_int("crawler:id", 0);
	inst_threads = config_get_int("crawler:threads", 1);
	clusterthreads = config_get_int("cluster:threads", 1);
	log_printf(LOG_INFO, "initialising static cluster node [%d/%d] in environment '%s'\n", inst_id, clusterthreads, clusterenv);
	return 0;
}

/* Initialise a cluster using etcd */
static int
cluster_etcd_init_(void)
{
	ETCD *balance_dir, *ping_dir;

	inst_threads = config_get_int("crawler:threads", 1);
	if(!clustername)
	{
		log_printf(LOG_CRIT, "cannot use registry service because no cluster name has been specified\n");
		return 1;
	}
	log_printf(LOG_INFO, "initialising cluster member %s:%s/%s\n", clustername, inst_uuidstr, clusterenv);
	etcd = etcd_connect(registry);
	if(!etcd)
	{
		log_printf(LOG_CRIT, "cannot connect to registry service\n");
		return 1;
	}
	clusterdir = etcd_dir_create(etcd, clustername, 0);
	if(!clusterdir)
	{
		clusterdir = etcd_dir_open(etcd, clustername);
		if(!clusterdir)
		{
			log_printf(LOG_CRIT, "failed to create or open registry directory for cluster '%s'\n", clustername);
			return 1;
		}
	}
	envdir = etcd_dir_create(clusterdir, clusterenv, 0);
	if(!envdir)
	{
		envdir = etcd_dir_open(clusterdir, clusterenv);
		if(!envdir)
		{
			log_printf(LOG_CRIT, "failed to create or open registry directory for cluster '%s/%s'\n", clustername, clusterenv);
			return 1;
		}
	}
	cluster_etcd_ping_(envdir);
	/* Perform an initial cluster balancing */
	cluster_etcd_balance_(envdir);
	/* Start the cluster rebalancing thread */
	balance_dir = etcd_clone(envdir);
	pthread_create(&thread, NULL, cluster_etcd_balance_thread_, (void *) balance_dir);
	/* Start the cluster ping thread */
	ping_dir = etcd_clone(envdir);
	pthread_create(&thread, NULL, cluster_etcd_ping_thread_, (void *) ping_dir);
	log_printf(LOG_NOTICE, "cluster member %s:%s/%s initialised\n", inst_uuidstr, clustername, clusterenv);
	return 0;
}

static int
cluster_etcd_balance_(ETCD *dir)
{
	int total, base, val;
	size_t n, c;
	const char *name;

	JD_SCOPE
	{
		jd_var dict = JD_INIT, keys = JD_INIT;
		jd_var *key, *entry, *value;

		if(etcd_dir_get(dir, &dict))
		{
			log_printf(LOG_ERR, "failed to retrieve cluster directory\n");
			return -1;
		}
		jd_keys(&keys, &dict);
		jd_sort(&keys);
		c = jd_count(&keys);
		base = 0;
		total = 0;
		for(n = 0; n < c; n++)
		{
			key = jd_get_idx(&keys, n);
			name = jd_bytes(key, NULL);
			entry = jd_get_key(&dict, key, 0);
			if(!entry || entry->type != HASH)
			{
				continue;
			}
			value = jd_get_ks(entry, "value", 0);
			if(!value)
			{
				continue;
			}
			JD_TRY
			{
				val = jd_get_int(value);
			}
			JD_CATCH(e)
			{
				val = 0;
			}
			if(!strcmp(name, inst_uuidstr))
			{
				base = total;
			}
			total += val;
		}
	}
	pthread_rwlock_wrlock(&lock);
	if(total != clusterthreads || base != inst_id)
	{
		log_printf(LOG_NOTICE, "cluster has re-balanced: new base is %d (was %d), new total is %d (was %d)\n", base, inst_id, total, clusterthreads);
		inst_id = base;
		clusterthreads = total;
	}
	pthread_rwlock_unlock(&lock);
	return 0;
}

static void *
cluster_etcd_balance_thread_(void *arg)
{
	ETCD *dir;
	int r;

	dir = (ETCD *) arg;

	/* Repeatedly watch the etcd directory for changes, and invoke
	 * cluster_etcd_balance() when they occur
	 */
	while(!crawld_terminate)
	{
		JD_SCOPE
		{
			jd_var change = JD_INIT;

			r = etcd_dir_wait(dir, 1, &change);
			jd_release(&change);
		}
		if(r)
		{
			log_printf(LOG_ERR, "failed to receive changes from cluster registry\n");
			sleep(30);
			continue;
		}
		cluster_etcd_balance_(dir);
	}
	return NULL;
}

static void *
cluster_etcd_ping_thread_(void *arg)
{
	ETCD *dir;

	dir = (ETCD *) arg;
	while(!crawld_terminate)
	{
		if(cluster_etcd_ping_(dir))
		{
			log_printf(LOG_ERR, "failed to update registry service\n");
			sleep(5);
			continue;
		}
		log_printf(LOG_DEBUG, "updated registry service for cluster member %s\n", inst_uuidstr);
		sleep(REGISTRY_REFRESH);
	}
	return NULL;
}

static int
cluster_etcd_ping_(ETCD *dir)
{
	char buf[64];

	sprintf(buf, "%d", inst_threads);
	return etcd_key_set_ttl(dir, inst_uuidstr, buf, REGISTRY_KEY_TTL);
}
