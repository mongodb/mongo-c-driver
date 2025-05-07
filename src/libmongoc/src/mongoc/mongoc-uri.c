/*
 * Copyright 2009-present MongoDB, Inc.
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <math.h>

/* strcasecmp on windows */
#include <mongoc/mongoc-util-private.h>

#include <mongoc/mongoc-config.h>
#include <mongoc/mongoc-error-private.h>
#include <mongoc/mongoc-host-list.h>
#include <mongoc/mongoc-host-list-private.h>
#include <mongoc/mongoc-log.h>
#include <mongoc/mongoc-handshake-private.h>
#include <mongoc/mongoc-socket.h>
#include <mongoc/mongoc-topology-private.h>
#include <mongoc/mongoc-uri-private.h>
#include <mongoc/mongoc-read-concern-private.h>
#include <mongoc/mongoc-write-concern-private.h>
#include <mongoc/mongoc-compression-private.h>
#include <mongoc/utlist.h>
#include <mongoc/mongoc-trace-private.h>
#include <mongoc/mongoc-oidc-env-private.h>

#include <common-bson-dsl-private.h>
#include <common-string-private.h>

struct _mongoc_uri_t {
   char *str;
   bool is_srv;
   char srv[BSON_HOST_NAME_MAX + 1];
   mongoc_host_list_t *hosts;
   char *username; // MongoCredential.username
   char *password; // MongoCredential.password
   char *database;
   bson_t raw;         // Unparsed options, see mongoc_uri_parse_options
   bson_t options;     // Type-coerced and canonicalized options
   bson_t credentials; // MongoCredential.source, MongoCredential.mechanism, and MongoCredential.mechanism_properties.
   bson_t compressors;
   mongoc_read_prefs_t *read_prefs;
   mongoc_read_concern_t *read_concern;
   mongoc_write_concern_t *write_concern;
};

#define MONGOC_URI_ERROR(error, format, ...) \
   _mongoc_set_error (error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, format, __VA_ARGS__)


static const char *escape_instructions = "Percent-encode username and password"
                                         " according to RFC 3986";

static bool
_mongoc_uri_set_option_as_int32 (mongoc_uri_t *uri, const char *option, int32_t value);

static bool
_mongoc_uri_set_option_as_int32_with_error (mongoc_uri_t *uri, const char *option, int32_t value, bson_error_t *error);

static bool
_mongoc_uri_set_option_as_int64_with_error (mongoc_uri_t *uri, const char *option, int64_t value, bson_error_t *error);

static void
mongoc_uri_do_unescape (char **str)
{
   char *tmp;

   if ((tmp = *str)) {
      *str = mongoc_uri_unescape (tmp);
      bson_free (tmp);
   }
}


#define VALIDATE_SRV_ERR()                                                   \
   do {                                                                      \
      _mongoc_set_error (error,                                              \
                         MONGOC_ERROR_STREAM,                                \
                         MONGOC_ERROR_STREAM_NAME_RESOLUTION,                \
                         "Invalid host \"%s\" returned for service \"%s\": " \
                         "host must be subdomain of service name",           \
                         host,                                               \
                         srv_hostname);                                      \
      return false;                                                          \
   } while (0)


static int
count_dots (const char *s)
{
   int n = 0;
   const char *dot = s;

   while ((dot = strchr (dot + 1, '.'))) {
      n++;
   }

   return n;
}

static char *
lowercase_str_new (const char *key)
{
   char *ret = bson_strdup (key);
   mongoc_lowercase (key, ret);
   return ret;
}

/* at least one character, and does not start with dot */
static bool
valid_hostname (const char *s)
{
   size_t len = strlen (s);

   return len > 1 && s[0] != '.';
}


bool
mongoc_uri_validate_srv_result (const mongoc_uri_t *uri, const char *host, bson_error_t *error)
{
   const char *srv_hostname;
   const char *srv_host;

   srv_hostname = mongoc_uri_get_srv_hostname (uri);
   BSON_ASSERT (srv_hostname);

   if (!valid_hostname (host)) {
      VALIDATE_SRV_ERR ();
   }

   srv_host = strchr (srv_hostname, '.');
   BSON_ASSERT (srv_host);

   /* host must be descendent of service root: if service is
    * "a.foo.co" host can be like "a.foo.co", "b.foo.co", "a.b.foo.co", etc.
    */
   if (strlen (host) < strlen (srv_host)) {
      VALIDATE_SRV_ERR ();
   }

   if (!mongoc_ends_with (host, srv_host)) {
      VALIDATE_SRV_ERR ();
   }

   return true;
}

/* copy and upsert @host into @uri's host list. */
static bool
_upsert_into_host_list (mongoc_uri_t *uri, mongoc_host_list_t *host, bson_error_t *error)
{
   if (uri->is_srv && !mongoc_uri_validate_srv_result (uri, host->host, error)) {
      return false;
   }

   _mongoc_host_list_upsert (&uri->hosts, host);

   return true;
}

bool
mongoc_uri_upsert_host_and_port (mongoc_uri_t *uri, const char *host_and_port, bson_error_t *error)
{
   mongoc_host_list_t temp;

   memset (&temp, 0, sizeof (mongoc_host_list_t));
   if (!_mongoc_host_list_from_string_with_err (&temp, host_and_port, error)) {
      return false;
   }

   return _upsert_into_host_list (uri, &temp, error);
}

bool
mongoc_uri_upsert_host (mongoc_uri_t *uri, const char *host, uint16_t port, bson_error_t *error)
{
   mongoc_host_list_t temp;

   memset (&temp, 0, sizeof (mongoc_host_list_t));
   if (!_mongoc_host_list_from_hostport_with_err (&temp, host, port, error)) {
      return false;
   }

   return _upsert_into_host_list (uri, &temp, error);
}

