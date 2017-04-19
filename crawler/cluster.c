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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "p_crawld.h"

static pthread_rwlock_t lock;
static char *clustername, *clusterenv, *registry;
static CLUSTER *cluster;

static int inst_id, inst_threads;
static int clusterthreads;

static int crawl_cluster_balancer_(CLUSTER *cluster, CLUSTERSTATE *state);

int
crawl_cluster_init(void)
{
	int n;
	const char *t;

	pthread_rwlock_init(&lock, NULL);
	clustername = config_geta("cluster:name", CRAWLER_APP_NAME);
	cluster = cluster_create(clustername);
	cluster_set_logger(cluster, log_vprintf);
	cluster_set_balancer(cluster, crawl_cluster_balancer_);
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
		log_printf(LOG_NOTICE, MSG_N_CRAWL_TESTMODE "\n");
		cluster_set_workers(cluster, 1);
		cluster_static_set_index(cluster, 0);
		cluster_static_set_total(cluster, 1);		
		return 0;
	}
	if((t = config_getptr_unlocked("crawler:partition", NULL)))
	{
		if(!strcasecmp(t, "null") || !strcasecmp(t, "none"))
		{
			t = NULL;
		}
		cluster_set_partition(cluster, t);
	}
	if((n = config_get_int("crawler:threads", 1)))
	{
		cluster_set_workers(cluster, n);
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
crawl_cluster_inst_threads(void)
{
	int count;

	pthread_rwlock_rdlock(&lock);
    count = inst_threads;
	pthread_rwlock_unlock(&lock);
	return count;
}

/* Return the number of crawler threads in total */
int
crawl_cluster_threads(void)
{
	int count;

	pthread_rwlock_rdlock(&lock);
    count = clusterthreads;
	pthread_rwlock_unlock(&lock);
	return count;
}

/* Return the base numeric ID of this crawler */
int
crawl_cluster_inst_id(void)
{
	int id;

	pthread_rwlock_rdlock(&lock);
    id = inst_id;
	pthread_rwlock_unlock(&lock);
	return id;
}

/* Return the name of the environment this cluster resides in */
const char *
crawl_cluster_env(void)
{
	return clusterenv;
}

/* Invoked after the process has been forked */
int
crawl_cluster_detached(void)
{
	return cluster_join(cluster);
}

/* Invoked by libcluster when the cluster balances */
static int
crawl_cluster_balancer_(CLUSTER *cluster, CLUSTERSTATE *state)
{
	(void) cluster;

	if(state->index == -1 || !state->total)
	{
		log_printf(LOG_NOTICE, MSG_N_CRAWL_REBALANCED ": instance %s has left cluster %s/%s\n", cluster_instance(cluster), cluster_key(cluster), cluster_env(cluster));
	}
	else if(state->workers == 1)
	{
		log_printf(LOG_NOTICE, MSG_N_CRAWL_REBALANCED ": instance %s single-thread index %d from cluster %s/%s of %d threads\n", cluster_instance(cluster), state->index + 1, cluster_key(cluster), cluster_env(cluster), state->total);
	}
	else
	{
		log_printf(LOG_NOTICE, MSG_N_CRAWL_REBALANCED ": instance %s thread indices %d..%d from cluster %s/%s of %d threads\n", cluster_instance(cluster), state->index + 1, state->index + state->workers, cluster_key(cluster), cluster_env(cluster), state->total);
	}
	pthread_rwlock_wrlock(&lock);
	inst_id = state->index;
	if(!state->total)
	{
		inst_id = -1;
	}
	inst_threads = state->workers;
	clusterthreads = state->total;
	pthread_rwlock_unlock(&lock);
	return 0;
}

