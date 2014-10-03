/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright 2014 BBC.
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

#ifndef P_PROCESSORS_H_
# define P_PROCESSORS_H_                1

# include "p_crawld.h"

# include <librdf.h>

typedef int (*rdf_filter_cb)(PROCESSOR *me, CRAWLOBJ *obj, const char *uri, librdf_model *model);

extern librdf_world *rdf_world(PROCESSOR *me);
extern int rdf_set_filter(PROCESSOR *me, rdf_filter_cb filter);

#endif /*!P_PROCESSORS_H_*/