void
mongoc_uri_remove_host (mongoc_uri_t *uri, const char *host, uint16_t port)
{
   _mongoc_host_list_remove_host (&(uri->hosts), host, port);
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
scan_to_unichar (const char *str, bson_unichar_t match, const char *terminators, const char **end)
{
   bson_unichar_t c;
   const char *iter;

   for (iter = str; iter && *iter && (c = bson_utf8_get_char (iter)); iter = bson_utf8_next_char (iter)) {
      if (c == match) {
         *end = iter;
         return bson_strndup (str, iter - str);
      } else if (c == '\\') {
         iter = bson_utf8_next_char (iter);
         if (!bson_utf8_get_char (iter)) {
            break;
         }
      } else {
         const char *term_iter;
         for (term_iter = terminators; *term_iter; term_iter++) {
            if (mlib_cmp (c, ==, *term_iter)) {
               return NULL;
            }
         }
      }
   }

   return NULL;
}


static bool
mongoc_uri_parse_scheme (mongoc_uri_t *uri, const char *str, const char **end)
{
   if (!strncmp (str, "mongodb+srv://", 14)) {
      uri->is_srv = true;
      *end = str + 14;
      return true;
   }

   if (!strncmp (str, "mongodb://", 10)) {
      uri->is_srv = false;
      *end = str + 10;
      return true;
   }

   return false;
}


static bool
mongoc_uri_has_unescaped_chars (const char *str, const char *chars)
{
   const char *c;
   const char *tmp;
   char *s;

   for (c = chars; *c; c++) {
      s = scan_to_unichar (str, (bson_unichar_t) *c, "", &tmp);
      if (s) {
         bson_free (s);
         return true;
      }
   }

   return false;
}


/* "str" is non-NULL, the part of URI between "mongodb://" and first "@" */
static bool
mongoc_uri_parse_userpass (mongoc_uri_t *uri, const char *str, bson_error_t *error)
{
   const char *prohibited = "@:/";
   const char *end_user;

   BSON_ASSERT (str);
   BSON_ASSERT (uri);

   if ((uri->username = scan_to_unichar (str, ':', "", &end_user))) {
      uri->password = bson_strdup (end_user + 1);
   } else {
      uri->username = bson_strdup (str);
      uri->password = NULL;
   }

   if (mongoc_uri_has_unescaped_chars (uri->username, prohibited)) {
      MONGOC_URI_ERROR (error, "Username \"%s\" must not have unescaped chars. %s", uri->username, escape_instructions);
      return false;
   }

   mongoc_uri_do_unescape (&uri->username);
   if (!uri->username) {
      MONGOC_URI_ERROR (error, "Incorrect URI escapes in username. %s", escape_instructions);
      return false;
   }

   /* Providing password at all is optional */
   if (uri->password) {
      if (mongoc_uri_has_unescaped_chars (uri->password, prohibited)) {
         MONGOC_URI_ERROR (
            error, "Password \"%s\" must not have unescaped chars. %s", uri->password, escape_instructions);
         return false;
      }

      mongoc_uri_do_unescape (&uri->password);
      if (!uri->password) {
         MONGOC_URI_ERROR (error, "%s", "Incorrect URI escapes in password");
         return false;
      }
   }

   return true;
}

bool
mongoc_uri_parse_host (mongoc_uri_t *uri, const char *host_and_port_in)
{
   char *host_and_port = bson_strdup (host_and_port_in);
   bson_error_t err = {0};
   bool r;

   /* unescape host. It doesn't hurt including port. */
   if (mongoc_uri_has_unescaped_chars (host_and_port, "/")) {
      MONGOC_WARNING ("Unix Domain Sockets must be escaped (e.g. / = %%2F)");
      bson_free (host_and_port);
      return false;
   }

   mongoc_uri_do_unescape (&host_and_port);
   if (!host_and_port) {
      /* invalid */
      bson_free (host_and_port);
      return false;
   }

   r = mongoc_uri_upsert_host_and_port (uri, host_and_port, &err);

   if (!r) {
      MONGOC_ERROR ("%s", err.message);
      bson_free (host_and_port);
      return false;
   }

   bson_free (host_and_port);
   return true;
}


static bool
mongoc_uri_parse_srv (mongoc_uri_t *uri, const char *str, bson_error_t *error)
{
   if (*str == '\0') {
      MONGOC_URI_ERROR (error, "%s", "Missing service name in SRV URI");
      return false;
   }

   {
      char *service = bson_strdup (str);

      mongoc_uri_do_unescape (&service);

      if (!service || !valid_hostname (service) || count_dots (service) < 2) {
         MONGOC_URI_ERROR (error, "%s", "Invalid service name in URI");
         bson_free (service);
         return false;
      }

      bson_strncpy (uri->srv, service, sizeof uri->srv);

      bson_free (service);
   }

   if (strchr (uri->srv, ',')) {
      MONGOC_URI_ERROR (error, "%s", "Multiple service names are prohibited in an SRV URI");
      return false;
   }

   if (strchr (uri->srv, ':')) {
      MONGOC_URI_ERROR (error, "%s", "Port numbers are prohibited in an SRV URI");
      return false;
   }

   return true;
}


/* "hosts" is non-NULL, the part between "mongodb://" or "@" and last "/" */
static bool
mongoc_uri_parse_hosts (mongoc_uri_t *uri, const char *hosts)
{
   const char *next;
   const char *end_hostport;
   char *s;
   BSON_ASSERT (hosts);
   /*
    * Parsing the series of hosts is a lot more complicated than you might
    * imagine. This is due to some characters being both separators as well as
    * valid characters within the "hostname". In particularly, we can have file
    * paths to specify paths to UNIX domain sockets. We impose the restriction
    * that they must be suffixed with ".sock" to simplify the parsing.
    *
    * You can separate hosts and file system paths to UNIX domain sockets with
    * ",".
    */
   s = scan_to_unichar (hosts, '?', "", &end_hostport);
   if (s) {
      MONGOC_WARNING ("%s", "A '/' is required between the host list and any options.");
      goto error;
   }
   next = hosts;
   do {
      /* makes a copy of the section of the string */
      s = scan_to_unichar (next, ',', "", &end_hostport);
      if (s) {
         next = (char *) end_hostport + 1;
      } else {
         s = bson_strdup (next);
         next = NULL;
      }
      if (!mongoc_uri_parse_host (uri, s)) {
         goto error;
      }
      bson_free (s);
   } while (next);
   return true;
error:
   bson_free (s);
   return false;
}

/* -----------------------------------------------------------------------------
 *
 * mongoc_uri_parse_database --
 *
 *        Parse the database after @str. @str is expected to point after the
 *        host list to the character immediately after the / in the uri string.
 *        If no database is specified in the uri, e.g. the uri has a form like:
 *        mongodb://localhost/?option=X then uri->database remains NULL after
 *        parsing.
 *
 * Return:
 *        True if the parsed database is valid. An empty database is considered
 *        valid.
 * -----------------------------------------------------------------------------
 */
static bool
mongoc_uri_parse_database (mongoc_uri_t *uri, const char *str, const char **end)
{
   const char *end_database;
   const char *c;
   char *invalid_c;
   const char *tmp;

   if ((uri->database = scan_to_unichar (str, '?', "", &end_database))) {
      if (strcmp (uri->database, "") == 0) {
         /* no database is found, don't store the empty string. */
         bson_free (uri->database);
         uri->database = NULL;
         /* but it is valid to have an empty database. */
         return true;
      }
      *end = end_database;
   } else if (*str) {
      uri->database = bson_strdup (str);
      *end = str + strlen (str);
   }

   mongoc_uri_do_unescape (&uri->database);
   if (!uri->database) {
      /* invalid */
      return false;
   }

   /* invalid characters in database name */
   for (c = "/\\. \"$"; *c; c++) {
      invalid_c = scan_to_unichar (uri->database, (bson_unichar_t) *c, "", &tmp);
      if (invalid_c) {
         bson_free (invalid_c);
         return false;
      }
   }

   return true;
}


static bool
mongoc_uri_parse_auth_mechanism_properties (mongoc_uri_t *uri, const char *str)
{
   const char *end_scan;

   bson_t properties = BSON_INITIALIZER;

   // Key-value pairs are delimited by ','.
   for (char *kvp; (kvp = scan_to_unichar (str, ',', "", &end_scan)); bson_free (kvp)) {
      str = end_scan + 1;

      char *const key = scan_to_unichar (kvp, ':', "", &end_scan);

      // Found delimiter: split into key and value.
      if (key) {
         char *const value = bson_strdup (end_scan + 1);
         BSON_APPEND_UTF8 (&properties, key, value);
         bson_free (key);
         bson_free (value);
      }

      // No delimiter: entire string is the key. Use empty string as value.
      else {
         BSON_APPEND_UTF8 (&properties, kvp, "");
      }
   }

   // Last (or only) pair.
   if (*str != '\0') {
      char *const key = scan_to_unichar (str, ':', "", &end_scan);

      // Found delimiter: split into key and value.
      if (key) {
         char *const value = bson_strdup (end_scan + 1);
         BSON_APPEND_UTF8 (&properties, key, value);
         bson_free (key);
         bson_free (value);
      }

      // No delimiter: entire string is the key. Use empty string as value.
      else {
         BSON_APPEND_UTF8 (&properties, str, "");
      }
   }

   /* append our auth properties to our credentials */
   if (!mongoc_uri_set_mechanism_properties (uri, &properties)) {
      bson_destroy (&properties);
      return false;
   }
   bson_destroy (&properties);
   return true;
}


static bool
mongoc_uri_check_srv_service_name (mongoc_uri_t *uri, const char *str)
{
   /* 63 character DNS query limit, excluding prepended underscore. */
   const size_t mongoc_srv_service_name_max = 62u;

   size_t length = 0u;
   size_t num_alpha = 0u;
   size_t i = 0u;
   char prev = '\0';

   BSON_ASSERT_PARAM (uri);
   BSON_ASSERT_PARAM (str);

   length = strlen (str);

   /* Initial DNS Seedlist Discovery Spec: This option specifies a valid SRV
    * service name according to RFC 6335, with the exception that it may exceed
    * 15 characters as long as the 63rd (62nd with prepended underscore)
    * character DNS query limit is not surpassed. */
   if (length > mongoc_srv_service_name_max) {
      return false;
   }

   /* RFC 6335: MUST be at least 1 character. */
   if (length == 0u) {
      return false;
   }

   for (i = 0u; i < length; ++i) {
      const char c = str[i];

      /* RFC 6335: MUST contain only US-ASCII letters 'A' - 'Z' and 'a' - 'z',
       * digits '0' - '9', and hyphens ('-', ASCII 0x2D or decimal 45). */
      if (!isalpha (c) && !isdigit (c) && c != '-') {
         return false;
      }

      /* RFC 6335: hyphens MUST NOT be adjacent to other hyphens. */
      if (c == '-' && prev == '-') {
         return false;
      }

      num_alpha += isalpha (c) ? 1u : 0u;
      prev = c;
   }

   /* RFC 6335: MUST contain at least one letter ('A' - 'Z' or 'a' - 'z') */
   if (num_alpha == 0u) {
      return false;
   }

   /* RFC 6335: MUST NOT begin or end with a hyphen. */
   if (str[0] == '-' || str[length - 1u] == '-') {
      return false;
   }

   return true;
}

static bool
mongoc_uri_parse_tags (mongoc_uri_t *uri, /* IN */
                       const char *str)   /* IN */
{
   const char *end_keyval;
   const char *end_key;
   bson_t b;
   char *keyval;
   char *key;

   bson_init (&b);

again:
   if ((keyval = scan_to_unichar (str, ',', "", &end_keyval))) {
      if (!(key = scan_to_unichar (keyval, ':', "", &end_key))) {
         bson_free (keyval);
         goto fail;
      }

      bson_append_utf8 (&b, key, -1, end_key + 1, -1);
      bson_free (key);
      bson_free (keyval);
      str = end_keyval + 1;
      goto again;
   } else if ((key = scan_to_unichar (str, ':', "", &end_key))) {
      bson_append_utf8 (&b, key, -1, end_key + 1, -1);
      bson_free (key);
   } else if (strlen (str)) {
      /* we're not finished but we couldn't parse the string */
      goto fail;
   }

   mongoc_read_prefs_add_tag (uri->read_prefs, &b);
   bson_destroy (&b);

   return true;

fail:
   MONGOC_WARNING ("Unsupported value for \"" MONGOC_URI_READPREFERENCETAGS "\": \"%s\"", str);
   bson_destroy (&b);
   return false;
}


/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_bson_append_or_replace_key --
 *
 *
 *       Appends 'option' to the end of 'options' if not already set.
 *
 *       Since we cannot grow utf8 strings inline, we have to allocate a
 *       temporary bson variable and splice in the new value if the key
 *       is already set.
 *
 *       NOTE: This function keeps the order of the BSON keys.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 *
 *--------------------------------------------------------------------------
 */

static void
mongoc_uri_bson_append_or_replace_key (bson_t *options, const char *option, const char *value)
{
   bson_iter_t iter;
   bool found = false;

   if (bson_iter_init (&iter, options)) {
      bson_t tmp = BSON_INITIALIZER;

      while (bson_iter_next (&iter)) {
         const bson_value_t *bvalue;

         if (!strcasecmp (bson_iter_key (&iter), option)) {
            bson_append_utf8 (&tmp, option, -1, value, -1);
            found = true;
            continue;
         }

         bvalue = bson_iter_value (&iter);
         BSON_APPEND_VALUE (&tmp, bson_iter_key (&iter), bvalue);
      }

      if (!found) {
         bson_append_utf8 (&tmp, option, -1, value, -1);
      }

      bson_destroy (options);
      bson_copy_to (&tmp, options);
      bson_destroy (&tmp);
   }
}


bool
mongoc_uri_has_option (const mongoc_uri_t *uri, const char *key)
{
   bson_iter_t iter;

   return bson_iter_init_find_case (&iter, &uri->options, key);
}

bool
mongoc_uri_option_is_int32 (const char *key)
{
   return mongoc_uri_option_is_int64 (key) || !strcasecmp (key, MONGOC_URI_CONNECTTIMEOUTMS) ||
          !strcasecmp (key, MONGOC_URI_HEARTBEATFREQUENCYMS) ||
          !strcasecmp (key, MONGOC_URI_SERVERSELECTIONTIMEOUTMS) ||
          !strcasecmp (key, MONGOC_URI_SOCKETCHECKINTERVALMS) || !strcasecmp (key, MONGOC_URI_SOCKETTIMEOUTMS) ||
          !strcasecmp (key, MONGOC_URI_LOCALTHRESHOLDMS) || !strcasecmp (key, MONGOC_URI_MAXPOOLSIZE) ||
          !strcasecmp (key, MONGOC_URI_MAXSTALENESSSECONDS) || !strcasecmp (key, MONGOC_URI_WAITQUEUETIMEOUTMS) ||
          !strcasecmp (key, MONGOC_URI_ZLIBCOMPRESSIONLEVEL) || !strcasecmp (key, MONGOC_URI_SRVMAXHOSTS);
}

bool
mongoc_uri_option_is_int64 (const char *key)
{
   return !strcasecmp (key, MONGOC_URI_WTIMEOUTMS);
}

bool
mongoc_uri_option_is_bool (const char *key)
{
   // CDRIVER-5933
   if (!strcasecmp (key, MONGOC_URI_CANONICALIZEHOSTNAME)) {
      MONGOC_WARNING (MONGOC_URI_CANONICALIZEHOSTNAME " is deprecated, use " MONGOC_URI_AUTHMECHANISMPROPERTIES
                                                      " with CANONICALIZE_HOST_NAME instead");
      return true;
   }

   return !strcasecmp (key, MONGOC_URI_DIRECTCONNECTION) || !strcasecmp (key, MONGOC_URI_JOURNAL) ||
          !strcasecmp (key, MONGOC_URI_RETRYREADS) || !strcasecmp (key, MONGOC_URI_RETRYWRITES) ||
          !strcasecmp (key, MONGOC_URI_SAFE) || !strcasecmp (key, MONGOC_URI_SERVERSELECTIONTRYONCE) ||
          !strcasecmp (key, MONGOC_URI_TLS) || !strcasecmp (key, MONGOC_URI_TLSINSECURE) ||
          !strcasecmp (key, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES) ||
          !strcasecmp (key, MONGOC_URI_TLSALLOWINVALIDHOSTNAMES) ||
          !strcasecmp (key, MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK) ||
          !strcasecmp (key, MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK) || !strcasecmp (key, MONGOC_URI_LOADBALANCED) ||
          /* deprecated options with canonical equivalents */
          !strcasecmp (key, MONGOC_URI_SSL) || !strcasecmp (key, MONGOC_URI_SSLALLOWINVALIDCERTIFICATES) ||
          !strcasecmp (key, MONGOC_URI_SSLALLOWINVALIDHOSTNAMES);
}

bool
mongoc_uri_option_is_utf8 (const char *key)
{
   return !strcasecmp (key, MONGOC_URI_APPNAME) || !strcasecmp (key, MONGOC_URI_REPLICASET) ||
          !strcasecmp (key, MONGOC_URI_READPREFERENCE) || !strcasecmp (key, MONGOC_URI_SERVERMONITORINGMODE) ||
          !strcasecmp (key, MONGOC_URI_SRVSERVICENAME) || !strcasecmp (key, MONGOC_URI_TLSCERTIFICATEKEYFILE) ||
          !strcasecmp (key, MONGOC_URI_TLSCERTIFICATEKEYFILEPASSWORD) || !strcasecmp (key, MONGOC_URI_TLSCAFILE) ||
          /* deprecated options with canonical equivalents */
          !strcasecmp (key, MONGOC_URI_SSLCLIENTCERTIFICATEKEYFILE) ||
          !strcasecmp (key, MONGOC_URI_SSLCLIENTCERTIFICATEKEYPASSWORD) ||
          !strcasecmp (key, MONGOC_URI_SSLCERTIFICATEAUTHORITYFILE);
}

const char *
mongoc_uri_canonicalize_option (const char *key)
{
   if (!strcasecmp (key, MONGOC_URI_SSL)) {
      return MONGOC_URI_TLS;
   } else if (!strcasecmp (key, MONGOC_URI_SSLCLIENTCERTIFICATEKEYFILE)) {
      return MONGOC_URI_TLSCERTIFICATEKEYFILE;
   } else if (!strcasecmp (key, MONGOC_URI_SSLCLIENTCERTIFICATEKEYPASSWORD)) {
      return MONGOC_URI_TLSCERTIFICATEKEYFILEPASSWORD;
   } else if (!strcasecmp (key, MONGOC_URI_SSLCERTIFICATEAUTHORITYFILE)) {
      return MONGOC_URI_TLSCAFILE;
   } else if (!strcasecmp (key, MONGOC_URI_SSLALLOWINVALIDCERTIFICATES)) {
      return MONGOC_URI_TLSALLOWINVALIDCERTIFICATES;
   } else if (!strcasecmp (key, MONGOC_URI_SSLALLOWINVALIDHOSTNAMES)) {
      return MONGOC_URI_TLSALLOWINVALIDHOSTNAMES;
   } else {
      return key;
   }
}

static bool
_mongoc_uri_parse_int64 (const char *key, const char *value, int64_t *result)
{
   char *endptr;
   int64_t i;

   errno = 0;
   i = bson_ascii_strtoll (value, &endptr, 10);
   if (errno || endptr < value + strlen (value)) {
      MONGOC_WARNING ("Invalid %s: cannot parse integer\n", key);
      return false;
   }

   *result = i;
   return true;
}


static bool
mongoc_uri_parse_int32 (const char *key, const char *value, int32_t *result)
{
   int64_t i;

   if (!_mongoc_uri_parse_int64 (key, value, &i)) {
      /* _mongoc_uri_parse_int64 emits a warning if it could not parse the
       * given value, so we don't have to add one here.
       */
      return false;
   }

   if (i > INT32_MAX || i < INT32_MIN) {
      MONGOC_WARNING ("Invalid %s: cannot fit in int32\n", key);
      return false;
   }

   *result = (int32_t) i;
   return true;
}


static bool
dns_option_allowed (const char *lkey)
{
   /* Initial DNS Seedlist Discovery Spec: "A Client MUST only support the
    * authSource, replicaSet, and loadBalanced options through a TXT record, and
    * MUST raise an error if any other option is encountered."
    */
   return !strcmp (lkey, MONGOC_URI_AUTHSOURCE) || !strcmp (lkey, MONGOC_URI_REPLICASET) ||
          !strcmp (lkey, MONGOC_URI_LOADBALANCED);
}


/* Decompose a key=val pair and place them into a document.
 * Includes case-folding for key portion.
 */
static bool
mongoc_uri_split_option (mongoc_uri_t *uri, bson_t *options, const char *str, bool from_dns, bson_error_t *error)
{
   bson_iter_t iter;
   const char *end_key;
   char *key = NULL;
   char *lkey = NULL;
   char *value = NULL;
   const char *opt;
   char *opt_end;
   size_t opt_len;
   bool ret = false;

   if (!(key = scan_to_unichar (str, '=', "", &end_key))) {
      MONGOC_URI_ERROR (error, "URI option \"%s\" contains no \"=\" sign", str);
      goto CLEANUP;
   }

   value = bson_strdup (end_key + 1);
   mongoc_uri_do_unescape (&value);
   if (!value) {
      /* do_unescape detected invalid UTF-8 and freed value */
      MONGOC_URI_ERROR (error, "Value for URI option \"%s\" contains invalid UTF-8", key);
      goto CLEANUP;
   }

   lkey = bson_strdup (key);
   mongoc_lowercase (key, lkey);

   /* Initial DNS Seedlist Discovery Spec: "A Client MUST only support the
    * authSource, replicaSet, and loadBalanced options through a TXT record, and
    * MUST raise an error if any other option is encountered."*/
   if (from_dns && !dns_option_allowed (lkey)) {
      MONGOC_URI_ERROR (error, "URI option \"%s\" prohibited in TXT record", key);
      goto CLEANUP;
   }

   /* Special case: READPREFERENCETAGS is a composing option.
    * Multiple instances should append, not overwrite.
    * Encode them directly to the options field,
    * bypassing canonicalization and duplicate checks.
    */
   if (!strcmp (lkey, MONGOC_URI_READPREFERENCETAGS)) {
      if (!mongoc_uri_parse_tags (uri, value)) {
         MONGOC_URI_ERROR (error, "Unsupported value for \"%s\": \"%s\"", key, value);
         goto CLEANUP;
      }
   } else if (bson_iter_init_find (&iter, &uri->raw, lkey) || bson_iter_init_find (&iter, options, lkey)) {
      /* Special case, MONGOC_URI_W == "any non-int" is not overridden
       * by later values.
       */
      if (!strcmp (lkey, MONGOC_URI_W) && (opt = bson_iter_utf8_unsafe (&iter, &opt_len))) {
         strtol (opt, &opt_end, 10);
         if (*opt_end != '\0') {
            ret = true;
            goto CLEANUP;
         }
      }

      /* Initial DNS Seedlist Discovery Spec: "Client MUST use options
       * specified in the Connection String to override options provided
       * through TXT records." So, do NOT override existing options with TXT
       * options. */
      if (from_dns) {
         if (0 == strcmp (lkey, MONGOC_URI_AUTHSOURCE)) {
            // Treat `authSource` as a special case. A server may support authentication with multiple mechanisms.
            // MONGODB-X509 requires authSource=$external. SCRAM-SHA-256 requires authSource=admin.
            // Only log a trace message since this may be expected.
            TRACE ("Ignoring URI option \"%s\" from TXT record \"%s\". Option is already present in URI", key, str);
         } else {
            MONGOC_WARNING (
               "Ignoring URI option \"%s\" from TXT record \"%s\". Option is already present in URI", key, str);
         }
         ret = true;
         goto CLEANUP;
      }
      MONGOC_WARNING ("Overwriting previously provided value for '%s'", key);
   }

   if (!(strcmp (lkey, MONGOC_URI_REPLICASET)) && *value == '\0') {
      MONGOC_URI_ERROR (error, "Value for URI option \"%s\" cannot be empty string", lkey);
      goto CLEANUP;
   }

   mongoc_uri_bson_append_or_replace_key (options, lkey, value);
   ret = true;

CLEANUP:
   bson_free (key);
   bson_free (lkey);
   bson_free (value);

   return ret;
}


/* Check for canonical/deprecated conflicts
 * between the option list a, and b.
 * If both names exist either way with differing values, error.
 */
static bool
mongoc_uri_options_validate_names (const bson_t *a, const bson_t *b, bson_error_t *error)
{
   bson_iter_t key_iter, canon_iter;
   const char *key = NULL;
   const char *canon = NULL;
   const char *value = NULL;
   const char *cval = NULL;
   size_t value_len = 0;
   size_t cval_len = 0;

   /* Scan `a` looking for deprecated names
    * where the canonical name was also used in `a`,
    * or was used in `b`. */
   bson_iter_init (&key_iter, a);
   while (bson_iter_next (&key_iter)) {
      key = bson_iter_key (&key_iter);
      value = bson_iter_utf8_unsafe (&key_iter, &value_len);
      canon = mongoc_uri_canonicalize_option (key);

      if (key == canon) {
         /* Canonical form, no point checking `b`. */
         continue;
      }

      /* Check for a conflict in `a`. */
      if (bson_iter_init_find (&canon_iter, a, canon)) {
         cval = bson_iter_utf8_unsafe (&canon_iter, &cval_len);
         if ((value_len != cval_len) || strcmp (value, cval)) {
            goto HANDLE_CONFLICT;
         }
      }

      /* Check for a conflict in `b`. */
      if (bson_iter_init_find (&canon_iter, b, canon)) {
         cval = bson_iter_utf8_unsafe (&canon_iter, &cval_len);
         if ((value_len != cval_len) || strcmp (value, cval)) {
            goto HANDLE_CONFLICT;
         }
      }
   }

   return true;

HANDLE_CONFLICT:
   MONGOC_URI_ERROR (error,
                     "Deprecated option '%s=%s' conflicts with "
                     "canonical name '%s=%s'",
                     key,
                     value,
                     canon,
                     cval);

   return false;
}


#define HANDLE_DUPE()                                                            \
   if (from_dns) {                                                               \
      MONGOC_WARNING ("Cannot override URI option \"%s\" from TXT record", key); \
      continue;                                                                  \
   } else if (1) {                                                               \
      MONGOC_WARNING ("Overwriting previously provided value for '%s'", key);    \
   } else                                                                        \
      (void) 0

static bool
mongoc_uri_apply_options (mongoc_uri_t *uri, const bson_t *options, bool from_dns, bson_error_t *error)
{
   bson_iter_t iter;
   int32_t v_int;
   int64_t v_int64;
   const char *key = NULL;
   const char *canon = NULL;
   const char *value = NULL;
   size_t value_len;
   bool bval;

   bson_iter_init (&iter, options);
   while (bson_iter_next (&iter)) {
      key = bson_iter_key (&iter);
      canon = mongoc_uri_canonicalize_option (key);
      value = bson_iter_utf8_unsafe (&iter, &value_len);

      /* Keep a record of how the option was originally presented. */
      mongoc_uri_bson_append_or_replace_key (&uri->raw, key, value);

      /* This check precedes mongoc_uri_option_is_int32 as all 64-bit values are
       * also recognised as 32-bit ints.
       */
      if (mongoc_uri_option_is_int64 (key)) {
         if (0 < strlen (value)) {
            if (!_mongoc_uri_parse_int64 (key, value, &v_int64)) {
               goto UNSUPPORTED_VALUE;
            }

            if (!_mongoc_uri_set_option_as_int64_with_error (uri, canon, v_int64, error)) {
               return false;
            }
         } else {
            MONGOC_WARNING ("Empty value provided for \"%s\"", key);
         }
      } else if (mongoc_uri_option_is_int32 (key)) {
         if (0 < strlen (value)) {
            if (!mongoc_uri_parse_int32 (key, value, &v_int)) {
               goto UNSUPPORTED_VALUE;
            }

            if (!_mongoc_uri_set_option_as_int32_with_error (uri, canon, v_int, error)) {
               return false;
            }
         } else {
            MONGOC_WARNING ("Empty value provided for \"%s\"", key);
         }
      } else if (!strcmp (key, MONGOC_URI_W)) {
         if (*value == '-' || isdigit (*value)) {
            v_int = (int) strtol (value, NULL, 10);
            _mongoc_uri_set_option_as_int32 (uri, MONGOC_URI_W, v_int);
         } else if (0 == strcasecmp (value, "majority")) {
            mongoc_uri_bson_append_or_replace_key (&uri->options, MONGOC_URI_W, "majority");
         } else if (*value) {
            mongoc_uri_bson_append_or_replace_key (&uri->options, MONGOC_URI_W, value);
         }

      } else if (mongoc_uri_option_is_bool (key)) {
         if (0 < strlen (value)) {
            if (0 == strcasecmp (value, "true")) {
               bval = true;
            } else if (0 == strcasecmp (value, "false")) {
               bval = false;
            } else if ((0 == strcmp (value, "1")) || (0 == strcasecmp (value, "yes")) ||
                       (0 == strcasecmp (value, "y")) || (0 == strcasecmp (value, "t"))) {
               MONGOC_WARNING ("Deprecated boolean value for \"%s\": \"%s\", "
                               "please update to \"%s=true\"",
                               key,
                               value,
                               key);
               bval = true;
            } else if ((0 == strcasecmp (value, "0")) || (0 == strcasecmp (value, "-1")) ||
                       (0 == strcmp (value, "no")) || (0 == strcmp (value, "n")) || (0 == strcmp (value, "f"))) {
               MONGOC_WARNING ("Deprecated boolean value for \"%s\": \"%s\", "
                               "please update to \"%s=false\"",
                               key,
                               value,
                               key);
               bval = false;
            } else {
               goto UNSUPPORTED_VALUE;
            }

            if (!mongoc_uri_set_option_as_bool (uri, canon, bval)) {
               _mongoc_set_error (
                  error, MONGOC_ERROR_COMMAND, MONGOC_ERROR_COMMAND_INVALID_ARG, "Failed to set %s to %d", canon, bval);
               return false;
            }
         } else {
            MONGOC_WARNING ("Empty value provided for \"%s\"", key);
         }

      } else if (!strcmp (key, MONGOC_URI_READPREFERENCETAGS)) {
         /* Skip this option here.
          * It was marshalled during mongoc_uri_split_option()
          * as a special case composing option.
          */

      } else if (!strcmp (key, MONGOC_URI_AUTHMECHANISM) || !strcmp (key, MONGOC_URI_AUTHSOURCE)) {
         if (bson_has_field (&uri->credentials, key)) {
            HANDLE_DUPE ();
         }
         mongoc_uri_bson_append_or_replace_key (&uri->credentials, canon, value);

      } else if (!strcmp (key, MONGOC_URI_READCONCERNLEVEL)) {
         if (!mongoc_read_concern_is_default (uri->read_concern)) {
            HANDLE_DUPE ();
         }
         mongoc_read_concern_set_level (uri->read_concern, value);

      } else if (!strcmp (key, MONGOC_URI_GSSAPISERVICENAME)) {
         char *tmp = bson_strdup_printf ("SERVICE_NAME:%s", value);
         if (bson_has_field (&uri->credentials, MONGOC_URI_AUTHMECHANISMPROPERTIES)) {
            MONGOC_WARNING ("authMechanismProperties SERVICE_NAME already set, "
                            "ignoring '%s'",
                            key);
         } else {
            // CDRIVER-5933
            MONGOC_WARNING (MONGOC_URI_GSSAPISERVICENAME " is deprecated, use " MONGOC_URI_AUTHMECHANISMPROPERTIES
                                                         " with SERVICE_NAME instead");

            if (!mongoc_uri_parse_auth_mechanism_properties (uri, tmp)) {
               bson_free (tmp);
               goto UNSUPPORTED_VALUE;
            }
         }
         bson_free (tmp);

      } else if (!strcmp (key, MONGOC_URI_SRVSERVICENAME)) {
         if (!mongoc_uri_check_srv_service_name (uri, value)) {
            goto UNSUPPORTED_VALUE;
         }
         mongoc_uri_bson_append_or_replace_key (&uri->options, canon, value);

      } else if (!strcmp (key, MONGOC_URI_AUTHMECHANISMPROPERTIES)) {
         if (bson_has_field (&uri->credentials, key)) {
            HANDLE_DUPE ();
         }
         if (!mongoc_uri_parse_auth_mechanism_properties (uri, value)) {
            goto UNSUPPORTED_VALUE;
         }

      } else if (!strcmp (key, MONGOC_URI_APPNAME)) {
         /* Part of uri->options */
         if (!mongoc_uri_set_appname (uri, value)) {
            goto UNSUPPORTED_VALUE;
         }

      } else if (!strcmp (key, MONGOC_URI_COMPRESSORS)) {
         if (!bson_empty (mongoc_uri_get_compressors (uri))) {
            HANDLE_DUPE ();
         }
         if (!mongoc_uri_set_compressors (uri, value)) {
            goto UNSUPPORTED_VALUE;
         }

      } else if (!strcmp (key, MONGOC_URI_SERVERMONITORINGMODE)) {
         if (!mongoc_uri_set_server_monitoring_mode (uri, value)) {
            goto UNSUPPORTED_VALUE;
         }

      } else if (mongoc_uri_option_is_utf8 (key)) {
         mongoc_uri_bson_append_or_replace_key (&uri->options, canon, value);

      } else {
         /*
          * Keys that aren't supported by a driver MUST be ignored.
          *
          * A WARN level logging message MUST be issued
          * https://github.com/mongodb/specifications/blob/master/source/connection-string/connection-string-spec.md#keys
          */
         MONGOC_WARNING ("Unsupported URI option \"%s\"", key);
      }
   }

   return true;

UNSUPPORTED_VALUE:
   MONGOC_URI_ERROR (error, "Unsupported value for \"%s\": \"%s\"", key, value);

   return false;
}


/* Processes a query string formatted set of driver options
 * (i.e. tls=true&connectTimeoutMS=250 ) into a BSON dict of values.
 * uri->raw is initially populated with the raw split of key/value pairs,
 * then the keys are canonicalized and the values coerced
 * to their appropriate type and stored in uri->options.
 */
bool
mongoc_uri_parse_options (mongoc_uri_t *uri, const char *str, bool from_dns, bson_error_t *error)
{
   bson_t options;
   const char *end_option;
   char *option;

   bson_init (&options);
   while ((option = scan_to_unichar (str, '&', "", &end_option))) {
      if (!mongoc_uri_split_option (uri, &options, option, from_dns, error)) {
         bson_free (option);
         bson_destroy (&options);
         return false;
      }
      bson_free (option);
      str = end_option + 1;
   }

   if (*str && !mongoc_uri_split_option (uri, &options, str, from_dns, error)) {
      bson_destroy (&options);
      return false;
   }

   /* Walk both sides of this map to handle each ordering:
    * deprecated first canonical later, and vice-versa.
    * Then finalize parse by writing final values to uri->options.
    */
   if (!mongoc_uri_options_validate_names (&uri->options, &options, error) ||
       !mongoc_uri_options_validate_names (&options, &uri->options, error) ||
       !mongoc_uri_apply_options (uri, &options, from_dns, error)) {
      bson_destroy (&options);
      return false;
   }

   bson_destroy (&options);
   return true;
}


static bool
mongoc_uri_finalize_tls (mongoc_uri_t *uri, bson_error_t *error)
{
   /* Initial DNS Seedlist Discovery Spec: "If mongodb+srv is used, a driver
    * MUST implicitly also enable TLS." */
   if (uri->is_srv && !bson_has_field (&uri->options, MONGOC_URI_TLS)) {
      mongoc_uri_set_option_as_bool (uri, MONGOC_URI_TLS, true);
   }

   /* tlsInsecure implies tlsAllowInvalidCertificates, tlsAllowInvalidHostnames,
    * tlsDisableOCSPEndpointCheck, and tlsDisableCertificateRevocationCheck, so
    * consider it an error to have both. The user might have the wrong idea. */
   if (bson_has_field (&uri->options, MONGOC_URI_TLSINSECURE) &&
       (bson_has_field (&uri->options, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES) ||
        bson_has_field (&uri->options, MONGOC_URI_TLSALLOWINVALIDHOSTNAMES) ||
        bson_has_field (&uri->options, MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK) ||
        bson_has_field (&uri->options, MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK))) {
      MONGOC_URI_ERROR (error,
                        "%s may not be specified with %s, %s, %s, or %s",
                        MONGOC_URI_TLSINSECURE,
                        MONGOC_URI_TLSALLOWINVALIDCERTIFICATES,
                        MONGOC_URI_TLSALLOWINVALIDHOSTNAMES,
                        MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK,
                        MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK);
      return false;
   }

   /* tlsAllowInvalidCertificates implies tlsDisableOCSPEndpointCheck and
    * tlsDisableCertificateRevocationCheck, so consider it an error to have
    * both. The user might have the wrong idea. */
   if (bson_has_field (&uri->options, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES) &&
       (bson_has_field (&uri->options, MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK) ||
        bson_has_field (&uri->options, MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK))) {
      MONGOC_URI_ERROR (error,
                        "%s may not be specified with %s or %s",
                        MONGOC_URI_TLSALLOWINVALIDCERTIFICATES,
                        MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK,
                        MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK);
      return false;
   }

   /*  tlsDisableCertificateRevocationCheck implies tlsDisableOCSPEndpointCheck,
    * so consider it an error to have both. The user might have the wrong idea.
    */
   if (bson_has_field (&uri->options, MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK) &&
       bson_has_field (&uri->options, MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK)) {
      MONGOC_URI_ERROR (error,
                        "%s may not be specified with %s",
                        MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK,
                        MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK);
      return false;
   }

   return true;
}


typedef enum _mongoc_uri_finalize_validate {
   _mongoc_uri_finalize_allowed,
   _mongoc_uri_finalize_required,
   _mongoc_uri_finalize_prohibited,
} mongoc_uri_finalize_validate;


static bool
_finalize_auth_username (const char *username,
                         const char *mechanism,
                         mongoc_uri_finalize_validate validate,
                         bson_error_t *error)
{
   BSON_OPTIONAL_PARAM (username);
   BSON_ASSERT_PARAM (mechanism);
   BSON_OPTIONAL_PARAM (error);

   switch (validate) {
   case _mongoc_uri_finalize_required:
      if (!username || strlen (username) == 0u) {
         MONGOC_URI_ERROR (error, "'%s' authentication mechanism requires a username", mechanism);
         return false;
      }
      break;

   case _mongoc_uri_finalize_prohibited:
      if (username) {
         MONGOC_URI_ERROR (error, "'%s' authentication mechanism does not accept a username", mechanism);
         return false;
      }
      break;

   case _mongoc_uri_finalize_allowed:
   default:
      if (username && strlen (username) == 0u) {
         MONGOC_URI_ERROR (error, "'%s' authentication mechanism requires a non-empty username", mechanism);
         return false;
      }
      break;
   }

   return true;
}

// source MUST be "$external"
static bool
_finalize_auth_source_external (const char *source, const char *mechanism, bson_error_t *error)
{
   BSON_OPTIONAL_PARAM (source);
   BSON_ASSERT_PARAM (mechanism);
   BSON_OPTIONAL_PARAM (error);

   if (source && strcasecmp (source, "$external") != 0) {
      MONGOC_URI_ERROR (error,
                        "'%s' authentication mechanism requires \"$external\" authSource, but \"%s\" was specified",
                        mechanism,
                        source);
      return false;
   }

   return true;
}

// source MUST be "$external" and defaults to "$external".
static bool
_finalize_auth_source_default_external (mongoc_uri_t *uri,
                                        const char *source,
                                        const char *mechanism,
                                        bson_error_t *error)
{
   BSON_ASSERT_PARAM (uri);
   BSON_OPTIONAL_PARAM (source);
   BSON_ASSERT_PARAM (mechanism);
   BSON_OPTIONAL_PARAM (error);

   if (!source) {
      bsonBuildAppend (uri->credentials, kv (MONGOC_URI_AUTHSOURCE, cstr ("$external")));
      if (bsonBuildError) {
         MONGOC_URI_ERROR (error,
                           "unexpected URI credentials BSON error when attempting to default '%s' "
                           "authentication source to '$external': %s",
                           mechanism,
                           bsonBuildError);
         return false;
      }
      return true;
   } else {
      return _finalize_auth_source_external (source, mechanism, error);
   }
}

static bool
_finalize_auth_password (const char *password,
                         const char *mechanism,
                         mongoc_uri_finalize_validate validate,
                         bson_error_t *error)
{
   BSON_OPTIONAL_PARAM (password);
   BSON_ASSERT_PARAM (mechanism);
   BSON_OPTIONAL_PARAM (error);

   switch (validate) {
   case _mongoc_uri_finalize_required:
      // Passwords may be zero length.
      if (!password) {
         MONGOC_URI_ERROR (error, "'%s' authentication mechanism requires a password", mechanism);
         return false;
      }
      break;

   case _mongoc_uri_finalize_prohibited:
      if (password) {
         MONGOC_URI_ERROR (error, "'%s' authentication mechanism does not accept a password", mechanism);
         return false;
      }
      break;

   case _mongoc_uri_finalize_allowed:
   default:
      break;
   }

   return true;
}

typedef struct __supported_mechanism_properties {
   const char *name;
   bson_type_t type;
} supported_mechanism_properties;

static bool
_supported_mechanism_properties_check (const supported_mechanism_properties *supported_properties,
                                       const bson_t *mechanism_properties,
                                       const char *mechanism,
                                       bson_error_t *error)
{
   BSON_ASSERT_PARAM (supported_properties);
   BSON_ASSERT_PARAM (mechanism_properties);
   BSON_ASSERT_PARAM (mechanism);
   BSON_ASSERT_PARAM (error);

   bson_iter_t iter;
   BSON_ASSERT (bson_iter_init (&iter, mechanism_properties));

   // For each element in `MongoCredential.mechanism_properties`...
   while (bson_iter_next (&iter)) {
      const char *const key = bson_iter_key (&iter);

      // ... ensure it matches one of the supported mechanism property fields.
      for (const supported_mechanism_properties *prop = supported_properties; prop->name; ++prop) {
         // Authentication spec: naming of mechanism properties MUST be case-insensitive. For instance, SERVICE_NAME and
         // service_name refer to the same property.
         if (strcasecmp (key, prop->name) == 0) {
            const bson_type_t type = bson_iter_type (&iter);

            if (type == prop->type) {
               goto found_match; // Matches both key and type.
            } else {
               // Authentication spec: Drivers SHOULD raise an error as early as possible when detecting invalid values
               // in a credential. For instance, if a mechanism_property is specified for MONGODB-CR, the driver should
               // raise an error indicating that the property does not apply.
               //
               // Note: this overrides the Connection String spec: Any invalid Values for a given key MUST be ignored
               // and MUST log a WARN level message.
               MONGOC_URI_ERROR (error,
                                 "'%s' authentication mechanism property '%s' has incorrect type '%s', should be '%s'",
                                 key,
                                 mechanism,
                                 _mongoc_bson_type_to_str (type),
                                 _mongoc_bson_type_to_str (prop->type));
               return false;
            }
         }
      }

      // Authentication spec: Drivers SHOULD raise an error as early as possible when detecting invalid values in a
      // credential. For instance, if a mechanism_property is specified for MONGODB-CR, the driver should raise an error
      // indicating that the property does not apply.
      //
      // Note: this overrides the Connection String spec: Any invalid Values for a given key MUST be ignored and MUST
      // log a WARN level message.
      MONGOC_URI_ERROR (error, "Unsupported '%s' authentication mechanism property: '%s'", mechanism, key);
      return false;

   found_match:
      continue;
   }

   return true;
}

static bool
_finalize_auth_gssapi_mechanism_properties (const bson_t *mechanism_properties, bson_error_t *error)
{
   BSON_OPTIONAL_PARAM (mechanism_properties);
   BSON_ASSERT_PARAM (error);

   static const supported_mechanism_properties supported_properties[] = {
      {"SERVICE_NAME", BSON_TYPE_UTF8},
      {"CANONICALIZE_HOST_NAME", BSON_TYPE_UTF8}, // CDRIVER-4128: UTF-8 even when "false" or "true".
      {"SERVICE_REALM", BSON_TYPE_UTF8},
      {"SERVICE_HOST", BSON_TYPE_UTF8},
      {0},
   };

   if (mechanism_properties) {
      return _supported_mechanism_properties_check (supported_properties, mechanism_properties, "GSSAPI", error);
   }

   return true;
}

static bool
_finalize_auth_aws_mechanism_properties (const bson_t *mechanism_properties, bson_error_t *error)
{
   BSON_OPTIONAL_PARAM (mechanism_properties);
   BSON_ASSERT_PARAM (error);

   static const supported_mechanism_properties supported_properties[] = {
      {"AWS_SESSION_TOKEN", BSON_TYPE_UTF8},
      {0},
   };

   if (mechanism_properties) {
      return _supported_mechanism_properties_check (supported_properties, mechanism_properties, "MONGODB-AWS", error);
   }

   return true;
}

static bool
_finalize_auth_oidc_mechanism_properties (const bson_t *mechanism_properties, bson_error_t *error)
{
   BSON_OPTIONAL_PARAM (mechanism_properties);
   BSON_ASSERT_PARAM (error);

   static const supported_mechanism_properties supported_properties[] = {
      {"ENVIRONMENT", BSON_TYPE_UTF8},
      {"TOKEN_RESOURCE", BSON_TYPE_UTF8},
      {0},
   };

   if (mechanism_properties) {
      return _supported_mechanism_properties_check (supported_properties, mechanism_properties, "MONGODB-OIDC", error);
   }

   return true;
}

static bool
mongoc_uri_finalize_auth (mongoc_uri_t *uri, bson_error_t *error)
{
   BSON_ASSERT_PARAM (uri);
   BSON_OPTIONAL_PARAM (error);

   // Most validation of MongoCredential fields below according to the Authentication spec must be deferred to the
   // implementation of the Authentication Handshake algorithm (i.e. `_mongoc_cluster_auth_node`) due to support for
   // partial and late setting of credential fields via `mongoc_uri_set_*` functions. Limit validation to requirements
   // for individual field which are explicitly specified. Do not validate requirements on fields in relation to one
   // another (e.g. "given field A, field B must..."). The username, password, and authSource credential fields are
   // exceptions to this rule for both backward compatibility and spec test compliance.

   bool ret = false;

   bson_iter_t iter;

   const char *const mechanism = mongoc_uri_get_auth_mechanism (uri);
   const char *const username = mongoc_uri_get_username (uri);
   const char *const password = mongoc_uri_get_password (uri);
   const char *const source =
      bson_iter_init_find_case (&iter, &uri->credentials, MONGOC_URI_AUTHSOURCE) ? bson_iter_utf8 (&iter, NULL) : NULL;

   // Satisfy Connection String spec test: "must raise an error when the authSource is empty".
   // This applies even before determining whether or not authentication is required.
   if (source && strlen (source) == 0) {
      MONGOC_URI_ERROR (error, "%s", "authSource may not be specified as an empty string");
      return false;
   }

   // Authentication spec: The presence of a credential delimiter (i.e. '@') in the URI connection string is
   // evidence that the user has unambiguously specified user information and MUST be interpreted as a user
   // configuring authentication credentials (even if the username and/or password are empty strings).
   //
   // Note: username is always set when the credential delimiter `@` is present in the URI as parsed by
   // `mongoc_uri_parse_userpass`.
   //
   // If neither an authentication mechanism nor a username is provided, there is nothing to do.
   if (!mechanism && !username) {
      return true;
   } else {
      // All code below assumes authentication credentials are being configured.
   }

   bson_t *mechanism_properties = NULL;
   bson_t mechanism_properties_owner;
   {
      bson_t tmp;
      if (mongoc_uri_get_mechanism_properties (uri, &tmp)) {
         bson_copy_to (&tmp, &mechanism_properties_owner); // Avoid invalidation by updates to `uri->credentials`.
         mechanism_properties = &mechanism_properties_owner;
      } else {
         bson_init (&mechanism_properties_owner); // Ensure initialization.
      }
   }

   // Default authentication method.
   if (!mechanism) {
      // The authentication mechanism will be derived by `_mongoc_cluster_auth_node` during handshake according to
      // `saslSupportedMechs`.

      // Authentication spec: username: MUST be specified and non-zero length.
      // Default authentication method is used when no mechanism is specified but a username is present; see the
      // `!mechanism && !username` check above.
      if (!_finalize_auth_username (username, "default", _mongoc_uri_finalize_required, error)) {
         goto fail;
      }

      // Defer remaining validation of `MongoCredential` fields to Authentication Handshake.
   }

   // SCRAM-SHA-1, SCRAM-SHA-256, and PLAIN (same validation requirements)
   else if (strcasecmp (mechanism, "SCRAM-SHA-1") == 0 || strcasecmp (mechanism, "SCRAM-SHA-256") == 0 ||
            strcasecmp (mechanism, "PLAIN") == 0) {
      // Authentication spec: username: MUST be specified and non-zero length.
      if (!_finalize_auth_username (username, mechanism, _mongoc_uri_finalize_required, error)) {
         goto fail;
      }

      // Authentication spec: password: MUST be specified.
      if (!_finalize_auth_password (password, mechanism, _mongoc_uri_finalize_required, error)) {
         goto fail;
      }

      // Defer remaining validation of `MongoCredential` fields to Authentication Handshake.
   }

   // MONGODB-X509
   else if (strcasecmp (mechanism, "MONGODB-X509") == 0) {
      // `MongoCredential.username` SHOULD NOT be provided for MongoDB 3.4 and newer.
      // CDRIVER-1959: allow for backward compatibility until the spec states "MUST NOT" instead of "SHOULD NOT" and
      // spec tests are updated accordingly to permit warnings or errors.
      if (!_finalize_auth_username (username, mechanism, _mongoc_uri_finalize_allowed, error)) {
         goto fail;
      }

      // Authentication spec: password: MUST NOT be specified.
      if (!_finalize_auth_password (password, mechanism, _mongoc_uri_finalize_prohibited, error)) {
         goto fail;
      }

      // Authentication spec: source: MUST be "$external" and defaults to "$external".
      if (!_finalize_auth_source_default_external (uri, source, mechanism, error)) {
         goto fail;
      }

      // Defer remaining validation of `MongoCredential` fields to Authentication Handshake.
   }

   // GSSAPI
   else if (strcasecmp (mechanism, "GSSAPI") == 0) {
      // Authentication spec: username: MUST be specified and non-zero length.
      if (!_finalize_auth_username (username, mechanism, _mongoc_uri_finalize_required, error)) {
         goto fail;
      }

      // Authentication spec: source: MUST be "$external" and defaults to "$external".
      if (!_finalize_auth_source_default_external (uri, source, mechanism, error)) {
         goto fail;
      }

      // Authentication spec: password: MAY be specified.
      if (!_finalize_auth_password (password, mechanism, _mongoc_uri_finalize_allowed, error)) {
         goto fail;
      }

      // `MongoCredentials.mechanism_properties` are allowed for GSSAPI.
      if (!_finalize_auth_gssapi_mechanism_properties (mechanism_properties, error)) {
         goto fail;
      }

      // Authentication spec: valid values for CANONICALIZE_HOST_NAME are true, false, "none", "forward",
      // "forwardAndReverse". If a value is provided that does not match one of these the driver MUST raise an error.
      if (mechanism_properties) {
         bsonParse (*mechanism_properties,
                    find (iKeyWithType ("CANONICALIZE_HOST_NAME", utf8),
                          case (when (iStrEqual ("true"), nop),
                                when (iStrEqual ("false"), nop),
                                // CDRIVER-4128: only legacy boolean values are currently supported.
                                else (do ({
                                   bsonParseError =
                                      "'GSSAPI' authentication mechanism requires CANONICALIZE_HOST_NAME is either "
                                      "\"true\" or \"false\"";
                                })))));
         if (bsonParseError) {
            MONGOC_URI_ERROR (error, "%s", bsonParseError);
            goto fail;
         }
      }

      // Authentication spec: Drivers MUST allow the user to specify a different service name. The default is
      // "mongodb".
      if (!mechanism_properties || !bson_iter_init_find_case (&iter, mechanism_properties, "SERVICE_NAME")) {
         bsonBuildDecl (props,
                        if (mechanism_properties, then (insert (*mechanism_properties, always))),
                        kv ("SERVICE_NAME", cstr ("mongodb")));
         const bool success = !bsonBuildError && mongoc_uri_set_mechanism_properties (uri, &props);
         bson_destroy (&props);
         if (!success) {
            MONGOC_URI_ERROR (error,
                              "unexpected URI credentials BSON error when attempting to default 'GSSAPI' "
                              "authentication mechanism property 'SERVICE_NAME' to 'mongodb': %s",
                              bsonBuildError ? bsonBuildError : "mongoc_uri_set_mechanism_properties failed");
            goto fail;
         }
      }

      // Defer remaining validation of `MongoCredential` fields to Authentication Handshake.
   }

   // MONGODB-AWS
   else if (strcasecmp (mechanism, "MONGODB-AWS") == 0) {
      // Authentication spec: username: MAY be specified (as the non-sensitive AWS access key).
      if (!_finalize_auth_username (username, mechanism, _mongoc_uri_finalize_allowed, error)) {
         goto fail;
      }

      // Authentication spec: source: MUST be "$external" and defaults to "$external".
      if (!_finalize_auth_source_default_external (uri, source, mechanism, error)) {
         goto fail;
      }

      // Authentication spec: password: MAY be specified (as the sensitive AWS secret key).
      if (!_finalize_auth_password (password, mechanism, _mongoc_uri_finalize_allowed, error)) {
         goto fail;
      }

      // mechanism_properties are allowed for MONGODB-AWS.
      if (!_finalize_auth_aws_mechanism_properties (mechanism_properties, error)) {
         goto fail;
      }

      // Authentication spec: if a username is provided without a password (or vice-versa), Drivers MUST raise an error.
      if (!username != !password) {
         MONGOC_URI_ERROR (error,
                           "'%s' authentication mechanism does not accept a username or a password without the other",
                           mechanism);
         goto fail;
      }

      // Defer remaining validation of `MongoCredential` fields to Authentication Handshake.
   }

   // MONGODB-OIDC
   else if (strcasecmp (mechanism, "MONGODB-OIDC") == 0) {
      // Authentication spec: username: MAY be specified (with callback/environment defined meaning).
      if (!_finalize_auth_username (username, mechanism, _mongoc_uri_finalize_allowed, error)) {
         goto fail;
      }

      // Authentication spec: source: MUST be "$external" and defaults to "$external".
      if (!_finalize_auth_source_default_external (uri, source, mechanism, error)) {
         goto fail;
      }

      // Authentication spec: password: MUST NOT be specified.
      if (!_finalize_auth_password (password, mechanism, _mongoc_uri_finalize_prohibited, error)) {
         goto fail;
      }

      // mechanism_properties are allowed for MONGODB-OIDC.
      if (!_finalize_auth_oidc_mechanism_properties (mechanism_properties, error)) {
         goto fail;
      }

      // The environment is optional, but if specified it must appear valid.
      if (mechanism_properties && bson_iter_init_find_case (&iter, mechanism_properties, "ENVIRONMENT")) {
         if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
            MONGOC_URI_ERROR (error, "'%s' authentication has non-string %s property", mechanism, "ENVIRONMENT");
            goto fail;
         }

         const mongoc_oidc_env_t *env = mongoc_oidc_env_find (bson_iter_utf8 (&iter, NULL));
         if (!env) {
            MONGOC_URI_ERROR (error,
                              "'%s' authentication has unrecognized %s property '%s'",
                              mechanism,
                              "ENVIRONMENT",
                              bson_iter_utf8 (&iter, NULL));
            goto fail;
         }

         if (username && !mongoc_oidc_env_supports_username (env)) {
            MONGOC_URI_ERROR (error,
                              "'%s' authentication with %s environment does not accept a %s",
                              mechanism,
                              mongoc_oidc_env_name (env),
                              "username");
            goto fail;
         }

         if (bson_iter_init_find_case (&iter, mechanism_properties, "TOKEN_RESOURCE")) {
            if (!BSON_ITER_HOLDS_UTF8 (&iter)) {
               MONGOC_URI_ERROR (error, "'%s' authentication has non-string %s property", mechanism, "TOKEN_RESOURCE");
               goto fail;
            }

            if (!mongoc_oidc_env_requires_token_resource (env)) {
               MONGOC_URI_ERROR (error,
                                 "'%s' authentication with %s environment does not accept a %s",
                                 mechanism,
                                 mongoc_oidc_env_name (env),
                                 "TOKEN_RESOURCE");
               goto fail;
            }
         } else {
            if (mongoc_oidc_env_requires_token_resource (env)) {
               MONGOC_URI_ERROR (error,
                                 "'%s' authentication with %s environment requires a %s",
                                 mechanism,
                                 mongoc_oidc_env_name (env),
                                 "TOKEN_RESOURCE");
               goto fail;
            }
         }
      }

      // Defer remaining validation of `MongoCredential` fields to Authentication Handshake.
   }

   // Invalid or unsupported authentication mechanism.
   else {
      MONGOC_URI_ERROR (
         error,
         "Unsupported value for authMechanism '%s': must be one of "
         "['MONGODB-OIDC', 'SCRAM-SHA-1', 'SCRAM-SHA-256', 'PLAIN', 'MONGODB-X509', 'GSSAPI', 'MONGODB-AWS']",
         mechanism);
      goto fail;
   }

   ret = true;

fail:
   bson_destroy (&mechanism_properties_owner);

   return ret;
}

