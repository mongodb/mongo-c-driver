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


#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "mongoc-host-list.h"
#include "mongoc-log.h"
#include "mongoc-uri.h"


#ifndef MONGOC_DEFAULT_PORT
#define MONGOC_DEFAULT_PORT 27017
#endif


struct _mongoc_uri_t
{
   char               *str;
   mongoc_host_list_t *hosts;
   char               *username;
   char               *password;
   char               *database;
   bson_t              options;
   bson_t              read_prefs;
   bson_t              write_concern;
};


static void
mongoc_uri_do_unescape (char **str)
{
   char *tmp;

   if ((tmp = *str)) {
      *str = mongoc_uri_unescape(tmp);
      bson_free(tmp);
   }
}


static void
mongoc_uri_append_host (mongoc_uri_t  *uri,
                        const char    *host,
                        bson_uint16_t  port)
{
   mongoc_host_list_t *iter;
   mongoc_host_list_t *link_;

   link_ = bson_malloc0(sizeof *link_);
   strncpy(link_->host, host, sizeof link_->host);
   link_->host[sizeof link_->host - 1] = '\0';
   if (strchr (host, ':')) {
      snprintf(link_->host_and_port, sizeof link_->host_and_port,
               "[%s]:%hu", host, port);
   } else {
      snprintf(link_->host_and_port, sizeof link_->host_and_port,
               "%s:%hu", host, port);
   }
   link_->host_and_port[sizeof link_->host_and_port - 1] = '\0';
   link_->port = port;
   link_->family = strstr(host, ".sock") ? AF_UNIX : AF_INET;

   if ((iter = uri->hosts)) {
      for (; iter && iter->next; iter = iter->next) {}
      iter->next = link_;
   } else {
      uri->hosts = link_;
   }
}


static char *
scan_to_unichar (const char      *str,
                 bson_unichar_t   stop,
                 const char     **end)
{
   bson_unichar_t c;
   const char *iter;

   for (iter = str;
        iter && *iter && (c = bson_utf8_get_char(iter));
        iter = bson_utf8_next_char(iter))
   {
      if (c == stop) {
         *end = iter;
         return bson_strndup(str, iter - str);
      } else if (c == '\\') {
         iter = bson_utf8_next_char(iter);
         if (!bson_utf8_get_char(iter)) {
            break;
         }
      }
   }

   return NULL;
}


static bson_bool_t
mongoc_uri_parse_scheme (const char    *str,
                         const char   **end)
{
   if (!!strncmp(str, "mongodb://", 10)) {
      return FALSE;
   }

   *end = str + 10;

   return TRUE;
}


static bson_bool_t
mongoc_uri_parse_userpass (mongoc_uri_t  *uri,
                           const char    *str,
                           const char   **end)
{
   bson_bool_t ret = FALSE;
   const char *end_userpass;
   const char *end_user;
   char *s;

   if ((s = scan_to_unichar(str, '@', &end_userpass))) {
      if ((uri->username = scan_to_unichar(s, ':', &end_user))) {
         uri->password = strdup(end_user + 1);
      } else {
         uri->username = strndup(str, end_userpass - str);
         uri->password = NULL;
      }
      mongoc_uri_do_unescape(&uri->username);
      mongoc_uri_do_unescape(&uri->password);
      *end = end_userpass + 1;
      bson_free(s);
      ret = TRUE;
   } else {
      ret = TRUE;
   }

   return ret;
}


static bson_bool_t
mongoc_uri_parse_host6 (mongoc_uri_t  *uri,
                        const char    *str)
{
   bson_uint16_t port = 27017;
   const char *portstr;
   const char *end_host;
   char *hostname;

   if ((portstr = strrchr (str, ':')) && !strstr (portstr, "]")) {
      sscanf (portstr, ":%hu", &port);
   }

   hostname = scan_to_unichar (str + 1, ']', &end_host);

   mongoc_uri_do_unescape (&hostname);
   mongoc_uri_append_host (uri, hostname, port);
   bson_free (hostname);

   return TRUE;
}


