// An example implementation of custom resume logic in a change stream. example-resume starts a client-wide change
// stream and persists the resume state in a file "resume-state.json". On restart, if "resume-state.json" exists, the
// change stream starts watching after the persisted resume token using the same pipeline and options as the original
// change stream.
//
// This behavior allows a user to exit example-resume, and restart it later without missing any change events.

#include <mongoc/mongoc.h>

#include <bson/bson.h>

#include <unistd.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const char *RESUME_STATE_PATH = "resume-state.json";

static bool
_save_resume_state(const bson_t *pipeline, const bson_t *watch_opts, const bson_t *doc)
{
   bson_iter_t iter = {0};
   if (!bson_iter_init_find(&iter, doc, "_id")) {
      fprintf(stderr, "change stream event does not contain a resume token (_id).");
      return false;
   }
   const bson_value_t *const resume_token = bson_iter_value(&iter);

   // Store the resume token, pipeline, and options.
   bson_t state = BSON_INITIALIZER;
   {
      bson_t opts = BSON_INITIALIZER;
      bson_copy_to_excluding_noinit(watch_opts, &opts, "resumeAfter", NULL);
      BSON_APPEND_VALUE(&opts, "resumeAfter", resume_token);
      BSON_APPEND_ARRAY(&state, "pipeline", pipeline);
      BSON_APPEND_DOCUMENT(&state, "options", &opts);
      bson_destroy(&opts);
   }

   size_t as_json_len = 0u;
   char *const as_json = bson_as_canonical_extended_json(&state, &as_json_len);
   if (!as_json) {
      fprintf(stderr, "failed to convert the resume state into a JSON string\n");
      bson_destroy(&state);
      return false;
   }

   FILE *const file_stream = fopen(RESUME_STATE_PATH, "w+");
   if (!file_stream) {
      fprintf(stderr, "failed to open '%s' for writing\n", RESUME_STATE_PATH);
      bson_free(as_json);
      bson_destroy(&state);
      return false;
   }
   size_t n_written = 0;
   while (n_written < as_json_len) {
      size_t r = fwrite((void *)(as_json + n_written), sizeof(char), as_json_len - n_written, file_stream);
      if (r == 0) {
         fprintf(stderr, "failed to write to %s\n", RESUME_STATE_PATH);
         bson_free(as_json);
         fclose(file_stream);
         bson_destroy(&state);
         return false;
      }
      n_written += r;
   }

   fclose(file_stream);
   bson_free(as_json);
   bson_destroy(&state);

   return true;
}

// Copies the embedded document (or array) stored at "key" in `*state` into `*dst`
// which must be an initialized and empty `bson_t`.
static bool
_copy_resume_state_field(const bson_t *state, const char *key, bson_type_t type, const char *description, bson_t *dst)
{
   bson_iter_t iter = {0};
   if (!bson_iter_init_find(&iter, state, key) || bson_iter_type(&iter) != type) {
      fprintf(stderr, "missing field '%s' in resume state '%s'\n", description, RESUME_STATE_PATH);
      return false;
   }

   const uint8_t *data = NULL;
   uint32_t len = 0u;
   if (type == BSON_TYPE_ARRAY) {
      bson_iter_array(&iter, &len, &data);
   } else {
      bson_iter_document(&iter, &len, &data);
   }

   {
      bson_t view = {0};
      if (!bson_init_static(&view, data, len)) {
         fprintf(stderr, "field '%s' in resume state '%s' is invalid\n", description, RESUME_STATE_PATH);
         return false;
      }
      bson_concat(dst, &view);
   }

   return true;
}

// Load the resume state fields "pipeline" and "options" into `pipeline` and `opts` respectively.
static bool
_load_resume_state(bson_t *pipeline, bson_t *opts)
{
   // if the file does not exist, create a new change stream.
   if (-1 == access(RESUME_STATE_PATH, R_OK)) {
      return true;
   }

   bson_error_t error = {0};
   bson_json_reader_t *const reader = bson_json_reader_new_from_file(RESUME_STATE_PATH, &error);
   if (!reader) {
      fprintf(stderr, "failed to open %s for reading: %s\n", RESUME_STATE_PATH, error.message);
      return false;
   }

   bson_t state = BSON_INITIALIZER;
   if (-1 == bson_json_reader_read(reader, &state, &error)) {
      fprintf(stderr, "failed to read doc from %s\n", RESUME_STATE_PATH);
      bson_destroy(&state);
      bson_json_reader_destroy(reader);
      return false;
   }
   bson_json_reader_destroy(reader);

   const bool loaded = _copy_resume_state_field(&state, "pipeline", BSON_TYPE_ARRAY, "\"pipeline\" array", pipeline) &&
                       _copy_resume_state_field(&state, "options", BSON_TYPE_DOCUMENT, "\"options\" document", opts);
   if (loaded) {
      printf("found cached resume state in '%s', resuming change stream.\n", RESUME_STATE_PATH);
   }

   bson_destroy(&state);
   return loaded;
}

int
main(void)
{
   int ret = EXIT_FAILURE;

   mongoc_init();

   mongoc_uri_t *uri = NULL;
   mongoc_client_t *client = NULL;
   bson_t pipeline = BSON_INITIALIZER;
   bson_t opts = BSON_INITIALIZER;
   mongoc_change_stream_t *stream = NULL;

   const int max_time = 30; // max amount of time, in seconds, that `mongoc_change_stream_next()` can block.

   bson_error_t error = {0};
   uri = mongoc_uri_new_with_error("mongodb://localhost:27017/db?replicaSet=rs0", &error);
   if (!uri) {
      fprintf(stderr,
              "failed to parse URI:\n"
              "error message: %s\n",
              error.message);
      goto cleanup;
   }

   client = mongoc_client_new_from_uri(uri);
   if (!client) {
      goto cleanup;
   }

   if (!_load_resume_state(&pipeline, &opts)) {
      goto cleanup;
   }

   if (!bson_has_field(&opts, "maxAwaitTimeMS")) {
      BSON_APPEND_INT64(&opts, "maxAwaitTimeMS", max_time * 1000);
   }

   printf("listening for changes on the client (max %d seconds).\n", max_time);
   stream = mongoc_client_watch(client, &pipeline, &opts);

   {
      const bson_t *doc = NULL;
      while (mongoc_change_stream_next(stream, &doc)) {
         char *as_json;

         as_json = bson_as_canonical_extended_json(doc, NULL);
         printf("change received: %s\n", as_json);
         bson_free(as_json);
         if (!_save_resume_state(&pipeline, &opts, doc)) {
            goto cleanup;
         }
      }
   }

   ret = EXIT_SUCCESS;

cleanup:
   mongoc_uri_destroy(uri);
   bson_destroy(&pipeline);
   bson_destroy(&opts);
   mongoc_change_stream_destroy(stream);
   mongoc_client_destroy(client);

   mongoc_cleanup();

   return ret;
}