static bool
mongoc_uri_finalize_directconnection (mongoc_uri_t *uri, bson_error_t *error)
{
   bool directconnection = false;

   directconnection = mongoc_uri_get_option_as_bool (uri, MONGOC_URI_DIRECTCONNECTION, false);
   if (!directconnection) {
      return true;
   }

   /* URI options spec: "The driver MUST report an error if the
    * directConnection=true URI option is specified with an SRV URI, because
    * the URI may resolve to multiple hosts. The driver MUST allow specifying
    * directConnection=false URI option with an SRV URI." */
   if (uri->is_srv) {
      MONGOC_URI_ERROR (error, "%s", "SRV URI not allowed with directConnection option");
      return false;
   }

   /* URI options spec: "The driver MUST report an error if the
    * directConnection=true URI option is specified with multiple seeds." */
   if (uri->hosts && uri->hosts->next) {
      MONGOC_URI_ERROR (error, "%s", "Multiple seeds not allowed with directConnection option");
      return false;
   }

   return true;
}

static bool
mongoc_uri_parse_before_slash (mongoc_uri_t *uri, const char *before_slash, bson_error_t *error)
{
   char *userpass;
   const char *hosts;

   userpass = scan_to_unichar (before_slash, '@', "", &hosts);
   if (userpass) {
      if (!mongoc_uri_parse_userpass (uri, userpass, error)) {
         goto error;
      }

      hosts++; /* advance past "@" */
      if (*hosts == '@') {
         /* special case: "mongodb://alice@@localhost" */
         MONGOC_URI_ERROR (error, "Invalid username or password. %s", escape_instructions);
         goto error;
      }
   } else {
      hosts = before_slash;
   }

   if (uri->is_srv) {
      if (!mongoc_uri_parse_srv (uri, hosts, error)) {
         goto error;
      }
   } else {
      if (!mongoc_uri_parse_hosts (uri, hosts)) {
         MONGOC_URI_ERROR (error, "%s", "Invalid host string in URI");
         goto error;
      }
   }

   bson_free (userpass);
   return true;

error:
   bson_free (userpass);
   return false;
}


