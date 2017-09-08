/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014-2017 BBC
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

#ifndef P_ANANSI_PLUGIN_H_
# define P_ANANSI_PLUGIN_H_            1

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <errno.h>
# include <ctype.h>

# include "liburi.h"
# include "libsql.h"
# include "libmq.h"
# include "libmq-engine.h"
# include "libcluster.h"

# define PLUGIN_NAME                   "Anansi"
# define ANANSI_URL_MIME               "application/x-anansi-url"

#endif /*!P_ANANSI_PLUGIN_H_*/
