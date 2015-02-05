/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2015 BBC
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

#include "p_libetcd.h"

ETCDDIR *
etcd_dir_create_(ETCD *etcd, ETCDDIR *parent, const char *name)
{
	ETCDDIR *dir;
	char *sb;

	dir = (ETCDDIR *) calloc(1, sizeof(ETCDDIR));
	if(!dir)
	{
		return NULL;
	}
	while(*name == '/')
	{
		name++;
	}
	sb = (char *) calloc(1, strlen(name) + 2);
	if(!sb)
	{
		free(dir);
		return NULL;
	}
	dir->uri = uri_create_str(name, parent ? parent->base : etcd->uri);
	if(!dir->uri)
	{
		free(dir);
		free(sb);
		return NULL;
	}
	strcpy(sb, name);
	strcat(sb, "/");
	dir->base = uri_create_str(sb, parent ? parent->base : etcd->uri);
	free(sb);
	if(!dir->base)
	{
		uri_destroy(dir->uri);
		free(dir);
		return NULL;
	}	
	return dir;
}	

ETCDDIR *
etcd_dir_open(ETCD *etcd, ETCDDIR *parent, const char *name)
{
	ETCDDIR *dir;
	CURL *ch;
	int status;
	jd_var dict = JD_INIT;
	jd_var *node, *k;

	dir = etcd_dir_create_(etcd, parent, name);
	if(!dir)
	{
		return NULL;
	}
	ch = etcd_curl_create_(etcd, dir->uri);
	if(!ch)
	{
		etcd_dir_close(dir);
		return NULL;
	}
	JD_SCOPE
	{		
		status = etcd_curl_perform_json_(ch, &dict);
		if(!status && dict.type == HASH)
		{
			status = 1;
			node = jd_get_ks(&dict, "node", 0);
			if(node &&
			   node->type == HASH &&
			   (k = jd_get_ks(node, "dir", 0)) &&
			   jd_cast_int(k))
			{
				/* Entry is a directory */
				status = 0;
			}
		}
		jd_release(&dict);
	}
	etcd_curl_done_(ch);
	if(status)
	{
		etcd_dir_close(dir);
		return NULL;
	}
	return dir;
}
	

ETCDDIR *
etcd_dir_create(ETCD *etcd, ETCDDIR *parent, const char *name, int mustexist)
{
	ETCDDIR *dir;
	CURL *ch;
	const char *data = "dir=1", *existdata = "dir=1&prevExist=true";
	int status;

	dir = etcd_dir_create_(etcd, parent, name);
	if(!dir)
	{
		return NULL;
	}
	ch = etcd_curl_put_(etcd, dir->uri, (mustexist ? existdata : data));
	status = etcd_curl_perform_(ch);
	etcd_curl_done_(ch);
	if(status)
	{
		etcd_dir_close(dir);
		return NULL;
	}
	return dir;
}

void
etcd_dir_close(ETCDDIR *dir)
{
	if(dir)
	{
		uri_destroy(dir->uri);
		uri_destroy(dir->base);
	}
	free(dir);
}