static bool
mongoc_uri_parse (mongoc_uri_t *uri, const char *str, bson_error_t *error)
{
   BSON_ASSERT_PARAM (uri);
   BSON_ASSERT_PARAM (str);

   const size_t str_len = strlen (str);

   if (!bson_utf8_validate (str, str_len, false /* allow_null */)) {
      MONGOC_URI_ERROR (error, "%s", "Invalid UTF-8 in URI");
      return false;
   }

   // Save for later.
   const char *const str_end = str + str_len;

   // Parse and remove scheme and its delimiter.
   // e.g. "mongodb://user:pass@host1:27017,host2:27018/database?key1=value1&key2=value2"
   //       ~~~~~~~~~~
   if (!mongoc_uri_parse_scheme (uri, str, &str)) {
      MONGOC_URI_ERROR (error, "%s", "Invalid URI Schema, expecting 'mongodb://' or 'mongodb+srv://'");
      return false;
   }
   // str -> "user:pass@host1:27017,host2:27018/database?key1=value1&key2=value2"

   // From this point forward, use this cursor to find the split between "userhosts" and "dbopts".
   const char *cursor = str;

   // Remove userinfo and its delimiter.
   // e.g. "user:pass@host1:27017,host2:27018/database?key1=value1&key2=value2"
   //       ~~~~~~~~~~
   {
      const char *tmp;

      // Only ':' is permitted among RFC-3986 gen-delims (":/?#[]@") in userinfo.
      // However, continue supporting these characters for backward compatibility, as permitted by the Connection
      // String spec: for backwards-compatibility reasons, drivers MAY allow reserved characters other than "@" and
      // ":" to be present in user information without percent-encoding.
      char *userinfo = scan_to_unichar (cursor, '@', "", &tmp);

      if (userinfo) {
         cursor = tmp + 1; // Consume userinfo delimiter.
         bson_free (userinfo);
      }
   }
   // cursor -> "host1:27017,host2:27018/database?key1=value1&key2=value2"

   // Find either the optional auth database delimiter or the query delimiter.
   // e.g. "host1:27017,host2:27018/database?key1=value1&key2=value2"
   //                              ^
   // e.g. "host1:27017,host2:27018?key1=value1&key2=value2"
   //                              ^
   {
      const char *tmp;

      // Only ':', '[', and ']' are permitted among RFC-3986 gen-delims (":/?#[]@") in hostinfo.
      const char *const terminators = "/?#@";

      char *hostinfo;

      // Optional auth delimiter is present.
      if ((hostinfo = scan_to_unichar (cursor, '/', terminators, &tmp))) {
         cursor = tmp; // Include the delimiter.
         bson_free (hostinfo);
      }

      // Query delimiter is present.
      else if ((hostinfo = scan_to_unichar (cursor, '?', terminators, &tmp))) {
         cursor = tmp; // Include the delimiter.
         bson_free (hostinfo);
      }

      // Neither delimiter is present. Entire rest of string is part of hostinfo.
      else {
         cursor = str_end; // Jump to end of string.
         BSON_ASSERT (*cursor == '\0');
      }
   }
   // cursor -> "/database?key1=value1&key2=value2"

   // Parse "userhosts". e.g. "user:pass@host1:27017,host2:27018"
   {
      char *const userhosts = bson_strndup (str, (size_t) (cursor - str));
      const bool ret = mongoc_uri_parse_before_slash (uri, userhosts, error);
      bson_free (userhosts);
      if (!ret) {
         return false;
      }
   }

   // Parse "dbopts". e.g. "/database?key1=value1&key2=value2"
   if (*cursor != '\0') {
      BSON_ASSERT (*cursor == '/' || *cursor == '?');

      // Parse the auth database.
      if (*cursor == '/') {
         ++cursor; // Consume the delimiter.

         // No auth database may be present even if the delimiter is present.
         // e.g. "mongodb://localhost:27017/"
         if (*cursor != '\0') {
            if (!mongoc_uri_parse_database (uri, cursor, &cursor)) {
               MONGOC_URI_ERROR (error, "%s", "Invalid database name in URI");
               return false;
            }
         }
      }

      // Parse the query options.
      if (*cursor == '?') {
         ++cursor; // Consume the delimiter.

         // No options may be present even if the delimiter is present.
         // e.g. "mongodb://localhost:27017?"
         if (*cursor != '\0') {
            if (!mongoc_uri_parse_options (uri, cursor, false /* from DNS */, error)) {
               return false;
            }
         }
      }
   }

   return mongoc_uri_finalize (uri, error);
}


