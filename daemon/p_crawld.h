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

#ifndef P_CRAWLD_H_
# define P_CRAWLD_H_                   1

# define _BSD_SOURCE                   1
# define _FILE_OFFSET_BITS             64

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <ctype.h>
# include <errno.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h>
# include <syslog.h>
# include <signal.h>
# include <pthread.h>
# include <uuid/uuid.h>

# include "libcrawld.h"
# include "libsupport.h"
# include "liburi.h"
# include "libetcd.h"

# define CRAWLER_APP_NAME               "crawler"

# define REGISTRY_KEY_TTL               120
# define REGISTRY_REFRESH               30

extern volatile int crawld_terminate;

int thread_init(void);
int thread_cleanup(void);
int thread_create(int crawler_offset);
int thread_run(void);
int thread_terminate(void);

int cluster_init(void);
int cluster_threads(void);
int cluster_inst_id(void);
int cluster_inst_threads(void);
const char *cluster_env(void);
int cluster_detached(void);

#endif /*!P_CRAWLD_H_*/
