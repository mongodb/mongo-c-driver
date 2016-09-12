/*
 * Copyright 2015 MongoDB, Inc.
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

#ifndef MONGOC_APM_H
#define MONGOC_APM_H

#if !defined (MONGOC_INSIDE) && !defined (MONGOC_COMPILATION)
# error "Only <mongoc.h> can be included directly."
#endif

#include <bson.h>
#include "mongoc-host-list.h"

BSON_BEGIN_DECLS

/*
 * An Application Performance Management (APM) interface, complying with
 * MongoDB's Command Monitoring Spec:
 *
 * https://github.com/mongodb/specifications/tree/master/source/command-monitoring
 */

/*
 * callbacks to receive APM events
 */

typedef struct _mongoc_apm_callbacks_t mongoc_apm_callbacks_t;


/*
 * events: command started, succeeded, or failed
 */

typedef struct _mongoc_apm_command_started_t   mongoc_apm_command_started_t;
typedef struct _mongoc_apm_command_succeeded_t mongoc_apm_command_succeeded_t;
typedef struct _mongoc_apm_command_failed_t    mongoc_apm_command_failed_t;


/*
 * event field accessors
 */

/* command-started event fields */

const bson_t *
mongoc_apm_command_started_get_command        (const mongoc_apm_command_started_t *event);
const char   *
mongoc_apm_command_started_get_database_name  (const mongoc_apm_command_started_t *event);
const char   *
mongoc_apm_command_started_get_command_name   (const mongoc_apm_command_started_t *event);
int64_t
mongoc_apm_command_started_get_request_id     (const mongoc_apm_command_started_t *event);
int64_t
mongoc_apm_command_started_get_operation_id   (const mongoc_apm_command_started_t *event);
const mongoc_host_list_t *
mongoc_apm_command_started_get_host           (const mongoc_apm_command_started_t *event);
uint32_t
mongoc_apm_command_started_get_server_id      (const mongoc_apm_command_started_t *event);
void *
mongoc_apm_command_started_get_context        (const mongoc_apm_command_started_t *event);

/* command-succeeded event fields */

int64_t
mongoc_apm_command_succeeded_get_duration     (const mongoc_apm_command_succeeded_t *event);
const bson_t *
mongoc_apm_command_succeeded_get_reply        (const mongoc_apm_command_succeeded_t *event);
const char *
mongoc_apm_command_succeeded_get_command_name (const mongoc_apm_command_succeeded_t *event);
int64_t
mongoc_apm_command_succeeded_get_request_id   (const mongoc_apm_command_succeeded_t *event);
int64_t
mongoc_apm_command_succeeded_get_operation_id (const mongoc_apm_command_succeeded_t *event);
const mongoc_host_list_t *
mongoc_apm_command_succeeded_get_host         (const mongoc_apm_command_succeeded_t *event);
uint32_t
mongoc_apm_command_succeeded_get_server_id    (const mongoc_apm_command_succeeded_t *event);
void *
mongoc_apm_command_succeeded_get_context      (const mongoc_apm_command_succeeded_t *event);

/* command-failed event fields */

int64_t
mongoc_apm_command_failed_get_duration        (const mongoc_apm_command_failed_t *event);
const char *
mongoc_apm_command_failed_get_command_name    (const mongoc_apm_command_failed_t *event);
/* retrieve the error by filling out the passed-in "error" struct */
void
mongoc_apm_command_failed_get_error           (const mongoc_apm_command_failed_t *event,
                                               bson_error_t *error);
int64_t
mongoc_apm_command_failed_get_request_id      (const mongoc_apm_command_failed_t *event);
int64_t
mongoc_apm_command_failed_get_operation_id    (const mongoc_apm_command_failed_t *event);
const mongoc_host_list_t *
mongoc_apm_command_failed_get_host            (const mongoc_apm_command_failed_t *event);
uint32_t
mongoc_apm_command_failed_get_server_id       (const mongoc_apm_command_failed_t *event);
void *
mongoc_apm_command_failed_get_context         (const mongoc_apm_command_failed_t *event);

/*
 * callbacks
 */

typedef void
(*mongoc_apm_command_started_cb_t)   (const mongoc_apm_command_started_t   *event);
typedef void
(*mongoc_apm_command_succeeded_cb_t) (const mongoc_apm_command_succeeded_t *event);
typedef void
(*mongoc_apm_command_failed_cb_t)    (const mongoc_apm_command_failed_t    *event);

/*
 * registering callbacks
 */

mongoc_apm_callbacks_t *
mongoc_apm_callbacks_new             (void);
void
mongoc_apm_callbacks_destroy         (mongoc_apm_callbacks_t            *callbacks);
void
mongoc_apm_set_command_started_cb    (mongoc_apm_callbacks_t            *callbacks,
                                      mongoc_apm_command_started_cb_t    cb);
void
mongoc_apm_set_command_succeeded_cb  (mongoc_apm_callbacks_t            *callbacks,
                                      mongoc_apm_command_succeeded_cb_t  cb);
void
mongoc_apm_set_command_failed_cb     (mongoc_apm_callbacks_t            *callbacks,
                                      mongoc_apm_command_failed_cb_t     cb);

BSON_END_DECLS

#endif /* MONGOC_APM_H */