const mongoc_host_list_t *
mongoc_uri_get_hosts (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return uri->hosts;
}


const char *
mongoc_uri_get_replica_set (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   if (bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_REPLICASET) && BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return NULL;
}


const bson_t *
mongoc_uri_get_credentials (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return &uri->credentials;
}


const char *
mongoc_uri_get_auth_mechanism (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   if (bson_iter_init_find_case (&iter, &uri->credentials, MONGOC_URI_AUTHMECHANISM) && BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return NULL;
}


bool
mongoc_uri_set_auth_mechanism (mongoc_uri_t *uri, const char *value)
{
   size_t len;

   BSON_ASSERT (value);

   len = strlen (value);

   if (!bson_utf8_validate (value, len, false)) {
      return false;
   }

   mongoc_uri_bson_append_or_replace_key (&uri->credentials, MONGOC_URI_AUTHMECHANISM, value);

   return true;
}


bool
mongoc_uri_get_mechanism_properties (const mongoc_uri_t *uri, bson_t *properties /* OUT */)
{
   bson_iter_t iter;

   BSON_ASSERT (uri);
   BSON_ASSERT (properties);

   if (bson_iter_init_find_case (&iter, &uri->credentials, MONGOC_URI_AUTHMECHANISMPROPERTIES) &&
       BSON_ITER_HOLDS_DOCUMENT (&iter)) {
      uint32_t len = 0;
      const uint8_t *data = NULL;

      bson_iter_document (&iter, &len, &data);
      BSON_ASSERT (bson_init_static (properties, data, len));

      return true;
   }

   return false;
}


bool
mongoc_uri_set_mechanism_properties (mongoc_uri_t *uri, const bson_t *properties)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (properties);

   bson_t tmp = BSON_INITIALIZER;
   bsonBuildAppend (tmp,
                    // Copy the existing credentials, dropping the existing properties if
                    // present
                    insert (uri->credentials, not(key (MONGOC_URI_AUTHMECHANISMPROPERTIES))),
                    // Append the new properties
                    kv (MONGOC_URI_AUTHMECHANISMPROPERTIES, bson (*properties)));
   bson_reinit (&uri->credentials);
   bsonBuildAppend (uri->credentials, insert (tmp, always));
   bson_destroy (&tmp);
   return bsonBuildError == NULL;
}