static bson_bool_t
mongoc_uri_parse_host (mongoc_uri_t  *uri,
                       const char    *str)
{
   bson_uint16_t port;
   const char *end_host;
   char *hostname;

   if (*str == '[' && strchr (str, ']')) {
      return mongoc_uri_parse_host6 (uri, str);
   }

   if ((hostname = scan_to_unichar(str, ':', &end_host))) {
      end_host++;
      if (!isdigit(*end_host)) {
         bson_free(hostname);
         return FALSE;
      }
      sscanf(end_host, "%hu", &port);
   } else {
      hostname = bson_strdup(str);
      port = MONGOC_DEFAULT_PORT;
   }

   mongoc_uri_do_unescape(&hostname);
   mongoc_uri_append_host(uri, hostname, port);
   bson_free(hostname);

   return TRUE;
}


bson_bool_t
_mongoc_host_list_from_string (mongoc_host_list_t *host_list,
                               const char         *host_and_port)
{
   bson_uint16_t port;
   const char *end_host;
   char *hostname = NULL;

   bson_return_val_if_fail(host_list, FALSE);
   bson_return_val_if_fail(host_and_port, FALSE);

   memset(host_list, 0, sizeof *host_list);

   if ((hostname = scan_to_unichar(host_and_port, ':', &end_host))) {
      end_host++;
      if (!isdigit(*end_host)) {
         bson_free(hostname);
         return FALSE;
      }
      sscanf(end_host, "%hu", &port);
   } else {
      hostname = bson_strdup(host_and_port);
      port = MONGOC_DEFAULT_PORT;
   }

   strncpy(host_list->host_and_port, host_and_port,
           sizeof host_list->host_and_port - 1);
   host_list->host_and_port[sizeof host_list->host_and_port-1] = '\0';

   strncpy(host_list->host, hostname, sizeof host_list->host - 1);
   host_list->host[sizeof host_list->host-1] = '\0';

   host_list->port = port;
   host_list->family = AF_INET;

   bson_free(hostname);

   return TRUE;
}


