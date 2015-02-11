/*
 * Copyright 2013 MongoDB, Inc.
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
#include <sys/types.h>

#include "mongoc-host-list.h"
#include "mongoc-host-list-private.h"
#include "mongoc-log.h"
#include "mongoc-socket.h"
#include "mongoc-uri.h"


#if defined(_WIN32) && !defined(strcasecmp)
# define strcasecmp _stricmp
#endif


struct _mongoc_uri_t
{
   char                   *str;
   mongoc_host_list_t     *hosts;
   char                   *username;
   char                   *password;
   char                   *database;
   bson_t                  options;
   bson_t                  credentials;
   bson_t                  read_prefs;
   mongoc_write_concern_t *write_concern;
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
                        uint16_t       port)
{
   mongoc_host_list_t *iter;
   mongoc_host_list_t *link_;

   link_ = bson_malloc0(sizeof *link_);
   bson_strncpy (link_->host, host, sizeof link_->host);
   if (strchr (host, ':')) {
      bson_snprintf (link_->host_and_port, sizeof link_->host_and_port,
                     "[%s]:%hu", host, port);
      link_->family = AF_INET6;
   } else {
      bson_snprintf (link_->host_and_port, sizeof link_->host_and_port,
                     "%s:%hu", host, port);
      link_->family = strstr (host, ".sock") ? AF_UNIX : AF_INET;
   }
   link_->host_and_port[sizeof link_->host_and_port - 1] = '\0';
   link_->port = port;

   if ((iter = uri->hosts)) {
      for (; iter && iter->next; iter = iter->next) {}
      iter->next = link_;
   } else {
      uri->hosts = link_;
   }
}

/*
 *--------------------------------------------------------------------------
 *
 * scan_to_unichar --
 *
 *       Scans 'str' until either a character matching 'match' is found,
 *       until one of the characters in 'terminators' is encountered, or
 *       until we reach the end of 'str'.
 *
 *       NOTE: 'terminators' may not include multibyte UTF-8 characters.
 *
 * Returns:
 *       If 'match' is found, returns a copy of the section of 'str' before
 *       that character.  Otherwise, returns NULL.
 *
 * Side Effects:
 *       If 'match' is found, sets 'end' to begin at the matching character
 *       in 'str'.
 *
 *--------------------------------------------------------------------------
 */

static char *
scan_to_unichar (const char      *str,
                 bson_unichar_t   match,
                 const char      *terminators,
                 const char     **end)
{
   bson_unichar_t c;
   const char *iter;

   for (iter = str;
        iter && *iter && (c = bson_utf8_get_char(iter));
        iter = bson_utf8_next_char(iter)) {
      if (c == match) {
         *end = iter;
         return bson_strndup(str, iter - str);
      } else if (c == '\\') {
         iter = bson_utf8_next_char(iter);
         if (!bson_utf8_get_char(iter)) {
            break;
         }
      } else {
         const char *term_iter;
         for (term_iter = terminators; *term_iter; term_iter++) {
            if (c == *term_iter) {
               return NULL;
            }
         }
      }
   }

   return NULL;
}


static bool
mongoc_uri_parse_scheme (const char    *str,
                         const char   **end)
{
   if (!!strncmp(str, "mongodb://", 10)) {
      return false;
   }

   *end = str + 10;

   return true;
}


static bool
mongoc_uri_parse_userpass (mongoc_uri_t  *uri,
                           const char    *str,
                           const char   **end)
{
   bool ret = false;
   const char *end_userpass;
   const char *end_user;
   char *s;

   if ((s = scan_to_unichar(str, '@', "", &end_userpass))) {
      if ((uri->username = scan_to_unichar(s, ':', "", &end_user))) {
         uri->password = bson_strdup(end_user + 1);
      } else {
         uri->username = bson_strndup(str, end_userpass - str);
         uri->password = NULL;
      }
      mongoc_uri_do_unescape(&uri->username);
      mongoc_uri_do_unescape(&uri->password);
      *end = end_userpass + 1;
      bson_free(s);
      ret = true;
   } else {
      ret = true;
   }

   return ret;
}


