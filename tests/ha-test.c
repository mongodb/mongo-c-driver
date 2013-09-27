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


#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <mongoc.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ha-test.h"


struct _ha_replica_set_t
{
   char      *name;
   ha_node_t *nodes;
   int        next_port;
};


struct _ha_node_t
{
   ha_node_t     *next;
   char          *name;
   char          *repl_set;
   char          *dbpath;
   bson_bool_t    is_arbiter;
   pid_t          pid;
   bson_uint16_t  port;
};


static ha_node_t *
ha_node_new (const char    *name,
             const char    *repl_set,
             const char    *dbpath,
             bson_bool_t    is_arbiter,
             bson_uint16_t  port)
{
   ha_node_t *node;

   node = bson_malloc0(sizeof *node);
   node->name = strdup(name);
   node->repl_set = strdup(repl_set);
   node->dbpath = strdup(dbpath);
   node->is_arbiter = is_arbiter;
   node->port = port;

   return node;
}


void
ha_node_setup (ha_node_t *node)
{
   if (!!mkdir(node->dbpath, 0750)) {
      MONGOC_WARNING("Failed to create directory \"%s\"",
                     node->dbpath);
      abort();
   }
}


void
ha_node_kill (ha_node_t *node)
{
   if (node->pid) {
      int status;

      kill(node->pid, SIGKILL);
      waitpid(node->pid, &status, 0);
      node->pid = 0;
   }
}


void
ha_node_restart (ha_node_t *node)
{
   pid_t pid;
   char portstr[12];
   char *argv[14];
   int i;

   snprintf(portstr, sizeof portstr, "%hu", node->port);
   portstr[sizeof portstr - 1] = '\0';

   ha_node_kill(node);

   argv[0] = (char *) "mongod";
   argv[1] = (char *) "--dbpath";
   argv[2] = (char *) ".";
   argv[3] = (char *) "--port";
   argv[4] = portstr;
   argv[5] = (char *) "--replSet";
   argv[6] = node->repl_set;
   argv[7] = (char *) "--nojournal";
   argv[8] = (char *) "--noprealloc";
   argv[9] = (char *) "--smallfiles";
   argv[10] = (char *) "--nohttpinterface";
   argv[11] = (char *) "--bind_ip";
   argv[12] = (char *) "127.0.0.1";
   argv[13] = NULL;

   pid = fork();
   if (pid < 0) {
      perror("Failed to fork process");
      abort();
   }

   if (!pid) {
      int fd;

      if (0 != chdir(node->dbpath)) {
         perror("Failed to chdir");
         abort();
      }

      fd = open("/dev/null", O_RDWR);
      if (fd == -1) {
         perror("Failed to open /dev/null");
         abort();
      }

      dup2(fd, STDIN_FILENO);
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);

      close(fd);

      if (-1 == execvp(argv[0], argv)) {
         perror("Failed to spawn process");
         abort();
      }
   }

   fprintf(stderr, "[%d]: ", (int)pid);
   for (i = 0; argv[i]; i++)
      fprintf(stderr, "%s ", argv[i]);
   fprintf(stderr, "\n");

   node->pid = pid;
}


static void
ha_node_destroy (ha_node_t *node)
{
   ha_node_kill(node);
   bson_free(node->name);
   bson_free(node->repl_set);
   bson_free(node->dbpath);
   bson_free(node);
}


ha_replica_set_t *
ha_replica_set_new (const char *name)
{
   ha_replica_set_t *repl_set;

   srand(getpid());
   fprintf(stderr, "srand(%d)\n", (int)getpid());

   repl_set = bson_malloc0(sizeof *repl_set);
   repl_set->name = strdup(name);
   repl_set->next_port = 30000 + (rand() % 10000);

   return repl_set;
}