static bson_bool_t
mongoc_uri_parse_hosts (mongoc_uri_t  *uri,
                        const char    *str,
                        const char   **end)
{
   bson_bool_t ret = FALSE;
   const char *end_hostport;
   const char *sock;
   const char *tmp;
   char *s;

   /*
    * Parsing the series of hosts is a lot more complicated than you might
    * imagine. This is due to some characters being both separators as well as
    * valid characters within the "hostname". In particularly, we can have file
    * paths to specify paths to UNIX domain sockets. We impose the restriction
    * that they must be suffixed with ".sock" to simplify the parsing.
    *
    * You can separate hosts and file system paths to UNIX domain sockets with
    * ",".
    *
    * When you reach a "/" or "?" that is not part of a file-system path, we
    * have completed our parsing of hosts.
    */

again:
   if (((*str == '/') && (sock = strstr(str, ".sock"))) &&
       (!(tmp = strstr(str, ",")) || (tmp > sock)) &&
       (!(tmp = strstr(str, "?")) || (tmp > sock))) {
      s = bson_strndup(str, sock + 5 - str);
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return FALSE;
      }
      bson_free(s);
      str = sock + 5;
      ret = TRUE;
      if (*str == ',') {
         str++;
         goto again;
      }
   } else if ((s = scan_to_unichar(str, ',', &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return FALSE;
      }
      bson_free(s);
      str = end_hostport + 1;
      ret = TRUE;
      goto again;
   } else if ((s = scan_to_unichar(str, '/', &end_hostport)) ||
              (s = scan_to_unichar(str, '?', &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return FALSE;
      }
      bson_free(s);
      *end = end_hostport;
      return TRUE;
   } else if (*str) {
      if (!mongoc_uri_parse_host(uri, str)) {
         return FALSE;
      }
      *end = str + strlen(str);
      return TRUE;
   }

   return ret;
}


static bson_bool_t
mongoc_uri_parse_database (mongoc_uri_t  *uri,
                           const char    *str,
                           const char   **end)
{
   const char *end_database;

   if ((uri->database = scan_to_unichar(str, '?', &end_database))) {
      *end = end_database;
   } else if (*str) {
      uri->database = strdup(str);
      *end = str + strlen(str);
   }

   mongoc_uri_do_unescape(&uri->database);

   return TRUE;
}


static void
mongoc_uri_parse_read_prefs (mongoc_uri_t *uri,
                             const char   *str)
{
   const char *end_keyval;
   const char *end_key;
   bson_t b;
   char *keyval;
   char *key;
   char keystr[32];
   int i;

   bson_init(&b);

again:
   if ((keyval = scan_to_unichar(str, ',', &end_keyval))) {
      if ((key = scan_to_unichar(keyval, ':', &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
      bson_free(keyval);
      str = end_keyval + 1;
      goto again;
   } else {
      if ((key = scan_to_unichar(str, ':', &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
   }

   i = bson_count_keys(&uri->read_prefs);
   snprintf(keystr, sizeof keystr, "%u", i);
   bson_append_document(&uri->read_prefs, keystr, -1, &b);
   bson_destroy(&b);
}


static bson_bool_t
mongoc_uri_parse_option (mongoc_uri_t *uri,
                         const char   *str)
{
   bson_int32_t v_int;
   const char *end_key;
   char *key;
   char *value;

   if (!(key = scan_to_unichar(str, '=', &end_key))) {
      return FALSE;
   }

   value = strdup(end_key + 1);
   mongoc_uri_do_unescape(&value);

   if (!strcasecmp(key, "connecttimeoutms") ||
       !strcasecmp(key, "sockettimeoutms") ||
       !strcasecmp(key, "maxpoolsize") ||
       !strcasecmp(key, "minpoolsize") ||
       !strcasecmp(key, "maxidletimems") ||
       !strcasecmp(key, "waitqueuemultiple") ||
       !strcasecmp(key, "waitqueuetimeoutms") ||
       !strcasecmp(key, "wtimeoutms")) {
      v_int = strtol(value, NULL, 10);
      bson_append_int32(&uri->options, key, -1, v_int);
   } else if (!strcasecmp(key, "w")) {
      if (*value == '-' || isdigit(*value)) {
         v_int = strtol(value, NULL, 10);
         bson_append_int32(&uri->options, key, -1, v_int);
      } else {
         bson_append_utf8(&uri->options, key, -1, value, -1);
      }
   } else if (!strcasecmp(key, "journal") ||
              !strcasecmp(key, "slaveok") ||
              !strcasecmp(key, "ssl")) {
      bson_append_bool(&uri->options, key, -1, !strcmp(value, "true"));
   } else if (!strcasecmp(key, "readpreferencetags")) {
      mongoc_uri_parse_read_prefs(uri, value);
   } else {
      bson_append_utf8(&uri->options, key, -1, value, -1);
   }

   bson_free(key);
   bson_free(value);

   return TRUE;
}


static bson_bool_t
mongoc_uri_parse_options (mongoc_uri_t *uri,
                          const char   *str)
{
   const char *end_option;
   char *option;

again:
   if ((option = scan_to_unichar(str, '&', &end_option))) {
      if (!mongoc_uri_parse_option(uri, option)) {
         bson_free(option);
         return FALSE;
      }
      bson_free(option);
      str = end_option + 1;
      goto again;
   } else if (*str) {
      if (!mongoc_uri_parse_option(uri, str)) {
         return FALSE;
      }
   }

   return TRUE;
}


static bson_bool_t
mongoc_uri_parse (mongoc_uri_t *uri,
                  const char   *str)
{
   if (!mongoc_uri_parse_scheme(str, &str)) {
      return FALSE;
   }

   if (!*str || !mongoc_uri_parse_userpass(uri, str, &str)) {
      return FALSE;
   }

   if (!*str || !mongoc_uri_parse_hosts(uri, str, &str)) {
      return FALSE;
   }

   switch (*str) {
   case '/':
      str++;
      if (*str && !mongoc_uri_parse_database(uri, str, &str)) {
         return FALSE;
      }
      if (!*str) {
         break;
      }
      /* Fall through */
   case '?':
      str++;
      if (*str && !mongoc_uri_parse_options(uri, str)) {
         return FALSE;
      }
      break;
   default:
      break;
   }

   return TRUE;
}


const mongoc_host_list_t *
mongoc_uri_get_hosts (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->hosts;
}


const char *
mongoc_uri_get_replica_set (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail(uri, NULL);

   if (bson_iter_init_find_case(&iter, &uri->options, "replicaSet") &&
       BSON_ITER_HOLDS_UTF8(&iter)) {
      return bson_iter_utf8(&iter, NULL);
   }

   return NULL;
}


const char *
mongoc_uri_get_auth_mechanism (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail (uri, NULL);

   if (bson_iter_init_find_case (&iter, &uri->options, "authMechanism") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return NULL;
}


mongoc_uri_t *
mongoc_uri_new (const char *uri_string)
{
   mongoc_uri_t *uri;

   uri = bson_malloc0(sizeof *uri);
   bson_init(&uri->options);
   bson_init(&uri->read_prefs);
   bson_init(&uri->write_concern);

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!mongoc_uri_parse(uri, uri_string)) {
      mongoc_uri_destroy(uri);
      return NULL;
   }

   uri->str = strdup(uri_string);

   return uri;
}


mongoc_uri_t *
mongoc_uri_new_for_host_port (const char    *hostname,
                              bson_uint16_t  port)
{
   mongoc_uri_t *uri;
   char *str;

   bson_return_val_if_fail(hostname, NULL);
   bson_return_val_if_fail(port, NULL);

   str = bson_strdup_printf("mongodb://%s:%hu/", hostname, port);
   uri = mongoc_uri_new(str);
   bson_free(str);

   return uri;
}


const char *
mongoc_uri_get_username (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->username;
}


const char *
mongoc_uri_get_password (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->password;
}


const char *
mongoc_uri_get_database (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->database;
}


const char *
mongoc_uri_get_auth_source (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail(uri, NULL);

   if (bson_iter_init_find_case(&iter, &uri->options, "authSource")) {
      return bson_iter_utf8(&iter, NULL);
   }

   return uri->database ? uri->database : "admin";
}


const bson_t *
mongoc_uri_get_options (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return &uri->options;
}


void
mongoc_uri_destroy (mongoc_uri_t *uri)
{
   mongoc_host_list_t *tmp;

   if (uri) {
      while (uri->hosts) {
         tmp = uri->hosts;
         uri->hosts = tmp->next;
         bson_free(tmp);
      }

      bson_free(uri->str);
      bson_free(uri->database);
      bson_free(uri->username);
      bson_destroy(&uri->options);
      bson_destroy(&uri->read_prefs);
      bson_destroy(&uri->write_concern);

      if (uri->password) {
         bson_zero_free(uri->password, strlen(uri->password));
      }

      bson_free(uri);
   }
}


mongoc_uri_t *
mongoc_uri_copy (const mongoc_uri_t *uri)
{
   return mongoc_uri_new(uri->str);
}


const char *
mongoc_uri_get_string (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return uri->str;
}


const bson_t *
mongoc_uri_get_read_prefs (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return &uri->read_prefs;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_unescape --
 *
 *       Escapes an UTF-8 encoded string containing URI escaped segments
 *       such as %20.
 *
 *       It is a programming error to call this function with a string
 *       that is not UTF-8 encoded!
 *
 * Returns:
 *       A newly allocated string that should be freed with bson_free().
 *
 * Side effects:
 *       None.
 *
 *--------------------------------------------------------------------------
 */

char *
mongoc_uri_unescape (const char *escaped_string)
{
   bson_unichar_t c;
   bson_string_t *str;
   unsigned int hex = 0;
   const char *ptr;
   const char *end;
   size_t len;

   bson_return_val_if_fail(escaped_string, NULL);

   len = strlen(escaped_string);

   /*
    * Double check that this is a UTF-8 valid string. Bail out if necessary.
    */
   if (!bson_utf8_validate(escaped_string, len, FALSE)) {
      MONGOC_WARNING("%s(): escaped_string contains invalid UTF-8",
                     __FUNCTION__);
      return FALSE;
   }

   ptr = escaped_string;
   end = ptr + len;
   str = bson_string_new(NULL);

   for (; *ptr; ptr = bson_utf8_next_char(ptr)) {
      c = bson_utf8_get_char(ptr);
      switch (c) {
      case '%':
         if (((end - ptr) < 2) ||
             !isxdigit(ptr[1]) ||
             !isxdigit(ptr[2]) ||
             (1 != sscanf(&ptr[1], "%02x", &hex)) ||
             !isprint(hex)) {
            bson_string_free(str, TRUE);
            return NULL;
         }
         bson_string_append_c(str, hex);
         ptr += 2;
         break;
      default:
         bson_string_append_unichar(str, c);
         break;
      }
   }

   return bson_string_free(str, FALSE);
}