static bool
mongoc_uri_parse_host6 (mongoc_uri_t  *uri,
                        const char    *str)
{
   uint16_t port = MONGOC_DEFAULT_PORT;
   const char *portstr;
   const char *end_host;
   char *hostname;

   if ((portstr = strrchr (str, ':')) && !strstr (portstr, "]")) {
#ifdef _MSC_VER
      sscanf_s (portstr, ":%hu", &port);
#else
      sscanf (portstr, ":%hu", &port);
#endif
   }

   hostname = scan_to_unichar (str + 1, ']', "", &end_host);

   mongoc_uri_do_unescape (&hostname);
   mongoc_uri_append_host (uri, hostname, port);
   bson_free (hostname);

   return true;
}


static bool
mongoc_uri_parse_host (mongoc_uri_t  *uri,
                       const char    *str)
{
   uint16_t port;
   const char *end_host;
   char *hostname;

   if (*str == '[' && strchr (str, ']')) {
      return mongoc_uri_parse_host6 (uri, str);
   }

   if ((hostname = scan_to_unichar(str, ':', "?/,", &end_host))) {
      end_host++;
      if (!isdigit(*end_host)) {
         bson_free(hostname);
         return false;
      }
#ifdef _MSC_VER
      sscanf_s (end_host, "%hu", &port);
#else
      sscanf (end_host, "%hu", &port);
#endif
   } else {
      hostname = bson_strdup(str);
      port = MONGOC_DEFAULT_PORT;
   }

   mongoc_uri_do_unescape(&hostname);
   mongoc_uri_append_host(uri, hostname, port);
   bson_free(hostname);

   return true;
}


bool
_mongoc_host_list_from_string (mongoc_host_list_t *host_list,
                               const char         *host_and_port)
{
   bool rval = false;
   char *uri_str = NULL;
   mongoc_uri_t *uri = NULL;
   const mongoc_host_list_t *uri_hl;

   bson_return_val_if_fail(host_list, false);
   bson_return_val_if_fail(host_and_port, false);

   uri_str = bson_strdup_printf("mongodb://%s/", host_and_port);
   if (! uri_str) goto CLEANUP;

   uri = mongoc_uri_new(uri_str);
   if (! uri) goto CLEANUP;

   uri_hl = mongoc_uri_get_hosts(uri);
   if (uri_hl->next) goto CLEANUP;

   memcpy(host_list, uri_hl, sizeof(*uri_hl));

   rval = true;

CLEANUP:

   bson_free(uri_str);
   if (uri) mongoc_uri_destroy(uri);

   return rval;
}


static bool
mongoc_uri_parse_hosts (mongoc_uri_t  *uri,
                        const char    *str,
                        const char   **end)
{
   bool ret = false;
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
         return false;
      }
      bson_free(s);
      str = sock + 5;
      ret = true;
      if (*str == ',') {
         str++;
         goto again;
      }
   } else if ((s = scan_to_unichar(str, ',', "/", &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return false;
      }
      bson_free(s);
      str = end_hostport + 1;
      ret = true;
      goto again;
   } else if ((s = scan_to_unichar(str, '/', "", &end_hostport)) ||
              (s = scan_to_unichar(str, '?', "", &end_hostport))) {
      if (!mongoc_uri_parse_host(uri, s)) {
         bson_free(s);
         return false;
      }
      bson_free(s);
      *end = end_hostport;
      return true;
   } else if (*str) {
      if (!mongoc_uri_parse_host(uri, str)) {
         return false;
      }
      *end = str + strlen(str);
      return true;
   }

   return ret;
}


