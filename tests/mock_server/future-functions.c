
/*
 * Define two sets of functions. A function in the first set, like
 * background_mongoc_cursor_next, runs a driver operation on a background
 * thread. One in the second set, like future_mongoc_cursor_next, launches
 * the background operation and returns a future_t that will resolve when
 * the operation finishes.
 *
 * These are used with mock_server_t so you can run the driver on a thread
 * while controlling the server from the main thread.
 */

#include "future-functions.h"


static void *
background_bulk_operation_execute (void *data)
{
   future_t *future = (future_t *) data;

   /* copy the future so we can unlock it while calling
    * bulk_operation_execute
    */
   future_t *copy = future_new_copy (future);
   future_value_t return_value;

   return_value.type = future_value_uint32_t_type;
 
   future_value_set_uint32_t (
      &return_value,
      mongoc_bulk_operation_execute (

         future_value_get_mongoc_bulk_operation_ptr (future_get_param(copy, 0)),
         future_value_get_bson_ptr (future_get_param(copy, 1)),
         future_value_get_bson_error_ptr (future_get_param(copy, 2))
   ));
 
   future_destroy (copy);
   future_resolve (future, return_value);
 
   return NULL;
}

static void *
background_cursor_next (void *data)
{
   future_t *future = (future_t *) data;

   /* copy the future so we can unlock it while calling
    * cursor_next
    */
   future_t *copy = future_new_copy (future);
   future_value_t return_value;

   return_value.type = future_value_bool_type;
 
   future_value_set_bool (
      &return_value,
      mongoc_cursor_next (

         future_value_get_mongoc_cursor_ptr (future_get_param(copy, 0)),
         future_value_get_const_bson_ptr_ptr (future_get_param(copy, 1))
   ));
 
   future_destroy (copy);
   future_resolve (future, return_value);
 
   return NULL;
}

static void *
background_client_get_database_names (void *data)
{
   future_t *future = (future_t *) data;

   /* copy the future so we can unlock it while calling
    * client_get_database_names
    */
   future_t *copy = future_new_copy (future);
   future_value_t return_value;

   return_value.type = future_value_char_ptr_ptr_type;
 
   future_value_set_char_ptr_ptr (
      &return_value,
      mongoc_client_get_database_names (

         future_value_get_mongoc_client_ptr (future_get_param(copy, 0)),
         future_value_get_bson_error_ptr (future_get_param(copy, 1))
   ));
 
   future_destroy (copy);
   future_resolve (future, return_value);
 
   return NULL;
}

static void *
background_database_get_collection_names (void *data)
{
   future_t *future = (future_t *) data;

   /* copy the future so we can unlock it while calling
    * database_get_collection_names
    */
   future_t *copy = future_new_copy (future);
   future_value_t return_value;

   return_value.type = future_value_char_ptr_ptr_type;
 
   future_value_set_char_ptr_ptr (
      &return_value,
      mongoc_database_get_collection_names (

         future_value_get_mongoc_database_ptr (future_get_param(copy, 0)),
         future_value_get_bson_error_ptr (future_get_param(copy, 1))
   ));
 
   future_destroy (copy);
   future_resolve (future, return_value);
 
   return NULL;
}



future_t *
future_bulk_operation_execute (
   mongoc_bulk_operation_ptr bulk,
   bson_ptr reply,
   bson_error_ptr error)
{
   future_t *future = future_new (future_value_uint32_t_type,
                                  3);
   
   future_value_set_mongoc_bulk_operation_ptr (
      future_get_param (future, 0), bulk);
   
   future_value_set_bson_ptr (
      future_get_param (future, 1), reply);
   
   future_value_set_bson_error_ptr (
      future_get_param (future, 2), error);
   
   future_start (future, background_bulk_operation_execute);
   return future;
}

future_t *
future_cursor_next (
   mongoc_cursor_ptr cursor,
   const_bson_ptr_ptr doc)
{
   future_t *future = future_new (future_value_bool_type,
                                  2);
   
   future_value_set_mongoc_cursor_ptr (
      future_get_param (future, 0), cursor);
   
   future_value_set_const_bson_ptr_ptr (
      future_get_param (future, 1), doc);
   
   future_start (future, background_cursor_next);
   return future;
}

future_t *
future_client_get_database_names (
   mongoc_client_ptr client,
   bson_error_ptr error)
{
   future_t *future = future_new (future_value_char_ptr_ptr_type,
                                  2);
   
   future_value_set_mongoc_client_ptr (
      future_get_param (future, 0), client);
   
   future_value_set_bson_error_ptr (
      future_get_param (future, 1), error);
   
   future_start (future, background_client_get_database_names);
   return future;
}

future_t *
future_database_get_collection_names (
   mongoc_database_ptr database,
   bson_error_ptr error)
{
   future_t *future = future_new (future_value_char_ptr_ptr_type,
                                  2);
   
   future_value_set_mongoc_database_ptr (
      future_get_param (future, 0), database);
   
   future_value_set_bson_error_ptr (
      future_get_param (future, 1), error);
   
   future_start (future, background_database_get_collection_names);
   return future;
}
