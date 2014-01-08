/*
 * Copyright 2013 10gen Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef MONGOC_SASL_PRIVATE_H
#define MONGOC_SASL_PRIVATE_H


#include <bson.h>
#include <sasl/sasl.h>


BSON_BEGIN_DECLS


typedef struct _mongoc_sasl_client_t mongoc_sasl_client_t;


struct _mongoc_sasl_client_t
{
   char            *service_name;
   char            *service_hostname;
   char            *mechanism;
   char            *user;
   char            *password;
   sasl_conn_t     *conn;
   sasl_callback_t  callbacks [4];
   int              step;
   int              done;
};


bson_bool_t
_mongoc_sasl_client_init (mongoc_sasl_client_t *client,
                          const char           *service_name,
                          const char           *service_hostname,
                          const char           *mechanism,
                          const char           *user,
                          const char           *password);


int
_mongoc_sasl_client_step (mongoc_sasl_client_t  *client,
                          const char            *instr,
                          char                 **outstr);


bson_bool_t
_mongoc_sasl_client_is_done (mongoc_sasl_client_t *client);


void
_mongoc_sasl_client_destroy (mongoc_sasl_client_t *client);


BSON_END_DECLS


#endif /* MONGOC_SASL_PRIVATE_H */