static bool
mongoc_uri_parse_database (mongoc_uri_t  *uri,
                           const char    *str,
                           const char   **end)
{
   const char *end_database;

   if ((uri->database = scan_to_unichar(str, '?', "", &end_database))) {
      *end = end_database;
   } else if (*str) {
      uri->database = bson_strdup(str);
      *end = str + strlen(str);
   }

   mongoc_uri_do_unescape(&uri->database);

   return true;
}


static bool
mongoc_uri_parse_auth_mechanism_properties (mongoc_uri_t *uri,
                                            const char   *str)
{
   char *field;
   char *value;
   const char *end_scan;
   bson_t properties;

   bson_init(&properties);

   /* build up the properties document */
   while ((field = scan_to_unichar(str, ':', "&", &end_scan))) {
      str = end_scan + 1;
      if (!(value = scan_to_unichar(str, ',', ":&", &end_scan))) {
         value = bson_strdup(str);
         str = "";
      } else {
         str = end_scan + 1;
      }
      bson_append_utf8(&properties, field, -1, value, -1);
      bson_free(field);
      bson_free(value);
   }

   /* append our auth properties to our credentials */
   bson_append_document(&uri->credentials, "mechanismProperties",
                        -1, (const bson_t *)&properties);
   return true;
}

static void
mongoc_uri_parse_tags (mongoc_uri_t *uri, /* IN */
                       const char   *str, /* IN */
                       bson_t       *doc) /* IN */
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
   if ((keyval = scan_to_unichar(str, ',', "", &end_keyval))) {
      if ((key = scan_to_unichar(keyval, ':', "", &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
      bson_free(keyval);
      str = end_keyval + 1;
      goto again;
   } else {
       if ((key = scan_to_unichar(str, ':', "", &end_key))) {
         bson_append_utf8(&b, key, -1, end_key + 1, -1);
         bson_free(key);
      }
   }

   i = bson_count_keys(doc);
   bson_snprintf(keystr, sizeof keystr, "%u", i);
   bson_append_document(doc, keystr, -1, &b);
   bson_destroy(&b);
}


static bool
mongoc_uri_parse_option (mongoc_uri_t *uri,
                         const char   *str)
{
   int32_t v_int;
   const char *end_key;
   char *key;
   char *value;

   if (!(key = scan_to_unichar(str, '=', "", &end_key))) {
      return false;
   }

   value = bson_strdup(end_key + 1);
   mongoc_uri_do_unescape(&value);

   if (!strcasecmp(key, "connecttimeoutms") ||
       !strcasecmp(key, "sockettimeoutms") ||
       !strcasecmp(key, "maxpoolsize") ||
       !strcasecmp(key, "minpoolsize") ||
       !strcasecmp(key, "maxidletimems") ||
       !strcasecmp(key, "waitqueuemultiple") ||
       !strcasecmp(key, "waitqueuetimeoutms") ||
       !strcasecmp(key, "wtimeoutms")) {
      v_int = (int) strtol (value, NULL, 10);
      bson_append_int32(&uri->options, key, -1, v_int);
   } else if (!strcasecmp(key, "w")) {
      if (*value == '-' || isdigit(*value)) {
         v_int = (int) strtol (value, NULL, 10);
         BSON_APPEND_INT32 (&uri->options, "w", v_int);
      } else if (0 == strcasecmp (value, "majority")) {
         BSON_APPEND_UTF8 (&uri->options, "w", "majority");
      } else if (*value) {
         BSON_APPEND_UTF8 (&uri->options, "W", value);
      }
   } else if (!strcasecmp(key, "canonicalizeHostname") ||
              !strcasecmp(key, "journal") ||
              !strcasecmp(key, "safe") ||
              !strcasecmp(key, "slaveok") ||
              !strcasecmp(key, "ssl")) {
      bson_append_bool (&uri->options, key, -1,
                        (0 == strcasecmp (value, "true")) ||
                        (0 == strcasecmp (value, "t")) ||
                        (0 == strcmp (value, "1")));
   } else if (!strcasecmp(key, "readpreferencetags")) {
      mongoc_uri_parse_tags(uri, value, &uri->read_prefs);
   } else if (!strcasecmp(key, "authmechanism") ||
              !strcasecmp(key, "authsource")) {
      bson_append_utf8(&uri->credentials, key, -1, value, -1);
   } else if (!strcasecmp(key, "authmechanismproperties")) {
      if (!mongoc_uri_parse_auth_mechanism_properties(uri, value)) {
         bson_free(key);
         bson_free(value);
         return false;
      }
   } else {
      bson_append_utf8(&uri->options, key, -1, value, -1);
   }

   bson_free(key);
   bson_free(value);

   return true;
}


static bool
mongoc_uri_parse_options (mongoc_uri_t *uri,
                          const char   *str)
{
   const char *end_option;
   char *option;

again:
   if ((option = scan_to_unichar(str, '&', "", &end_option))) {
      if (!mongoc_uri_parse_option(uri, option)) {
         bson_free(option);
         return false;
      }
      bson_free(option);
      str = end_option + 1;
      goto again;
   } else if (*str) {
      if (!mongoc_uri_parse_option(uri, str)) {
         return false;
      }
   }

   return true;
}


static bool
mongoc_uri_finalize_auth (mongoc_uri_t *uri) {
   bson_iter_t iter;
   const char *source = NULL;
   const char *mechanism = mongoc_uri_get_auth_mechanism(uri);

   if (bson_iter_init_find_case(&iter, &uri->credentials, "authSource")) {
      source = bson_iter_utf8(&iter, NULL);
   }

   /* authSource with GSSAPI or X509 should always be external */
   if (mechanism) {
      if (!strcasecmp(mechanism, "GSSAPI") ||
          !strcasecmp(mechanism, "MONGODB-X509")) {
         if (source) {
            if (strcasecmp(source, "$external")) {
               return false;
            }
         } else {
            bson_append_utf8(&uri->credentials, "authsource", -1, "$external", -1);
         }
      }
   }
   return true;
}

static bool
mongoc_uri_parse (mongoc_uri_t *uri,
                  const char   *str)
{
   if (!mongoc_uri_parse_scheme(str, &str)) {
      return false;
   }

   if (!*str || !mongoc_uri_parse_userpass(uri, str, &str)) {
      return false;
   }

   if (!*str || !mongoc_uri_parse_hosts(uri, str, &str)) {
      return false;
   }

   switch (*str) {
   case '/':
      str++;
      if (*str && !mongoc_uri_parse_database(uri, str, &str)) {
         return false;
      }
      if (!*str) {
         break;
      }
      /* Fall through */
   case '?':
      str++;
      if (*str && !mongoc_uri_parse_options(uri, str)) {
         return false;
      }
      break;
   default:
      break;
   }

   return mongoc_uri_finalize_auth(uri);
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


const bson_t *
mongoc_uri_get_credentials (const mongoc_uri_t *uri)
{
   bson_return_val_if_fail(uri, NULL);
   return &uri->credentials;
}


const char *
mongoc_uri_get_auth_mechanism (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   bson_return_val_if_fail (uri, NULL);

   if (bson_iter_init_find_case (&iter, &uri->credentials, "authMechanism") &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return NULL;
}


bool
mongoc_uri_get_mechanism_properties (const mongoc_uri_t *uri, bson_t *properties)
{
   bson_iter_t iter;

   if (!uri) {
      return false;
   }

   if (bson_iter_init_find_case (&iter, &uri->credentials, "mechanismProperties") &&
      BSON_ITER_HOLDS_DOCUMENT (&iter)) {
      uint32_t len = 0;
      const uint8_t *data = NULL;

      bson_iter_document (&iter, &len, &data);
      bson_init_static (properties, data, len);

      return true;
   }

   return false;
}


static void
_mongoc_uri_build_write_concern (mongoc_uri_t *uri) /* IN */
{
   mongoc_write_concern_t *write_concern;
   const char *str;
   bson_iter_t iter;
   int32_t wtimeoutms = 0;
   int value;

   BSON_ASSERT (uri);

   write_concern = mongoc_write_concern_new ();

   if (bson_iter_init_find_case (&iter, &uri->options, "safe") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       !bson_iter_bool (&iter)) {
      mongoc_write_concern_set_w (write_concern,
                                  MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "wtimeoutms") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      wtimeoutms = bson_iter_int32 (&iter);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "journal") &&
       BSON_ITER_HOLDS_BOOL (&iter) &&
       bson_iter_bool (&iter)) {
      mongoc_write_concern_set_journal (write_concern, true);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, "w")) {
      if (BSON_ITER_HOLDS_INT32 (&iter)) {
         value = bson_iter_int32 (&iter);

         switch (value) {
         case -1:
            mongoc_write_concern_set_w (write_concern,
                                        MONGOC_WRITE_CONCERN_W_ERRORS_IGNORED);
            break;
         case 0:
            mongoc_write_concern_set_w (write_concern,
                                        MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED);
            break;
         case 1:
            mongoc_write_concern_set_w (write_concern,
                                        MONGOC_WRITE_CONCERN_W_DEFAULT);
            break;
         default:
            if (value > 1) {
               mongoc_write_concern_set_w (write_concern, value);
               break;
            }
            MONGOC_WARNING ("Unsupported w value [w=%d].", value);
            break;
         }
      } else if (BSON_ITER_HOLDS_UTF8 (&iter)) {
         str = bson_iter_utf8 (&iter, NULL);

         if (0 == strcasecmp ("majority", str)) {
            mongoc_write_concern_set_wmajority (write_concern, wtimeoutms);
         } else {
            mongoc_write_concern_set_wtag (write_concern, str);
         }
      } else {
         BSON_ASSERT (false);
      }
   }

   uri->write_concern = write_concern;
}


mongoc_uri_t *
mongoc_uri_new (const char *uri_string)
{
   mongoc_uri_t *uri;

   uri = bson_malloc0(sizeof *uri);
   bson_init(&uri->options);
   bson_init(&uri->credentials);
   bson_init(&uri->read_prefs);

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!mongoc_uri_parse(uri, uri_string)) {
      mongoc_uri_destroy(uri);
      return NULL;
   }

   uri->str = bson_strdup(uri_string);

   _mongoc_uri_build_write_concern (uri);

   return uri;
}


mongoc_uri_t *
mongoc_uri_new_for_host_port (const char *hostname,
                              uint16_t    port)
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

   if (bson_iter_init_find_case(&iter, &uri->credentials, "authSource")) {
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
      bson_destroy(&uri->credentials);
      bson_destroy(&uri->read_prefs);
      mongoc_write_concern_destroy(uri->write_concern);

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
   if (!bson_utf8_validate(escaped_string, len, false)) {
      MONGOC_WARNING("%s(): escaped_string contains invalid UTF-8",
                     __FUNCTION__);
      return NULL;
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
#ifdef _MSC_VER
             (1 != sscanf_s(&ptr[1], "%02x", &hex)) ||
#else
             (1 != sscanf(&ptr[1], "%02x", &hex)) ||
#endif
             !isprint(hex)) {
            bson_string_free(str, true);
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

   return bson_string_free(str, false);
}


const mongoc_write_concern_t *
mongoc_uri_get_write_concern (const mongoc_uri_t *uri) /* IN */
{
   bson_return_val_if_fail (uri, NULL);

   return uri->write_concern;
}


bool
mongoc_uri_get_ssl (const mongoc_uri_t *uri) /* IN */
{
   bson_iter_t iter;

   bson_return_val_if_fail (uri, false);

   return (bson_iter_init_find_case (&iter, &uri->options, "ssl") &&
           BSON_ITER_HOLDS_BOOL (&iter) &&
           bson_iter_bool (&iter));
}
