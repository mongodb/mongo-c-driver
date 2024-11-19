/* gcc example-structured-log.c -o example-structured-log \
 *     $(pkg-config --cflags --libs libmongoc-1.0) */

#include <mongoc/mongoc.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

static pthread_mutex_t handler_mutex;

static void
example_handler (const mongoc_structured_log_entry_t *entry, void *user_data)
{
   mongoc_structured_log_component_t component = mongoc_structured_log_entry_get_component (entry);
   mongoc_structured_log_level_t level = mongoc_structured_log_entry_get_level (entry);

   /*
    * Structured log handlers need to be thread-safe.
    * Many apps will be happy to use a global mutex in their logging handler,
    * but high performance multithreaded apps may prefer dispatching log
    * messages asynchronously with thread-safe data structures.
    */
   pthread_mutex_lock (&handler_mutex);

   printf ("Log component=%s level=%s\n",
           mongoc_structured_log_get_component_name (component),
           mongoc_structured_log_get_level_name (level));

   /*
    * At this point, the handler might make additional filtering decisions
    * before asking for a bson_t. As an example, let's log the component and
    * level for all messages but only show contents for command logs.
    */
   if (component == MONGOC_STRUCTURED_LOG_COMPONENT_COMMAND) {
      bson_t *message = mongoc_structured_log_entry_message_as_bson (entry);
      char *json = bson_as_relaxed_extended_json (message, NULL);
      printf ("Log body: %s\n", json);
      bson_destroy (message);
      bson_free (json);
   }

   pthread_mutex_unlock (&handler_mutex);
}

int
main (void)
{
   const char *uri_string = "mongodb://localhost:27017";
   int result = EXIT_FAILURE;
   bson_error_t error;
   mongoc_uri_t *uri = NULL;
   mongoc_client_t *client = NULL;

   /*
    * For demonstration purposes, set up a handler that receives all possible log messages.
    */
   pthread_mutex_init (&handler_mutex, NULL);
   mongoc_structured_log_set_max_level_for_all_components (MONGOC_STRUCTURED_LOG_LEVEL_TRACE);
   mongoc_structured_log_set_handler (example_handler, NULL);

   /*
    * By default libmongoc proceses log options from the environment first,
    * and then allows you to apply programmatic overrides. To request the
    * opposite behavior, allowing the environment to override programmatic
    * defaults, you can ask for the environment to be re-read after setting
    * your own defaults.
    */
   mongoc_structured_log_set_max_levels_from_env ();

   /*
    * This is the main libmongoc initialization, but structured logging
    * can be used earlier. It's automatically initialized on first use.
    */
   mongoc_init ();

   /*
    * Create a MongoDB URI object. This example assumes a local server.
    */
   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      fprintf (stderr, "URI parse error: %s\n", error.message);
      goto done;
   }

   /*
    * Create a new client instance.
    */
   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      goto done;
   }

   /*
    * Do some work that we'll see logs from. This example just sends a 'ping' command.
    */
   bson_t *command = BCON_NEW ("ping", BCON_INT32 (1));
   bson_t reply;
   bool command_ret = mongoc_client_command_simple (client, "admin", command, NULL, &reply, &error);
   bson_destroy (command);
   bson_destroy (&reply);
   if (!command_ret) {
      fprintf (stderr, "Command error: %s\n", error.message);
      goto done;
   }

   result = EXIT_SUCCESS;
done:
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);
   mongoc_cleanup ();
   return result;
}
