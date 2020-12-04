/*
 * Copyright 2020-present MongoDB, Inc.
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

#include "entity-map.h"

#include "bson-parser.h"
#include "TestSuite.h"
#include "test-conveniences.h"
#include "utlist.h"

static void
entity_destroy (entity_t *entity);

entity_map_t *
entity_map_new ()
{
   return bson_malloc0 (sizeof (entity_map_t));
}

void
entity_map_destroy (entity_map_t *entity_map)
{
   entity_t *entity, *tmp;
   LL_FOREACH_SAFE (entity_map->entities, entity, tmp)
   {
      entity_destroy (entity);
   }
   bson_free (entity_map);
}

entity_t *
entity_client_new (bson_t *bson, bson_error_t *error)
{
   bson_parser_t *parser;
   entity_t *entity;

   entity = bson_malloc0 (sizeof (*entity));
   parser = bson_parser_new ();
   bson_parser_utf8 (parser, "id", &entity->id);
   bson_parser_doc_optional (parser, "uriOptions", &entity->uri_options);
   bson_parser_bool_optional (
      parser, "useMultipleMongoses", &entity->use_multiple_mongoses);
   bson_parser_array_optional (
      parser, "observeEvents", &entity->observe_events);
   bson_parser_array_optional (parser,
                               "ignoreCommandMonitoringEvents",
                               &entity->ignore_command_monitoring_events);

   if (!bson_parser_parse (parser, bson, error)) {
      entity_destroy (entity);
      entity = NULL;
      return NULL;
   }
   bson_parser_destroy (parser);
   return entity;
}

entity_t *
entity_database_new (bson_t *bson, bson_error_t *error)
{
   bson_parser_t *parser;
   entity_t *entity;

   entity = bson_malloc0 (sizeof (*entity));
   parser = bson_parser_new ();
   bson_parser_utf8 (parser, "id", &entity->id);
   bson_parser_utf8 (parser, "client", &entity->client);
   bson_parser_utf8 (parser, "databaseName", &entity->database_name);

   if (!bson_parser_parse (parser, bson, error)) {
      entity_destroy (entity);
      entity = NULL;
      return NULL;
   }
   bson_parser_destroy (parser);
   return entity;
}

entity_t *
entity_collection_new (bson_t *bson, bson_error_t *error)
{
   bson_parser_t *parser;
   entity_t *entity;

   entity = bson_malloc0 (sizeof (*entity));
   parser = bson_parser_new ();
   bson_parser_utf8 (parser, "id", &entity->id);
   bson_parser_utf8 (parser, "database", &entity->database);
   bson_parser_utf8 (parser, "collectionName", &entity->collection_name);
   if (!bson_parser_parse (parser, bson, error)) {
      entity_destroy (entity);
      entity = NULL;
      return NULL;
   }
   bson_parser_destroy (parser);
   return entity;
}

bool
entity_map_create (entity_map_t *entity_map, bson_t *bson, bson_error_t *error)
{
   bson_iter_t iter;
   const char *entity_type;
   bson_t entity_bson;
   entity_t *entity;

   bson_iter_init (&iter, bson);
   if (!bson_iter_next (&iter)) {
      test_set_error (error, "Empty entity");
      return false;
   }

   entity_type = bson_iter_key (&iter);
   bson_iter_bson (&iter, &entity_bson);
   if (bson_iter_next (&iter)) {
      test_set_error (error,
                      "Extra field in entity: %s: %s",
                      bson_iter_key (&iter),
                      tmp_json (bson));
      return false;
   }

   if (0 == strcmp (entity_type, "client")) {
      entity = entity_client_new (&entity_bson, error);
   } else if (0 == strcmp (entity_type, "database")) {
      entity = entity_database_new (&entity_bson, error);
   } else if (0 == strcmp (entity_type, "collection")) {
      entity = entity_collection_new (&entity_bson, error);
   } else {
      test_set_error (
         error, "Unknown entity type: %s: %s", entity_type, tmp_json (bson));
      return false;
   }

   if (!entity) {
      return false;
   }

   entity->type = bson_strdup (entity_type);
   LL_PREPEND (entity_map->entities, entity);
   return true;
}

static void
entity_destroy (entity_t *entity)
{
   if (!entity) {
      return;
   }
   bson_destroy (entity->uri_options);
   bson_free (entity->use_multiple_mongoses);
   bson_destroy (entity->observe_events);
   bson_destroy (entity->ignore_command_monitoring_events);
   bson_free (entity->client);
   bson_free (entity->database_name);
   bson_free (entity->database);
   bson_free (entity->collection_name);
   bson_free (entity->type);
   bson_free (entity->id);
   bson_free (entity);
}