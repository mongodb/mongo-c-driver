/*
 * Copyright 2024-present MongoDB, Inc.
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

#include <bson/bson.h>
#include <mongoc/mongoc-error.h>
#include <mongoc-bulkwrite.h>

struct _mongoc_bulkwriteoptions_t {
   bool ordered;
   bool bypassdocumentvalidation;
   const bson_t *let;
   const mongoc_write_concern_t *writeconcern;
   bool verboseresults;
   const bson_t *comment;
   mongoc_client_session_t *session;
   const bson_t *extra;
   uint32_t serverid;
};

mongoc_bulkwriteoptions_t *
mongoc_bulkwriteoptions_new (void)
{
   return bson_malloc0 (sizeof (mongoc_bulkwriteoptions_t));
}
void
mongoc_bulkwriteoptions_set_ordered (mongoc_bulkwriteoptions_t *self, bool ordered)
{
   BSON_ASSERT_PARAM (self);
   self->ordered = ordered;
}
void
mongoc_bulkwriteoptions_set_bypassdocumentvalidation (mongoc_bulkwriteoptions_t *self, bool bypassdocumentvalidation)
{
   BSON_ASSERT_PARAM (self);
   self->bypassdocumentvalidation = bypassdocumentvalidation;
}
void
mongoc_bulkwriteoptions_set_let (mongoc_bulkwriteoptions_t *self, const bson_t *let)
{
   BSON_ASSERT_PARAM (self);
   self->let = let;
}
void
mongoc_bulkwriteoptions_set_writeconcern (mongoc_bulkwriteoptions_t *self, const mongoc_write_concern_t *writeconcern)
{
   BSON_ASSERT_PARAM (self);
   self->writeconcern = writeconcern;
}
void
mongoc_bulkwriteoptions_set_verboseresults (mongoc_bulkwriteoptions_t *self, bool verboseresults)
{
   BSON_ASSERT_PARAM (self);
   self->verboseresults = verboseresults;
}
void
mongoc_bulkwriteoptions_set_comment (mongoc_bulkwriteoptions_t *self, const bson_t *comment)
{
   BSON_ASSERT_PARAM (self);
   self->comment = comment;
}
void
mongoc_bulkwriteoptions_set_session (mongoc_bulkwriteoptions_t *self, mongoc_client_session_t *session)
{
   BSON_ASSERT_PARAM (self);
   self->session = session;
}
void
mongoc_bulkwriteoptions_set_extra (mongoc_bulkwriteoptions_t *self, const bson_t *extra)
{
   BSON_ASSERT_PARAM (self);
   self->extra = extra;
}
BSON_EXPORT (void)
mongoc_bulkwriteoptions_set_serverid (mongoc_bulkwriteoptions_t *self, uint32_t serverid)
{
   BSON_ASSERT_PARAM (self);
   self->serverid = serverid;
}
void
mongoc_bulkwriteoptions_destroy (mongoc_bulkwriteoptions_t *self)
{
   if (!self) {
      return;
   }
   bson_free (self);
}


struct _mongoc_bulkwrite_t {
   mongoc_client_t *client;
   bool executed;
};


// `mongoc_client_bulkwrite_new` creates a new bulk write operation.
mongoc_bulkwrite_t *
mongoc_client_bulkwrite_new (mongoc_client_t *self, mongoc_bulkwriteoptions_t *opts)
{
   BSON_ASSERT_PARAM (self);
   BSON_UNUSED (opts);
   mongoc_bulkwrite_t *bw = bson_malloc0 (sizeof (mongoc_bulkwrite_t));
   bw->client = self;
   return bw;
}

void
mongoc_bulkwrite_destroy (mongoc_bulkwrite_t *self)
{
   if (!self) {
      return;
   }
   bson_free (self);
}

struct _mongoc_insertoneopts_t {
   bson_validate_flags_t vflags;
};

mongoc_insertoneopts_t *
mongoc_insertoneopts_new (void)
{
   return bson_malloc0 (sizeof (mongoc_insertoneopts_t));
}

void
mongoc_insertoneopts_set_validation (mongoc_insertoneopts_t *self, bson_validate_flags_t vflags)
{
   BSON_ASSERT_PARAM (self);
   self->vflags = vflags;
}

void
mongoc_insertoneopts_destroy (mongoc_insertoneopts_t *self)
{
   if (!self) {
      return;
   }
   bson_free (self);
}

bool
mongoc_client_bulkwrite_append_insertone (mongoc_bulkwrite_t *self,
                                          const char *ns,
                                          int ns_len,
                                          const bson_t *document,
                                          mongoc_insertoneopts_t *opts, // may be NULL
                                          bson_error_t *error)
{
   BSON_ASSERT_PARAM (self);
   BSON_UNUSED (ns);
   BSON_UNUSED (ns_len);
   BSON_UNUSED (document);
   BSON_UNUSED (opts);

   if (self->executed) {
      bson_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "bulk write already executed");
      return false;
   }

   // TODO: implement.
   return true;
}


struct _mongoc_bulkwriteresult_t {
   int64_t acknowledged;
   int64_t insertedCount;
   int64_t upsertedcount;
   int64_t matchedcount;
   int64_t modifiedcount;
   int64_t deletedcount;
   bson_t *verbose_results;
   uint32_t serverid;
};

bool
mongoc_bulkwriteresult_acknowledged (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->acknowledged;
}

int64_t
mongoc_bulkwriteresult_insertedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->insertedCount;
}

int64_t
mongoc_bulkwriteresult_upsertedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->upsertedcount;
}

int64_t
mongoc_bulkwriteresult_matchedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->matchedcount;
}

int64_t
mongoc_bulkwriteresult_modifiedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->modifiedcount;
}

int64_t
mongoc_bulkwriteresult_deletedcount (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->deletedcount;
}

const bson_t *
mongoc_bulkwriteresult_verboseresults (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->verbose_results;
}

uint32_t
mongoc_bulkwriteresult_serverid (const mongoc_bulkwriteresult_t *self)
{
   BSON_ASSERT_PARAM (self);
   return self->serverid;
}

void
mongoc_bulkwriteresult_destroy (mongoc_bulkwriteresult_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->verbose_results);
   bson_free (self);
}

struct _mongoc_bulkwriteexception_t {
   bson_error_t error;
   bson_t *error_document;
};

void
mongoc_bulkwriteexception_error (const mongoc_bulkwriteexception_t *self,
                                 bson_error_t *error,
                                 const bson_t **error_document)
{
   BSON_ASSERT_PARAM (self);
   memcpy (error, &self->error, sizeof (*error));
   if (error_document) {
      *error_document = self->error_document;
   }
}

void
mongoc_bulkwriteexception_destroy (mongoc_bulkwriteexception_t *self)
{
   if (!self) {
      return;
   }
   bson_destroy (self->error_document);
   bson_free (self);
}

mongoc_bulkwritereturn_t
mongoc_bulkwrite_execute (mongoc_bulkwrite_t *self)
{
   BSON_ASSERT_PARAM (self);
   // TODO: implement.
   self->executed = true;
   // Create stub results.
   mongoc_bulkwriteresult_t *bwr = bson_malloc0 (sizeof (mongoc_bulkwriteresult_t));
   bwr->insertedCount = 123;
   bwr->verbose_results = bson_new_from_json ((const uint8_t *) BSON_STR ({"foo" : "bar"}), -1, NULL);
   BSON_ASSERT (bwr->verbose_results);

   mongoc_bulkwriteexception_t *bwe = bson_malloc0 (sizeof (mongoc_bulkwriteexception_t));
   bwe->error_document = bson_new_from_json (
      (const uint8_t *) BSON_STR (
         {"errorLabels" : ["RetryableWriteError"], "writeErrors" : [], "writeConcernErrors" : [], "errorReplies" : []}),
      -1,
      NULL);
   BSON_ASSERT (bwe->error_document);
   bson_set_error (&bwe->error, MONGOC_ERROR_SERVER, 123, "This is a stub error");
   return (mongoc_bulkwritereturn_t){.res = bwr, .exc = bwe};
}