static ha_node_t *
ha_replica_set_add_node (ha_replica_set_t *replica_set,
                         const char       *name,
                         bson_bool_t       is_arbiter)
{
   ha_node_t *node;
   ha_node_t *iter;
   char dbpath[PATH_MAX];

   snprintf(dbpath, sizeof dbpath, "%s/%s", replica_set->name, name);
   dbpath[sizeof dbpath - 1] = '\0';

   node = ha_node_new(name,
                      replica_set->name,
                      dbpath,
                      is_arbiter,
                      replica_set->next_port++);

   if (!replica_set->nodes) {
      replica_set->nodes = node;
   } else {
      for (iter = replica_set->nodes; iter->next; iter = iter->next) { }
      iter->next = node;
   }

   return node;
}


ha_node_t *
ha_replica_set_add_arbiter (ha_replica_set_t *replica_set,
                            const char       *name)
{
   return ha_replica_set_add_node(replica_set, name, TRUE);
}


ha_node_t *
ha_replica_set_add_replica (ha_replica_set_t *replica_set,
                            const char       *name)
{
   return ha_replica_set_add_node(replica_set, name, FALSE);
}


static void
ha_replica_set_configure (ha_replica_set_t *replica_set,
                          ha_node_t        *primary)
{
   mongoc_database_t *database;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_error_t error;
   ha_node_t *node;
   bson_t ar;
   bson_t cmd;
   bson_t config;
   bson_t member;
   char *str;
   char *uristr;
   char hoststr[32];
   char key[8];
   int i = 0;

   uristr = bson_strdup_printf("mongodb://127.0.0.1:%hu/", primary->port);
   client = mongoc_client_new(uristr);
   bson_free(uristr);

   bson_init(&cmd);
   bson_append_document_begin(&cmd, "replSetInitiate", -1, &config);
   bson_append_utf8(&config, "_id", 3, replica_set->name, -1);
   bson_append_array_begin(&config, "members", -1, &ar);
   for (node = replica_set->nodes; node; node = node->next) {
      snprintf(key, sizeof key, "%u", i);
      key[sizeof key - 1] = '\0';

      snprintf(hoststr, sizeof hoststr, "127.0.0.1:%hu", node->port);
      hoststr[sizeof hoststr - 1] = '\0';

      bson_append_document_begin(&ar, key, -1, &member);
      bson_append_int32(&member, "_id", -1, i);
      bson_append_utf8(&member, "host", -1, hoststr, -1);
      bson_append_bool(&member, "arbiterOnly", -1, node->is_arbiter);
      bson_append_document_end(&ar, &member);

      i++;
   }
   bson_append_array_end(&config, &ar);
   bson_append_document_end(&cmd, &config);

   str = bson_as_json(&cmd, NULL);
   MONGOC_DEBUG("Config: %s", str);
   bson_free(str);

   database = mongoc_client_get_database(client, "admin");

again:
   cursor = mongoc_database_command(database,
                                    MONGOC_QUERY_NONE,
                                    0,
                                    1,
                                    &cmd,
                                    NULL,
                                    NULL);

   while (mongoc_cursor_next(cursor, &doc)) {
      str = bson_as_json(doc, NULL);
      MONGOC_DEBUG("Reply: %s", str);
      bson_free(str);
   }

   if (mongoc_cursor_error(cursor, &error)) {
      mongoc_cursor_destroy(cursor);
      MONGOC_WARNING("%s. Retrying in 1 second.", error.message);
      sleep(1);
      goto again;
   }

   mongoc_cursor_destroy(cursor);
   mongoc_database_destroy(database);
   mongoc_client_destroy(client);
   bson_destroy(&cmd);
}


