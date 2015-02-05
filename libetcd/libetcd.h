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

#ifndef LIBETCD_H_
# define LIBETCD_H_                     1

# include "liburi.h"
# include "jsondata.h"

typedef struct etcd_struct ETCD;
typedef struct etcddir_struct ETCDDIR;

ETCD *etcd_connect(const char *url);
ETCD *etcd_connect_uri(const URI *uri);
void etcd_disconnect(ETCD *etcd);

ETCDDIR *etcd_dir_open(ETCD *etcd, ETCDDIR *parent, const char *name);
ETCDDIR *etcd_dir_create(ETCD *etcd, ETCDDIR *parent, const char *name, int mustexist);
int etcd_dir_delete(ETCD *etcd, const char *name, int recurse);
int etcd_dir_list(ETCD *etcd, const char *name, jd_var *out);
void etcd_dir_close(ETCDDIR *dir);

int etcd_key_set(ETCD *etcd, ETCDDIR *dir, const char *name, const char *value);
int etcd_key_set_ttl(ETCD *etcd, ETCDDIR *dir, const char *name, const char *value, int ttl);
int etcd_key_set_data_ttl(ETCD *etcd, ETCDDIR *dir, const char *name, const unsigned char *data, size_t len, int ttl);

#endif /*!LIBETCD_H_*/