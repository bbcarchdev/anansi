/* Author: Mo McRoberts <mo.mcroberts@bbc.co.uk>
 *
 * Copyright (c) 2014 BBC
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

#ifndef LIBS3CLIENT_H_
# define LIBS3CLIENT_H_                 1

/* Sign an AWS request, appending a suitable 'Authorization: AWS ...' header
 * to the list provided.
 */
struct curl_slist *s3_sign(const char *method, const char *resource, const char *access_key, const char *secret, struct curl_slist *headers);

#endif /*!LIBS3CLIENT_H_*/