static bool
_mongoc_uri_assign_read_prefs_mode (mongoc_uri_t *uri, bson_error_t *error)
{
   BSON_ASSERT (uri);

   mongoc_read_mode_t mode = 0;
   const char *pref = NULL;
   bsonParse (uri->options,
              find (
                 // Find the 'readPreference' string
                 iKeyWithType (MONGOC_URI_READPREFERENCE, utf8),
                 case ( // Switch on the string content:
                    when (iStrEqual ("primary"), do (mode = MONGOC_READ_PRIMARY)),
                    when (iStrEqual ("primaryPreferred"), do (mode = MONGOC_READ_PRIMARY_PREFERRED)),
                    when (iStrEqual ("secondary"), do (mode = MONGOC_READ_SECONDARY)),
                    when (iStrEqual ("secondaryPreferred"), do (mode = MONGOC_READ_SECONDARY_PREFERRED)),
                    when (iStrEqual ("nearest"), do (mode = MONGOC_READ_NEAREST)),
                    else (do ({
                       pref = bsonAs (cstr);
                       bsonParseError = "Unsupported readPreference value";
                    })))));

   if (bsonParseError) {
      const char *prefix = "Error while assigning URI read preference";
      if (pref) {
         MONGOC_URI_ERROR (error, "%s: %s [readPreference=%s]", prefix, bsonParseError, pref);
      } else {
         MONGOC_URI_ERROR (error, "%s: %s", prefix, bsonParseError);
      }
      return false;
   }

   if (mode != 0) {
      mongoc_read_prefs_set_mode (uri->read_prefs, mode);
   }
   return true;
}


static bool
_mongoc_uri_build_write_concern (mongoc_uri_t *uri, bson_error_t *error)
{
   mongoc_write_concern_t *write_concern;
   int64_t wtimeoutms;

   BSON_ASSERT (uri);

   write_concern = mongoc_write_concern_new ();
   uri->write_concern = write_concern;

   bsonParse (uri->options,
              find (iKeyWithType (MONGOC_URI_SAFE, boolean),
                    do (mongoc_write_concern_set_w (write_concern,
                                                    bsonAs (boolean) ? 1 : MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED))));

   if (bsonParseError) {
      MONGOC_URI_ERROR (error, "Error while parsing 'safe' URI option: %s", bsonParseError);
      return false;
   }

   wtimeoutms = mongoc_uri_get_option_as_int64 (uri, MONGOC_URI_WTIMEOUTMS, 0);
   if (wtimeoutms < 0) {
      MONGOC_URI_ERROR (error, "Unsupported wtimeoutMS value [w=%" PRId64 "]", wtimeoutms);
      return false;
   } else if (wtimeoutms > 0) {
      mongoc_write_concern_set_wtimeout_int64 (write_concern, wtimeoutms);
   }

   bsonParse (uri->options,
              find (iKeyWithType (MONGOC_URI_JOURNAL, boolean),
                    do (mongoc_write_concern_set_journal (write_concern, bsonAs (boolean)))));
   if (bsonParseError) {
      MONGOC_URI_ERROR (error, "Error while parsing 'journal' URI option: %s", bsonParseError);
      return false;
   }

   int w_int = INT_MAX;
   const char *w_str = NULL;
   bsonParse (uri->options,
              find (iKey ("w"), //
                    storeInt32 (w_int),
                    storeStrRef (w_str),
                    case (
                       // Special W options:
                       when (eq (int32, MONGOC_WRITE_CONCERN_W_UNACKNOWLEDGED),
                             // These conflict with journalling:
                             if (eval (mongoc_write_concern_get_journal (write_concern)),
                                 then (error ("Journal conflicts with w value"))),
                             do (mongoc_write_concern_set_w (write_concern, bsonAs (int32)))),
                       // Other positive 'w' value:
                       when (allOf (type (int32), eval (bsonAs (int32) > 0)),
                             do (mongoc_write_concern_set_w (write_concern, bsonAs (int32)))),
                       // Special "majority" string:
                       when (iStrEqual ("majority"),
                             do (mongoc_write_concern_set_w (write_concern, MONGOC_WRITE_CONCERN_W_MAJORITY))),
                       // Other string:
                       when (type (utf8), do (mongoc_write_concern_set_wtag (write_concern, bsonAs (cstr)))),
                       // Invalid value:
                       else (error ("Unsupported w value")))));

   if (bsonParseError) {
      const char *const prefix = "Error while parsing the 'w' URI option";
      if (w_str) {
         MONGOC_URI_ERROR (error, "%s: %s [w=%s]", prefix, bsonParseError, w_str);
      } else if (w_int != INT_MAX) {
         MONGOC_URI_ERROR (error, "%s: %s [w=%d]", prefix, bsonParseError, w_int);
      } else {
         MONGOC_URI_ERROR (error, "%s: %s", prefix, bsonParseError);
      }
      return false;
   }
   return true;
}

/* can't use mongoc_uri_get_option_as_int32, it treats 0 specially */
static int32_t
_mongoc_uri_get_max_staleness_option (const mongoc_uri_t *uri)
{
   const bson_t *options;
   bson_iter_t iter;
   int32_t retval = MONGOC_NO_MAX_STALENESS;

   if ((options = mongoc_uri_get_options (uri)) &&
       bson_iter_init_find_case (&iter, options, MONGOC_URI_MAXSTALENESSSECONDS) && BSON_ITER_HOLDS_INT32 (&iter)) {
      retval = bson_iter_int32 (&iter);
      if (retval == 0) {
         MONGOC_WARNING ("Unsupported value for \"" MONGOC_URI_MAXSTALENESSSECONDS "\": \"%d\"", retval);
         retval = -1;
      } else if (retval < 0 && retval != -1) {
         MONGOC_WARNING ("Unsupported value for \"" MONGOC_URI_MAXSTALENESSSECONDS "\": \"%d\"", retval);
         retval = MONGOC_NO_MAX_STALENESS;
      }
   }

   return retval;
}

mongoc_uri_t *
mongoc_uri_new_with_error (const char *uri_string, bson_error_t *error)
{
   mongoc_uri_t *uri;
   int32_t max_staleness_seconds;

   uri = BSON_ALIGNED_ALLOC0 (mongoc_uri_t);
   bson_init (&uri->raw);
   bson_init (&uri->options);
   bson_init (&uri->credentials);
   bson_init (&uri->compressors);

   /* Initialize read_prefs, since parsing may add to it */
   uri->read_prefs = mongoc_read_prefs_new (MONGOC_READ_PRIMARY);

   /* Initialize empty read_concern */
   uri->read_concern = mongoc_read_concern_new ();

   if (!uri_string) {
      uri_string = "mongodb://127.0.0.1/";
   }

   if (!mongoc_uri_parse (uri, uri_string, error)) {
      mongoc_uri_destroy (uri);
      return NULL;
   }

   uri->str = bson_strdup (uri_string);

   if (!_mongoc_uri_assign_read_prefs_mode (uri, error)) {
      mongoc_uri_destroy (uri);
      return NULL;
   }
   max_staleness_seconds = _mongoc_uri_get_max_staleness_option (uri);
   mongoc_read_prefs_set_max_staleness_seconds (uri->read_prefs, max_staleness_seconds);

   if (!mongoc_read_prefs_is_valid (uri->read_prefs)) {
      mongoc_uri_destroy (uri);
      MONGOC_URI_ERROR (error, "%s", "Invalid readPreferences");
      return NULL;
   }

   if (!_mongoc_uri_build_write_concern (uri, error)) {
      mongoc_uri_destroy (uri);
      return NULL;
   }

   if (!mongoc_write_concern_is_valid (uri->write_concern)) {
      mongoc_uri_destroy (uri);
      MONGOC_URI_ERROR (error, "%s", "Invalid writeConcern");
      return NULL;
   }

   return uri;
}

mongoc_uri_t *
mongoc_uri_new (const char *uri_string)
{
   bson_error_t error = {0};
   mongoc_uri_t *uri;

   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (error.domain) {
      MONGOC_WARNING ("Error parsing URI: '%s'", error.message);
   }

   return uri;
}


mongoc_uri_t *
mongoc_uri_new_for_host_port (const char *hostname, uint16_t port)
{
   mongoc_uri_t *uri;
   char *str;

   BSON_ASSERT (hostname);
   BSON_ASSERT (port);

   str = bson_strdup_printf ("mongodb://%s:%hu/", hostname, port);
   uri = mongoc_uri_new (str);
   bson_free (str);

   return uri;
}


const char *
mongoc_uri_get_username (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);

   return uri->username;
}

bool
mongoc_uri_set_username (mongoc_uri_t *uri, const char *username)
{
   size_t len;

   BSON_ASSERT (username);

   len = strlen (username);

   if (!bson_utf8_validate (username, len, false)) {
      return false;
   }

   if (uri->username) {
      bson_free (uri->username);
   }

   uri->username = bson_strdup (username);
   return true;
}


const char *
mongoc_uri_get_password (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);

   return uri->password;
}

bool
mongoc_uri_set_password (mongoc_uri_t *uri, const char *password)
{
   size_t len;

   BSON_ASSERT (password);

   len = strlen (password);

   if (!bson_utf8_validate (password, len, false)) {
      return false;
   }

   if (uri->password) {
      bson_free (uri->password);
   }

   uri->password = bson_strdup (password);
   return true;
}


const char *
mongoc_uri_get_database (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return uri->database;
}

bool
mongoc_uri_set_database (mongoc_uri_t *uri, const char *database)
{
   size_t len;

   BSON_ASSERT (database);

   len = strlen (database);

   if (!bson_utf8_validate (database, len, false)) {
      return false;
   }

   if (uri->database) {
      bson_free (uri->database);
   }

   uri->database = bson_strdup (database);
   return true;
}


const char *
mongoc_uri_get_auth_source (const mongoc_uri_t *uri)
{
   BSON_ASSERT_PARAM (uri);

   // Explicitly set.
   {
      bson_iter_t iter;
      if (bson_iter_init_find_case (&iter, &uri->credentials, MONGOC_URI_AUTHSOURCE)) {
         return bson_iter_utf8 (&iter, NULL);
      }
   }

   // The database name if supplied.
   const char *const db = uri->database;

   // Depending on the authentication mechanism, `MongoCredential.source` has different defaults.
   const char *const mechanism = mongoc_uri_get_auth_mechanism (uri);

   // Default authentication mechanism uses either SCRAM-SHA-1 or SCRAM-SHA-256.
   if (!mechanism) {
      return db ? db : "admin";
   }

   // Defaults to the database name if supplied on the connection string or "admin" for:
   {
      static const char *const matches[] = {
         "SCRAM-SHA-1",
         "SCRAM-SHA-256",
         NULL,
      };

      for (const char *const *match = matches; *match; ++match) {
         if (strcasecmp (mechanism, *match) == 0) {
            return db ? db : "admin";
         }
      }
   }

   // Defaults to the database name if supplied on the connection string or "$external" for:
   //  - PLAIN
   if (strcasecmp (mechanism, "PLAIN") == 0) {
      return db ? db : "$external";
   }

   // Fallback to "$external" for all remaining authentication mechanisms:
   //  - MONGODB-X509
   //  - GSSAPI
   //  - MONGODB-AWS
   return "$external";
}


bool
mongoc_uri_set_auth_source (mongoc_uri_t *uri, const char *value)
{
   size_t len;

   BSON_ASSERT (value);

   len = strlen (value);

   if (!bson_utf8_validate (value, len, false)) {
      return false;
   }

   mongoc_uri_bson_append_or_replace_key (&uri->credentials, MONGOC_URI_AUTHSOURCE, value);

   return true;
}


const char *
mongoc_uri_get_appname (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);

   return mongoc_uri_get_option_as_utf8 (uri, MONGOC_URI_APPNAME, NULL);
}


bool
mongoc_uri_set_appname (mongoc_uri_t *uri, const char *value)
{
   BSON_ASSERT (value);

   if (!bson_utf8_validate (value, strlen (value), false)) {
      return false;
   }

   if (!_mongoc_handshake_appname_is_valid (value)) {
      return false;
   }

   mongoc_uri_bson_append_or_replace_key (&uri->options, MONGOC_URI_APPNAME, value);

   return true;
}

