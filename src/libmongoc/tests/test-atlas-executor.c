#include "TestSuite.h"

#include "unified/operation.h"
#include "unified/runner.h"

#include "json-test.h"
#include "test-libmongoc.h"

#include <bson/bson.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>

static void
TestSuite_Init_Atlas (TestSuite *suite, int argc, char **argv)
{
   ASSERT_WITH_MSG (argc > 1, "test-atlas-executor requires a workload spec!");

   *suite = (TestSuite){
      .ctest_run = NULL,
      .failing_flaky_skips = {0},
      .flags = TEST_NOFORK,
      .match_patterns = {0},
      .mock_server_log = NULL,
      .mock_server_log_buf = NULL,
      .name = bson_strdup ("/atlas"),
      .outfile = NULL,
      .prgname = bson_strdup (argv[0]),
      .silent = false,
      .tests = NULL,
   };
}

bson_t *
workload_spec_to_bson (const char *workload_spec)
{
   BSON_ASSERT_PARAM (workload_spec);

   bson_t *const res = bson_new ();

   const size_t workload_spec_len = strlen (workload_spec);
   const size_t bson_json_default_buf_size = 1u << 14; // From bson-json.c.

   bson_json_reader_t *const reader =
      bson_json_data_reader_new (false, bson_json_default_buf_size);
   bson_json_data_reader_ingest (
      reader, (const uint8_t *) workload_spec, workload_spec_len);


   bson_error_t error;
   ASSERT_OR_PRINT (bson_json_reader_read (reader, res, &error) == 1, error);
   bson_json_reader_destroy (reader);

   return res;
}

// Used to ensure that repeated SIGINT are not ignored.
void (*original_sigint_handler) (int) = NULL;

static void
sigint_handler (int sigint)
{
   assert (sigint == SIGINT);
   operation_loop_terminated = true;
   signal (SIGINT, original_sigint_handler);
}

static void
TestSuite_Run_Atlas (TestSuite *suite)
{
   BSON_ASSERT_PARAM (suite);

   Test *const test = suite->tests;

   ASSERT_WITH_MSG (test, "missing expected test in test suite");
   ASSERT_WITH_MSG (!test->next, "expected exactly one test in test suite");

   original_sigint_handler = signal (SIGINT, sigint_handler);

   srand (test->seed);

   test_conveniences_init ();
   test->func (test->ctx);
   test_conveniences_cleanup ();

   capture_logs (false);
}

int
main (int argc, char **argv)
{
   ASSERT_WITH_MSG (argc > 1, "test-atlas-executor requires a workload spec!");

   TestSuite suite = {0};
   TestSuite_Init_Atlas (&suite, argc, argv);

   bson_t *const bson = workload_spec_to_bson (argv[1]);

   TestSuite_AddFull (&suite,
                      "test",
                      (TestFuncWC) &run_one_test_file,
                      (TestFuncDtor) &bson_destroy,
                      bson,
                      TestSuite_CheckLive,
                      NULL);

   mongoc_init ();
   TestSuite_Run_Atlas (&suite);
   mongoc_cleanup ();

   TestSuite_Destroy (&suite);

   return 0;
}
