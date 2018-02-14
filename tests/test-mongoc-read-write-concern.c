#include <mongoc.h>

#include "mongoc-util-private.h"
#include "mongoc-write-concern-private.h"
#include "json-test.h"
#include "test-libmongoc.h"


static void
compare_write_concern (const bson_t *wc_doc, const mongoc_write_concern_t *wc)
{
   if (bson_has_field (wc_doc, "w")) {
      ASSERT_CMPINT32 (wc->w, ==, bson_lookup_int32 (wc_doc, "w"));
   } else {
      ASSERT_CMPINT32 (wc->w, ==, MONGOC_WRITE_CONCERN_W_DEFAULT);
   }

   if (bson_has_field (wc_doc, "wtimeoutMS")) {
      ASSERT_CMPINT32 (
         wc->wtimeout, ==, bson_lookup_int32 (wc_doc, "wtimeoutMS"));
   } else {
      ASSERT_CMPINT32 (wc->wtimeout, ==, 0);
   }

   if (bson_has_field (wc_doc, "journal")) {
      if (_mongoc_lookup_bool (wc_doc, "journal", false)) {
         ASSERT_CMPINT ((int) wc->journal, ==, 1);
      } else {
         ASSERT_CMPINT ((int) wc->journal, ==, 0);
      }
   } else {
      ASSERT_CMPINT (
         (int) wc->journal, ==, MONGOC_WRITE_CONCERN_JOURNAL_DEFAULT);
   }
}


static void
compare_read_concern (const bson_t *rc_doc, const mongoc_read_concern_t *rc)
{
   const char *level;

   if (bson_has_field (rc_doc, "level")) {
      level = bson_lookup_utf8 (rc_doc, "level");
      ASSERT_CMPSTR (level, mongoc_read_concern_get_level (rc));
   } else {
      BSON_ASSERT (!mongoc_read_concern_get_level (rc));
   }
}


static void
test_rw_concern_uri (bson_t *scenario)
{
   bson_iter_t scenario_iter;
   bson_iter_t test_iter;
   bson_t test;
   const char *description;
   const char *uri_str;
   bool valid;
   mongoc_uri_t *uri;
   bson_t rc_doc;
   const mongoc_read_concern_t *rc;
   bson_t wc_doc;
   const mongoc_write_concern_t *wc;

   BSON_ASSERT (bson_iter_init_find (&scenario_iter, scenario, "tests"));
   BSON_ASSERT (bson_iter_recurse (&scenario_iter, &test_iter));

   while (bson_iter_next (&test_iter)) {
      bson_iter_bson (&test_iter, &test);

      description = bson_lookup_utf8 (&test, "description");
      uri_str = bson_lookup_utf8 (&test, "uri");
      valid = _mongoc_lookup_bool (&test, "valid", true);

      if (_mongoc_lookup_bool (&test, "warning", false)) {
         MONGOC_ERROR ("update the \"%s\" test to handle warning: true",
                       description);
         abort ();
      }

      uri = mongoc_uri_new_with_error (uri_str, NULL);
      if (!valid) {
         BSON_ASSERT (!uri);
         return;
      }

      BSON_ASSERT (uri);

      if (bson_has_field (&test, "readConcern")) {
         rc = mongoc_uri_get_read_concern (uri);
         bson_lookup_doc (&test, "readConcern", &rc_doc);
         compare_read_concern (&rc_doc, rc);
      }

      if (bson_has_field (&test, "writeConcern")) {
         wc = mongoc_uri_get_write_concern (uri);
         bson_lookup_doc (&test, "writeConcern", &wc_doc);
         compare_write_concern (&wc_doc, wc);
      }

      mongoc_uri_destroy (uri);
   }
}


static void
test_rw_concern_document (bson_t *scenario)
{
   bson_iter_t scenario_iter;
   bson_iter_t test_iter;
   mongoc_read_write_opts_t read_write_opts;
   bson_error_t error;
   bson_t test;
   bool valid;
   bool r;
   bson_t rc_doc;
   bson_t wc_doc;

   BSON_ASSERT (bson_iter_init_find (&scenario_iter, scenario, "tests"));
   BSON_ASSERT (bson_iter_recurse (&scenario_iter, &test_iter));

   while (bson_iter_next (&test_iter)) {
      bson_iter_bson (&test_iter, &test);

      valid = _mongoc_lookup_bool (&test, "valid", true);
      r = _mongoc_read_write_opts_parse (
         NULL /* client */, &test, &read_write_opts, &error);

      if (!valid) {
         BSON_ASSERT (!r);
         _mongoc_read_write_opts_cleanup (&read_write_opts);
         return;
      }

      ASSERT_OR_PRINT (r, error);

      if (bson_has_field (&test, "readConcern")) {
         bson_lookup_doc (&test, "readConcern", &rc_doc);
         match_bson (
            &rc_doc, &read_write_opts.readConcern, false /* is_command */);
      }

      if (bson_has_field (&test, "writeConcern")) {
         bson_lookup_doc (&test, "writeConcern", &wc_doc);
         compare_write_concern (&wc_doc, read_write_opts.writeConcern);
      }

      _mongoc_read_write_opts_cleanup (&read_write_opts);
   }
}


void
test_read_write_concern_install (TestSuite *suite)
{
   char resolved[PATH_MAX];

   ASSERT (
      realpath (JSON_DIR "/read_write_concern/connection-string", resolved));
   install_json_test_suite (suite, resolved, &test_rw_concern_uri);

   ASSERT (realpath (JSON_DIR "/read_write_concern/document", resolved));
   install_json_test_suite (suite, resolved, &test_rw_concern_document);
}