bool
mongoc_uri_set_compressors (mongoc_uri_t *uri, const char *value)
{
   const char *end_compressor;
   char *entry;

   bson_destroy (&uri->compressors);
   bson_init (&uri->compressors);

   if (value && !bson_utf8_validate (value, strlen (value), false)) {
      return false;
   }
   while ((entry = scan_to_unichar (value, ',', "", &end_compressor))) {
      if (mongoc_compressor_supported (entry)) {
         mongoc_uri_bson_append_or_replace_key (&uri->compressors, entry, "yes");
      } else {
         MONGOC_WARNING ("Unsupported compressor: '%s'", entry);
      }
      value = end_compressor + 1;
      bson_free (entry);
   }
   if (value) {
      if (mongoc_compressor_supported (value)) {
         mongoc_uri_bson_append_or_replace_key (&uri->compressors, value, "yes");
      } else {
         MONGOC_WARNING ("Unsupported compressor: '%s'", value);
      }
   }

   return true;
}

const bson_t *
mongoc_uri_get_compressors (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return &uri->compressors;
}


/* can't use mongoc_uri_get_option_as_int32, it treats 0 specially */
int32_t
mongoc_uri_get_local_threshold_option (const mongoc_uri_t *uri)
{
   const bson_t *options;
   bson_iter_t iter;
   int32_t retval = MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS;

   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, "localthresholdms") &&
       BSON_ITER_HOLDS_INT32 (&iter)) {
      retval = bson_iter_int32 (&iter);

      if (retval < 0) {
         MONGOC_WARNING ("Invalid localThresholdMS: %d", retval);
         retval = MONGOC_TOPOLOGY_LOCAL_THRESHOLD_MS;
      }
   }

   return retval;
}


const char *
mongoc_uri_get_srv_hostname (const mongoc_uri_t *uri)
{
   if (uri->is_srv) {
      return uri->srv;
   }

   return NULL;
}


/* Initial DNS Seedlist Discovery Spec: `srvServiceName` requires a string value
 * and defaults to "mongodb". */
static const char *const mongoc_default_srv_service_name = "mongodb";


const char *
mongoc_uri_get_srv_service_name (const mongoc_uri_t *uri)
{
   bson_iter_t iter;

   BSON_ASSERT_PARAM (uri);

   if (bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_SRVSERVICENAME)) {
      BSON_ASSERT (BSON_ITER_HOLDS_UTF8 (&iter));
      return bson_iter_utf8 (&iter, NULL);
   }

   return mongoc_default_srv_service_name;
}


const bson_t *
mongoc_uri_get_options (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return &uri->options;
}


void
mongoc_uri_destroy (mongoc_uri_t *uri)
{
   if (uri) {
      _mongoc_host_list_destroy_all (uri->hosts);
      bson_free (uri->str);
      bson_free (uri->database);
      bson_free (uri->username);
      bson_destroy (&uri->raw);
      bson_destroy (&uri->options);
      bson_destroy (&uri->credentials);
      bson_destroy (&uri->compressors);
      mongoc_read_prefs_destroy (uri->read_prefs);
      mongoc_read_concern_destroy (uri->read_concern);
      mongoc_write_concern_destroy (uri->write_concern);

      if (uri->password) {
         bson_zero_free (uri->password, strlen (uri->password));
      }

      bson_free (uri);
   }
}


mongoc_uri_t *
mongoc_uri_copy (const mongoc_uri_t *uri)
{
   mongoc_uri_t *copy;
   mongoc_host_list_t *iter;
   bson_error_t error;

   BSON_ASSERT (uri);

   copy = BSON_ALIGNED_ALLOC0 (mongoc_uri_t);

   copy->str = bson_strdup (uri->str);
   copy->is_srv = uri->is_srv;
   bson_strncpy (copy->srv, uri->srv, sizeof uri->srv);
   copy->username = bson_strdup (uri->username);
   copy->password = bson_strdup (uri->password);
   copy->database = bson_strdup (uri->database);

   copy->read_prefs = mongoc_read_prefs_copy (uri->read_prefs);
   copy->read_concern = mongoc_read_concern_copy (uri->read_concern);
   copy->write_concern = mongoc_write_concern_copy (uri->write_concern);

   LL_FOREACH (uri->hosts, iter)
   {
      if (!mongoc_uri_upsert_host (copy, iter->host, iter->port, &error)) {
         MONGOC_ERROR ("%s", error.message);
         mongoc_uri_destroy (copy);
         return NULL;
      }
   }

   bson_copy_to (&uri->raw, &copy->raw);
   bson_copy_to (&uri->options, &copy->options);
   bson_copy_to (&uri->credentials, &copy->credentials);
   bson_copy_to (&uri->compressors, &copy->compressors);

   return copy;
}


const char *
mongoc_uri_get_string (const mongoc_uri_t *uri)
{
   BSON_ASSERT (uri);
   return uri->str;
}


char *
mongoc_uri_unescape (const char *escaped_string)
{
   bson_unichar_t c;
   unsigned int hex = 0;
   const char *ptr;
   const char *end;
   size_t len;
   bool unescape_occurred = false;

   BSON_ASSERT (escaped_string);

   len = strlen (escaped_string);

   /*
    * Double check that this is a UTF-8 valid string. Bail out if necessary.
    */
   if (!bson_utf8_validate (escaped_string, len, false)) {
      MONGOC_WARNING ("%s(): escaped_string contains invalid UTF-8", BSON_FUNC);
      return NULL;
   }

   ptr = escaped_string;
   end = ptr + len;

   mcommon_string_append_t append;
   mcommon_string_new_with_capacity_as_append (&append, len);

   for (; *ptr; ptr = bson_utf8_next_char (ptr)) {
      c = bson_utf8_get_char (ptr);
      switch (c) {
      case '%':
         if (((end - ptr) < 2) || !isxdigit (ptr[1]) || !isxdigit (ptr[2]) ||
#ifdef _MSC_VER
             (1 != sscanf_s (&ptr[1], "%02x", &hex))
#else
             (1 != sscanf (&ptr[1], "%02x", &hex))
#endif
             || 0 == hex) {
            mcommon_string_from_append_destroy (&append);
            MONGOC_WARNING ("Invalid %% escape sequence");
            return NULL;
         }

         // This isn't guaranteed to be valid UTF-8, we check again below
         char byte = (char) hex;
         mcommon_string_append_bytes (&append, &byte, 1);
         ptr += 2;
         unescape_occurred = true;
         break;
      default:
         mcommon_string_append_unichar (&append, c);
         break;
      }
   }

   /* Check that after unescaping, it is still valid UTF-8 */
   if (unescape_occurred &&
       !bson_utf8_validate (mcommon_str_from_append (&append), mcommon_strlen_from_append (&append), false)) {
      MONGOC_WARNING ("Invalid %% escape sequence: unescaped string contains invalid UTF-8");
      mcommon_string_from_append_destroy (&append);
      return NULL;
   }

   return mcommon_string_from_append_destroy_with_steal (&append);
}


const mongoc_read_prefs_t *
mongoc_uri_get_read_prefs_t (const mongoc_uri_t *uri) /* IN */
{
   BSON_ASSERT (uri);

   return uri->read_prefs;
}


void
mongoc_uri_set_read_prefs_t (mongoc_uri_t *uri, const mongoc_read_prefs_t *prefs)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (prefs);

   mongoc_read_prefs_destroy (uri->read_prefs);
   uri->read_prefs = mongoc_read_prefs_copy (prefs);
}


const mongoc_read_concern_t *
mongoc_uri_get_read_concern (const mongoc_uri_t *uri) /* IN */
{
   BSON_ASSERT (uri);

   return uri->read_concern;
}


void
mongoc_uri_set_read_concern (mongoc_uri_t *uri, const mongoc_read_concern_t *rc)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (rc);

   mongoc_read_concern_destroy (uri->read_concern);
   uri->read_concern = mongoc_read_concern_copy (rc);
}


const mongoc_write_concern_t *
mongoc_uri_get_write_concern (const mongoc_uri_t *uri) /* IN */
{
   BSON_ASSERT (uri);

   return uri->write_concern;
}


void
mongoc_uri_set_write_concern (mongoc_uri_t *uri, const mongoc_write_concern_t *wc)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (wc);

   mongoc_write_concern_destroy (uri->write_concern);
   uri->write_concern = mongoc_write_concern_copy (wc);
}


bool
mongoc_uri_get_tls (const mongoc_uri_t *uri) /* IN */
{
   bson_iter_t iter;

   BSON_ASSERT (uri);

   if (bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLS) && BSON_ITER_HOLDS_BOOL (&iter)) {
      return bson_iter_bool (&iter);
   }

   if (bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSCERTIFICATEKEYFILE) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSCAFILE) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSALLOWINVALIDCERTIFICATES) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSALLOWINVALIDHOSTNAMES) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSINSECURE) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSCERTIFICATEKEYFILEPASSWORD) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSDISABLEOCSPENDPOINTCHECK) ||
       bson_iter_init_find_case (&iter, &uri->options, MONGOC_URI_TLSDISABLECERTIFICATEREVOCATIONCHECK)) {
      return true;
   }

   return false;
}


const char *
mongoc_uri_get_server_monitoring_mode (const mongoc_uri_t *uri)
{
   BSON_ASSERT_PARAM (uri);

   return mongoc_uri_get_option_as_utf8 (uri, MONGOC_URI_SERVERMONITORINGMODE, "auto");
}


bool
mongoc_uri_set_server_monitoring_mode (mongoc_uri_t *uri, const char *value)
{
   BSON_ASSERT_PARAM (uri);
   BSON_ASSERT_PARAM (value);

   // Check for valid value
   if (strcmp (value, "stream") && strcmp (value, "poll") && strcmp (value, "auto")) {
      return false;
   }

   mongoc_uri_bson_append_or_replace_key (&uri->options, MONGOC_URI_SERVERMONITORINGMODE, value);
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_int32 --
 *
 *       Checks if the URI 'option' is set and of correct type (int32).
 *       The special value '0' is considered as "unset".
 *       This is so users can provide
 *       sprintf("mongodb://localhost/?option=%d", myvalue) style connection
 *       strings, and still apply default values.
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as int32 (and not 0), or
 *       'fallback'.
 *
 *--------------------------------------------------------------------------
 */

int32_t
mongoc_uri_get_option_as_int32 (const mongoc_uri_t *uri, const char *option_orig, int32_t fallback)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;
   int64_t retval = 0;

   option = mongoc_uri_canonicalize_option (option_orig);

   /* BC layer to allow retrieving 32-bit values stored in 64-bit options */
   if (mongoc_uri_option_is_int64 (option_orig)) {
      retval = mongoc_uri_get_option_as_int64 (uri, option_orig, 0);

      if (retval > INT32_MAX || retval < INT32_MIN) {
         MONGOC_WARNING ("Cannot read 64-bit value for \"%s\": %" PRId64, option_orig, retval);

         retval = 0;
      }
   } else if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option) &&
              BSON_ITER_HOLDS_INT32 (&iter)) {
      retval = bson_iter_int32 (&iter);
   }

   if (!retval) {
      retval = fallback;
   }

   return (int32_t) retval;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_int32 --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_int32 ().
 *
 *       Does in-place-update of the option BSON if 'option' is already set.
 *       Appends the option to the end otherwise.
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_int32 (mongoc_uri_t *uri, const char *option_orig, int32_t value)
{
   const char *option;
   bson_error_t error;
   bool r;

   if (mongoc_uri_option_is_int64 (option_orig)) {
      return mongoc_uri_set_option_as_int64 (uri, option_orig, value);
   }

   option = mongoc_uri_canonicalize_option (option_orig);

   if (!mongoc_uri_option_is_int32 (option)) {
      MONGOC_WARNING ("Unsupported value for \"%s\": %d, \"%s\" is not an int32 option", option_orig, value, option);
      return false;
   }

   r = _mongoc_uri_set_option_as_int32_with_error (uri, option, value, &error);
   if (!r) {
      MONGOC_WARNING ("%s", error.message);
   }

   return r;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_uri_set_option_as_int32_with_error --
 *
 *       Same as mongoc_uri_set_option_as_int32, with error reporting.
 *
 * Precondition:
 *       mongoc_uri_option_is_int32(option) must be true.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_uri_set_option_as_int32_with_error (mongoc_uri_t *uri,
                                            const char *option_orig,
                                            int32_t value,
                                            bson_error_t *error)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;
   char *option_lowercase = NULL;

   option = mongoc_uri_canonicalize_option (option_orig);
   /* Server Discovery and Monitoring Spec: "the driver MUST NOT permit users
    * to configure it less than minHeartbeatFrequencyMS (500ms)." */
   if (!bson_strcasecmp (option, MONGOC_URI_HEARTBEATFREQUENCYMS) &&
       value < MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS) {
      MONGOC_URI_ERROR (error,
                        "Invalid \"%s\" of %d: must be at least %d",
                        option_orig,
                        value,
                        MONGOC_TOPOLOGY_MIN_HEARTBEAT_FREQUENCY_MS);
      return false;
   }

   /* zlib levels are from -1 (default) through 9 (best compression) */
   if (!bson_strcasecmp (option, MONGOC_URI_ZLIBCOMPRESSIONLEVEL) && (value < -1 || value > 9)) {
      MONGOC_URI_ERROR (error, "Invalid \"%s\" of %d: must be between -1 and 9", option_orig, value);
      return false;
   }

   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_INT32 (&iter)) {
         bson_iter_overwrite_int32 (&iter, value);
         return true;
      } else {
         MONGOC_URI_ERROR (error,
                           "Cannot set URI option \"%s\" to %d, it already has "
                           "a non-32-bit integer value",
                           option,
                           value);
         return false;
      }
   }
   option_lowercase = lowercase_str_new (option);
   if (!bson_append_int32 (&uri->options, option_lowercase, -1, value)) {
      bson_free (option_lowercase);
      MONGOC_URI_ERROR (error, "Failed to set URI option \"%s\" to %d", option_orig, value);

      return false;
   }

   bson_free (option_lowercase);
   return true;
}