void
ha_replica_set_start (ha_replica_set_t *replica_set)
{
   struct stat st;
   ha_node_t *primary = NULL;
   ha_node_t *node;
   char *cmd;

   if (!stat(replica_set->name, &st)) {
      if (S_ISDIR(st.st_mode)) {
         /* Ayyyeeeeeee */
         cmd = bson_strdup_printf("rm -rf \"%s\"", replica_set->name);
         fprintf(stderr, "%s\n", cmd);
         system(cmd);
         bson_free(cmd);
      }
   }

   if (!!mkdir(replica_set->name, 0750)) {
      fprintf(stderr, "Failed to create directory \"%s\"\n",
              replica_set->name);
      abort();
   }

   for (node = replica_set->nodes; node; node = node->next) {
      if (!primary && !node->is_arbiter) {
         primary = node;
      }
      ha_node_setup(node);
      ha_node_restart(node);
   }

   BSON_ASSERT(primary);

   sleep(2);

   ha_replica_set_configure(replica_set, primary);
}


void
ha_replica_set_shutdown (ha_replica_set_t *replica_set)
{
   ha_node_t *node;

   for (node = replica_set->nodes; node; node = node->next) {
      ha_node_kill(node);
   }
}


void
ha_replica_set_destroy (ha_replica_set_t *replica_set)
{
   ha_node_t *node;

   while ((node = replica_set->nodes)) {
      replica_set->nodes = node->next;
      ha_node_destroy(node);
   }

   bson_free(replica_set->name);
   bson_free(replica_set);
}


static bson_bool_t
ha_replica_set_get_status (ha_replica_set_t *replica_set,
                           bson_t           *status)
{
   mongoc_database_t *db;
   mongoc_client_t *client;
   mongoc_cursor_t *cursor;
   const bson_t *doc;
   bson_bool_t ret = FALSE;
   ha_node_t *node;
   bson_t cmd;
   char *uristr;

   bson_init(&cmd);
   bson_append_int32(&cmd, "replSetGetStatus", -1, 1);

   for (node = replica_set->nodes; !ret && node; node = node->next) {
      uristr = bson_strdup_printf("mongodb://127.0.0.1:%hu/?slaveOk=true",
                                  node->port);
      client = mongoc_client_new(uristr);
      bson_free(uristr);

      db = mongoc_client_get_database(client, "admin");

      if ((cursor = mongoc_database_command(db,
                                            MONGOC_QUERY_SLAVE_OK,
                                            0,
                                            1,
                                            &cmd,
                                            NULL,
                                            NULL))) {
         if (mongoc_cursor_next(cursor, &doc)) {
            bson_copy_to(doc, status);
            ret = TRUE;
         }
         mongoc_cursor_destroy(cursor);
      }

      mongoc_database_destroy(db);
      mongoc_client_destroy(client);
   }

   return ret;
}


void
ha_replica_set_wait_for_healthy (ha_replica_set_t *replica_set)
{
   bson_iter_t iter;
   bson_iter_t ar;
   bson_iter_t member;
   const char *stateStr;
   bson_t status;

again:
   sleep(1);

   if (!ha_replica_set_get_status(replica_set, &status)) {
      MONGOC_INFO("Failed to get replicaSet status. "
                  "Sleeping 1 second.");
      goto again;
   }

#if 0
   {
      char *str;

      str = bson_as_json(&status, NULL);
      printf("%s\n", str);
      bson_free(str);
   }
#endif

   if (!bson_iter_init_find(&iter, &status, "members") ||
       !BSON_ITER_HOLDS_ARRAY(&iter) ||
       !bson_iter_recurse(&iter, &ar)) {
      bson_destroy(&status);
      MONGOC_INFO("ReplicaSet has not yet come online. "
                  "Sleeping 1 second.");
      goto again;
   }

   while (bson_iter_next(&ar)) {
      if (BSON_ITER_HOLDS_DOCUMENT(&ar) &&
          bson_iter_recurse(&ar, &member) &&
          bson_iter_find(&member, "stateStr") &&
          (stateStr = bson_iter_utf8(&member, NULL))) {
         if (!!strcmp(stateStr, "PRIMARY") &&
             !!strcmp(stateStr, "SECONDARY") &&
             !!strcmp(stateStr, "ARBITER")) {
            bson_destroy(&status);
            MONGOC_INFO("Found unhealthy node. Sleeping 1 second.");
            goto again;
         }
      }
   }

   bson_destroy(&status);
}
