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
static char *clustername, *clusterenv, *registry;
static CLUSTER *cluster;

static int inst_id, inst_threads;
static int clusterthreads;

static int cluster_balancer_(CLUSTER *cluster, CLUSTERSTATE *state);

int
cluster_init(void)
{
	int n;

	pthread_rwlock_init(&lock, NULL);
	clustername = config_geta("cluster:name", CRAWLER_APP_NAME);
	cluster = cluster_create(clustername);
	cluster_set_logger(cluster, log_vprintf);
	cluster_set_balancer(cluster, cluster_balancer_);
	if(config_get_bool("cluster:verbose", 0))
	{
		cluster_set_verbose(cluster, 1);
	}
	if((clusterenv = config_geta("cluster:environment", NULL)))
	{
		cluster_set_env(cluster, clusterenv);
	}
	if(config_get_bool("crawler:oneshot", 0) || config_getptr_unlocked("crawler:test-uri", NULL))
	{
		log_printf(LOG_NOTICE, "test mode enabled - ignoring cluster configuration\n");
		cluster_set_threads(cluster, 1);
		cluster_static_set_index(cluster, 0);
		cluster_static_set_total(cluster, 1);		
		return 0;
	}
	if((n = config_get_int("crawler:threads", 1)))
	{
		cluster_set_threads(cluster, n);
	}
	if((registry = config_geta("cluster:registry", NULL)))
	{
		cluster_set_registry(cluster, registry);
	}
	if((n = config_get_int("crawler:id", 0)))
	{
		cluster_static_set_index(cluster, n);
	}
	if((n = config_get_int("cluster:threads", 0)))
	{
		cluster_static_set_total(cluster, n);
	}
	return 0;
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

/* Invoked after the process has been forked */
int
cluster_detached(void)
{
	return cluster_join(cluster);
}

/* Invoked by libcluster when the cluster balances */
static int
cluster_balancer_(CLUSTER *cluster, CLUSTERSTATE *state)
{
	(void) cluster;

	log_printf(LOG_NOTICE, "cluster has re-balanced: instance thread indices %d..%d from a cluster size %d\n", state->index, state->threads, state->total);
	pthread_rwlock_wrlock(&lock);
	inst_id = state->index;
	inst_threads = state->threads;
	clusterthreads = state->total;
	pthread_rwlock_unlock(&lock);
	return 0;
}