/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_uri_set_option_as_int32 --
 *
 *       Same as mongoc_uri_set_option_as_int32, except the option is not
 *       validated against valid int32 options
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_uri_set_option_as_int32 (mongoc_uri_t *uri, const char *option_orig, int32_t value)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;
   char *option_lowercase = NULL;

   option = mongoc_uri_canonicalize_option (option_orig);
   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_INT32 (&iter)) {
         bson_iter_overwrite_int32 (&iter, value);
         return true;
      } else {
         return false;
      }
   }

   option_lowercase = lowercase_str_new (option);
   bson_append_int32 (&uri->options, option_lowercase, -1, value);
   bson_free (option_lowercase);
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_int64 --
 *
 *       Checks if the URI 'option' is set and of correct type (int32 or
 *       int64).
 *       The special value '0' is considered as "unset".
 *       This is so users can provide
 *       sprintf("mongodb://localhost/?option=%" PRId64, myvalue) style
 *       connection strings, and still apply default values.
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as int64 or int32 (and not 0), or
 *       'fallback'.
 *
 *--------------------------------------------------------------------------
 */

int64_t
mongoc_uri_get_option_as_int64 (const mongoc_uri_t *uri, const char *option_orig, int64_t fallback)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;
   int64_t retval = fallback;

   option = mongoc_uri_canonicalize_option (option_orig);
   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_INT (&iter)) {
         if (!(retval = bson_iter_as_int64 (&iter))) {
            retval = fallback;
         }
      }
   }

   return retval;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_int64 --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_int64 ().
 *
 *       Does in-place-update of the option BSON if 'option' is already set.
 *       Appends the option to the end otherwise.
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_int64 (mongoc_uri_t *uri, const char *option_orig, int64_t value)
{
   const char *option;
   bson_error_t error;
   bool r;

   option = mongoc_uri_canonicalize_option (option_orig);
   if (!mongoc_uri_option_is_int64 (option)) {
      if (mongoc_uri_option_is_int32 (option_orig)) {
         if (value >= INT32_MIN && value <= INT32_MAX) {
            MONGOC_WARNING ("Setting value for 32-bit option \"%s\" through 64-bit method", option_orig);

            return mongoc_uri_set_option_as_int32 (uri, option_orig, (int32_t) value);
         }

         MONGOC_WARNING (
            "Unsupported value for \"%s\": %" PRId64 ", \"%s\" is not an int64 option", option_orig, value, option);
         return false;
      }
   }

   r = _mongoc_uri_set_option_as_int64_with_error (uri, option, value, &error);
   if (!r) {
      MONGOC_WARNING ("%s", error.message);
   }

   return r;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_uri_set_option_as_int64_with_error --
 *
 *       Same as mongoc_uri_set_option_as_int64, with error reporting.
 *
 * Precondition:
 *       mongoc_uri_option_is_int64(option) must be true.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

static bool
_mongoc_uri_set_option_as_int64_with_error (mongoc_uri_t *uri,
                                            const char *option_orig,
                                            int64_t value,
                                            bson_error_t *error)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;
   char *option_lowercase = NULL;

   option = mongoc_uri_canonicalize_option (option_orig);

   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_INT64 (&iter)) {
         bson_iter_overwrite_int64 (&iter, value);
         return true;
      } else {
         MONGOC_URI_ERROR (error,
                           "Cannot set URI option \"%s\" to %" PRId64 ", it already has "
                           "a non-64-bit integer value",
                           option,
                           value);
         return false;
      }
   }

   option_lowercase = lowercase_str_new (option);
   if (!bson_append_int64 (&uri->options, option_lowercase, -1, value)) {
      bson_free (option_lowercase);
      MONGOC_URI_ERROR (error, "Failed to set URI option \"%s\" to %" PRId64, option_orig, value);

      return false;
   }
   bson_free (option_lowercase);
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_bool --
 *
 *       Checks if the URI 'option' is set and of correct type (bool).
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as bool, or 'fallback'.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_get_option_as_bool (const mongoc_uri_t *uri, const char *option_orig, bool fallback)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;

   option = mongoc_uri_canonicalize_option (option_orig);
   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option) &&
       BSON_ITER_HOLDS_BOOL (&iter)) {
      return bson_iter_bool (&iter);
   }

   return fallback;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_bool --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_bool ().
 *
 *       Does in-place-update of the option BSON if 'option' is already set.
 *       Appends the option to the end otherwise.
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_bool (mongoc_uri_t *uri, const char *option_orig, bool value)
{
   const char *option;
   char *option_lowercase;
   const bson_t *options;
   bson_iter_t iter;

   option = mongoc_uri_canonicalize_option (option_orig);
   BSON_ASSERT (option);

   if (!mongoc_uri_option_is_bool (option)) {
      return false;
   }

   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option)) {
      if (BSON_ITER_HOLDS_BOOL (&iter)) {
         bson_iter_overwrite_bool (&iter, value);
         return true;
      } else {
         return false;
      }
   }
   option_lowercase = lowercase_str_new (option);
   bson_append_bool (&uri->options, option_lowercase, -1, value);
   bson_free (option_lowercase);
   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_get_option_as_utf8 --
 *
 *       Checks if the URI 'option' is set and of correct type (utf8).
 *
 *       If not set, or set to invalid type, 'fallback' is returned.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       The value of 'option' if available as utf8, or 'fallback'.
 *
 *--------------------------------------------------------------------------
 */

const char *
mongoc_uri_get_option_as_utf8 (const mongoc_uri_t *uri, const char *option_orig, const char *fallback)
{
   const char *option;
   const bson_t *options;
   bson_iter_t iter;

   option = mongoc_uri_canonicalize_option (option_orig);
   if ((options = mongoc_uri_get_options (uri)) && bson_iter_init_find_case (&iter, options, option) &&
       BSON_ITER_HOLDS_UTF8 (&iter)) {
      return bson_iter_utf8 (&iter, NULL);
   }

   return fallback;
}

/*
 *--------------------------------------------------------------------------
 *
 * mongoc_uri_set_option_as_utf8 --
 *
 *       Sets a URI option 'after the fact'. Allows users to set individual
 *       URI options without passing them as a connection string.
 *
 *       Only allows a set of known options to be set.
 *       @see mongoc_uri_option_is_utf8 ().
 *
 *       If the option is not already set, this function will append it to
 *the end of the options bson. NOTE: If the option is already set the entire
 *options bson will be overwritten, containing the new option=value
 *(at the same position).
 *
 *       NOTE: If 'option' is already set, and is of invalid type, this
 *       function will return false.
 *
 *       NOTE: 'option' must be valid utf8.
 *
 *       NOTE: 'option' is case*in*sensitive.
 *
 * Returns:
 *       true on successfully setting the option, false on failure.
 *
 *--------------------------------------------------------------------------
 */

bool
mongoc_uri_set_option_as_utf8 (mongoc_uri_t *uri, const char *option_orig, const char *value)
{
   const char *option;
   size_t len;
   char *option_lowercase = NULL;

   option = mongoc_uri_canonicalize_option (option_orig);
   BSON_ASSERT (option);

   len = strlen (value);

   if (!bson_utf8_validate (value, len, false)) {
      return false;
   }

   if (!mongoc_uri_option_is_utf8 (option)) {
      return false;
   }
   if (!bson_strcasecmp (option, MONGOC_URI_APPNAME)) {
      return mongoc_uri_set_appname (uri, value);
   } else if (!bson_strcasecmp (option, MONGOC_URI_SERVERMONITORINGMODE)) {
      return mongoc_uri_set_server_monitoring_mode (uri, value);
   } else {
      option_lowercase = lowercase_str_new (option);
      mongoc_uri_bson_append_or_replace_key (&uri->options, option_lowercase, value);
      bson_free (option_lowercase);
   }

   return true;
}

/*
 *--------------------------------------------------------------------------
 *
 * _mongoc_uri_requires_auth_negotiation --
 *
 *       Returns true if auth mechanism is necessary for this uri. According
 *       to the auth spec: "If an application provides a username but does
 *       not provide an authentication mechanism, drivers MUST negotiate a
 *       mechanism".
 *
 * Returns:
 *       true if the driver should negotiate the auth mechanism for the uri
 *
 *--------------------------------------------------------------------------
 */
bool
_mongoc_uri_requires_auth_negotiation (const mongoc_uri_t *uri)
{
   return mongoc_uri_get_username (uri) && !mongoc_uri_get_auth_mechanism (uri);
}


/* A bit of a hack. Needed for multi mongos tests to create a URI with the same
 * auth, SSL, and compressors settings but with only one specific host. */
mongoc_uri_t *
_mongoc_uri_copy_and_replace_host_list (const mongoc_uri_t *original, const char *host)
{
   mongoc_uri_t *uri = mongoc_uri_copy (original);
   _mongoc_host_list_destroy_all (uri->hosts);
   uri->hosts = bson_malloc0 (sizeof (mongoc_host_list_t));
   _mongoc_host_list_from_string (uri->hosts, host);
   return uri;
}

bool
mongoc_uri_init_with_srv_host_list (mongoc_uri_t *uri, mongoc_host_list_t *host_list, bson_error_t *error)
{
   mongoc_host_list_t *host;

   BSON_ASSERT (uri->is_srv);
   BSON_ASSERT (!uri->hosts);

   LL_FOREACH (host_list, host)
   {
      if (!mongoc_uri_upsert_host_and_port (uri, host->host_and_port, error)) {
         return false;
      }
   }

   return true;
}

#ifdef MONGOC_ENABLE_CRYPTO
void
_mongoc_uri_init_scram (const mongoc_uri_t *uri, mongoc_scram_t *scram, mongoc_crypto_hash_algorithm_t algo)
{
   BSON_ASSERT (uri);
   BSON_ASSERT (scram);

   _mongoc_scram_init (scram, algo);

   _mongoc_scram_set_pass (scram, mongoc_uri_get_password (uri));
   _mongoc_scram_set_user (scram, mongoc_uri_get_username (uri));
}
#endif

static bool
mongoc_uri_finalize_loadbalanced (const mongoc_uri_t *uri, bson_error_t *error)
{
   if (!mongoc_uri_get_option_as_bool (uri, MONGOC_URI_LOADBALANCED, false)) {
      return true;
   }

   /* Load Balancer Spec: When `loadBalanced=true` is provided in the connection
    * string, the driver MUST throw an exception if the connection string
    * contains more than one host/port. */
   if (uri->hosts && uri->hosts->next) {
      MONGOC_URI_ERROR (error, "URI with \"%s\" enabled must not contain more than one host", MONGOC_URI_LOADBALANCED);
      return false;
   }

   if (mongoc_uri_has_option (uri, MONGOC_URI_REPLICASET)) {
      MONGOC_URI_ERROR (error,
                        "URI with \"%s\" enabled must not contain option \"%s\"",
                        MONGOC_URI_LOADBALANCED,
                        MONGOC_URI_REPLICASET);
      return false;
   }

   if (mongoc_uri_has_option (uri, MONGOC_URI_DIRECTCONNECTION) &&
       mongoc_uri_get_option_as_bool (uri, MONGOC_URI_DIRECTCONNECTION, false)) {
      MONGOC_URI_ERROR (error,
                        "URI with \"%s\" enabled must not contain option \"%s\" enabled",
                        MONGOC_URI_LOADBALANCED,
                        MONGOC_URI_DIRECTCONNECTION);
      return false;
   }

   return true;
}

static bool
mongoc_uri_finalize_srv (const mongoc_uri_t *uri, bson_error_t *error)
{
   /* Initial DNS Seedlist Discovery Spec: The driver MUST report an error if
    * either the `srvServiceName` or `srvMaxHosts` URI options are specified
    * with a non-SRV URI. */
   if (!uri->is_srv) {
      const char *option = NULL;

      if (mongoc_uri_has_option (uri, MONGOC_URI_SRVSERVICENAME)) {
         option = MONGOC_URI_SRVSERVICENAME;
      } else if (mongoc_uri_has_option (uri, MONGOC_URI_SRVMAXHOSTS)) {
         option = MONGOC_URI_SRVMAXHOSTS;
      }

      if (option) {
         MONGOC_URI_ERROR (error, "%s must not be specified with a non-SRV URI", option);
         return false;
      }
   }

   if (uri->is_srv) {
      const int32_t max_hosts = mongoc_uri_get_option_as_int32 (uri, MONGOC_URI_SRVMAXHOSTS, 0);

      /* Initial DNS Seedless Discovery Spec: This option requires a
       * non-negative integer and defaults to zero (i.e. no limit). */
      if (max_hosts < 0) {
         MONGOC_URI_ERROR (error,
                           "%s is required to be a non-negative integer, but "
                           "has value %" PRId32,
                           MONGOC_URI_SRVMAXHOSTS,
                           max_hosts);
         return false;
      }

      if (max_hosts > 0) {
         /* Initial DNS Seedless Discovery spec: If srvMaxHosts is a positive
          * integer, the driver MUST throw an error if the connection string
          * contains a `replicaSet` option. */
         if (mongoc_uri_has_option (uri, MONGOC_URI_REPLICASET)) {
            MONGOC_URI_ERROR (error, "%s must not be specified with %s", MONGOC_URI_SRVMAXHOSTS, MONGOC_URI_REPLICASET);
            return false;
         }

         /* Initial DNS Seedless Discovery Spec: If srvMaxHosts is a positive
          * integer, the driver MUST throw an error if the connection string
          * contains a `loadBalanced` option with a value of `true`.
          */
         if (mongoc_uri_get_option_as_bool (uri, MONGOC_URI_LOADBALANCED, false)) {
            MONGOC_URI_ERROR (
               error, "%s must not be specified with %s=true", MONGOC_URI_SRVMAXHOSTS, MONGOC_URI_LOADBALANCED);
            return false;
         }
      }
   }

   return true;
}

/* This should be called whenever URI options change (e.g. parsing a new URI
 * string, after setting one or more options explicitly, applying TXT records).
 * While the primary purpose of this function is to validate the URI, it may
 * also alter the URI (e.g. implicitly enable TLS when SRV is used). Returns
 * true on success; otherwise, returns false and sets @error. */
bool
mongoc_uri_finalize (mongoc_uri_t *uri, bson_error_t *error)
{
   BSON_ASSERT_PARAM (uri);

   if (!mongoc_uri_finalize_tls (uri, error)) {
      return false;
   }

   if (!mongoc_uri_finalize_auth (uri, error)) {
      return false;
   }

   if (!mongoc_uri_finalize_directconnection (uri, error)) {
      return false;
   }

   if (!mongoc_uri_finalize_loadbalanced (uri, error)) {
      return false;
   }

   if (!mongoc_uri_finalize_srv (uri, error)) {
      return false;
   }

   return true;
}
