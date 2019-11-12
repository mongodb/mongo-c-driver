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

#include "mongoc-config.h"

#ifdef MONGOC_ENABLE_SSL_OPENSSL

#include <bson/bson.h>
#include <limits.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/ocsp.h>
#include <openssl/x509v3.h>
#include <openssl/crypto.h>

#include <string.h>

#include "mongoc-init.h"
#include "mongoc-openssl-private.h"
#include "mongoc-socket.h"
#include "mongoc-ssl.h"
#include "mongoc-stream-tls-openssl-private.h"
#include "mongoc-thread-private.h"
#include "mongoc-trace-private.h"
#include "mongoc-util-private.h"

#ifdef _WIN32
#include <wincrypt.h>
#endif

#if OPENSSL_VERSION_NUMBER < 0x10100000L
static bson_mutex_t *gMongocOpenSslThreadLocks;

static void
_mongoc_openssl_thread_startup (void);
static void
_mongoc_openssl_thread_cleanup (void);
#endif
#ifndef MONGOC_HAVE_ASN1_STRING_GET0_DATA
#define ASN1_STRING_get0_data ASN1_STRING_data
#endif

/**
 * _mongoc_openssl_init:
 *
 * initialization function for SSL
 *
 * This needs to get called early on and is not threadsafe.  Called by
 * mongoc_init.
 */
void
_mongoc_openssl_init (void)
{
   SSL_CTX *ctx;

   SSL_library_init ();
   SSL_load_error_strings ();
   ERR_load_BIO_strings ();
   OpenSSL_add_all_algorithms ();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
   _mongoc_openssl_thread_startup ();
#endif

   ctx = SSL_CTX_new (SSLv23_method ());
   if (!ctx) {
      MONGOC_ERROR ("Failed to initialize OpenSSL.");
   }

   SSL_CTX_free (ctx);
}

void
_mongoc_openssl_cleanup (void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
   _mongoc_openssl_thread_cleanup ();
#endif
}

static int
_mongoc_openssl_password_cb (char *buf, int num, int rwflag, void *user_data)
{
   char *pass = (char *) user_data;
   int pass_len = (int) strlen (pass);

   if (num < pass_len + 1) {
      return 0;
   }

   bson_strncpy (buf, pass, num);
   return pass_len;
}

#ifdef _WIN32
bool
_mongoc_openssl_import_cert_store (LPWSTR store_name,
                                   DWORD dwFlags,
                                   X509_STORE *openssl_store)
{
   PCCERT_CONTEXT cert = NULL;
   HCERTSTORE cert_store;

   cert_store = CertOpenStore (
      CERT_STORE_PROV_SYSTEM,                  /* provider */
      X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, /* certificate encoding */
      0,                                       /* unused */
      dwFlags,                                 /* dwFlags */
      store_name); /* system store name. "My" or "Root" */

   if (cert_store == NULL) {
      LPTSTR msg = NULL;
      FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
                        FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_ARGUMENT_ARRAY,
                     NULL,
                     GetLastError (),
                     LANG_NEUTRAL,
                     (LPTSTR) &msg,
                     0,
                     NULL);
      MONGOC_ERROR ("Can't open CA store: 0x%.8X: '%s'", GetLastError (), msg);
      LocalFree (msg);
      return false;
   }

   while ((cert = CertEnumCertificatesInStore (cert_store, cert)) != NULL) {
      X509 *x509Obj = d2i_X509 (NULL,
                                (const unsigned char **) &cert->pbCertEncoded,
                                cert->cbCertEncoded);

      if (x509Obj == NULL) {
         MONGOC_WARNING (
            "Error parsing X509 object from Windows certificate store");
         continue;
      }

      X509_STORE_add_cert (openssl_store, x509Obj);
      X509_free (x509Obj);
   }

   CertCloseStore (cert_store, 0);
   return true;
}

bool
_mongoc_openssl_import_cert_stores (SSL_CTX *context)
{
   bool retval;
   X509_STORE *store = SSL_CTX_get_cert_store (context);

   if (!store) {
      MONGOC_WARNING ("no X509 store found for SSL context while loading "
                      "system certificates");
      return false;
   }

   retval = _mongoc_openssl_import_cert_store (L"root",
                                               CERT_SYSTEM_STORE_CURRENT_USER |
                                                  CERT_STORE_READONLY_FLAG,
                                               store);
   retval &= _mongoc_openssl_import_cert_store (
      L"CA", CERT_SYSTEM_STORE_CURRENT_USER | CERT_STORE_READONLY_FLAG, store);

   return retval;
}
#endif

/** mongoc_openssl_hostcheck
 *
 * rfc 6125 match a given hostname against a given pattern
 *
 * Patterns come from DNS common names or subjectAltNames.
 *
 * This code is meant to implement RFC 6125 6.4.[1-3]
 *
 */
static bool
_mongoc_openssl_hostcheck (const char *pattern, const char *hostname)
{
   const char *pattern_label_end;
   const char *pattern_wildcard;
   const char *hostname_label_end;
   size_t prefixlen;
   size_t suffixlen;

   TRACE ("Comparing '%s' == '%s'", pattern, hostname);
   pattern_wildcard = strchr (pattern, '*');

   if (pattern_wildcard == NULL) {
      return strcasecmp (pattern, hostname) == 0;
   }

   pattern_label_end = strchr (pattern, '.');

   /* Bail out on wildcarding in a couple of situations:
    * o we don't have 2 dots - we're not going to wildcard root tlds
    * o the wildcard isn't in the left most group (separated by dots)
    * o the pattern is embedded in an A-label or U-label
    */
   if (pattern_label_end == NULL ||
       strchr (pattern_label_end + 1, '.') == NULL ||
       pattern_wildcard > pattern_label_end ||
       strncasecmp (pattern, "xn--", 4) == 0) {
      return strcasecmp (pattern, hostname) == 0;
   }

   hostname_label_end = strchr (hostname, '.');

   /* we know we have a dot in the pattern, we need one in the hostname */
   if (hostname_label_end == NULL ||
       strcasecmp (pattern_label_end, hostname_label_end)) {
      return 0;
   }

   /* The wildcard must match at least one character, so the left part of the
    * hostname is at least as large as the left part of the pattern. */
   if ((hostname_label_end - hostname) < (pattern_label_end - pattern)) {
      return 0;
   }

   /* If the left prefix group before the star matches and right of the star
    * matches... we have a wildcard match */
   prefixlen = pattern_wildcard - pattern;
   suffixlen = pattern_label_end - (pattern_wildcard + 1);
   return strncasecmp (pattern, hostname, prefixlen) == 0 &&
          strncasecmp (pattern_wildcard + 1,
                       hostname_label_end - suffixlen,
                       suffixlen) == 0;
}


/** check if a provided cert matches a passed hostname
 */
bool
_mongoc_openssl_check_cert (SSL *ssl,
                            const char *host,
                            bool allow_invalid_hostname)
{
   X509 *peer;
   X509_NAME *subject_name;
   X509_NAME_ENTRY *entry;
   ASN1_STRING *entry_data;
   int length;
   int idx;
   int r = 0;
   long verify_status;

   size_t addrlen = 0;
   unsigned char addr4[sizeof (struct in_addr)];
   unsigned char addr6[sizeof (struct in6_addr)];
   int i;
   int n_sans = -1;
   int target = GEN_DNS;

   STACK_OF (GENERAL_NAME) *sans = NULL;

   ENTRY;
   BSON_ASSERT (ssl);
   BSON_ASSERT (host);

   if (allow_invalid_hostname) {
      RETURN (true);
   }

   /** if the host looks like an IP address, match that, otherwise we assume we
    * have a DNS name */
   if (inet_pton (AF_INET, host, &addr4)) {
      target = GEN_IPADD;
      addrlen = sizeof addr4;
   } else if (inet_pton (AF_INET6, host, &addr6)) {
      target = GEN_IPADD;
      addrlen = sizeof addr6;
   }

   peer = SSL_get_peer_certificate (ssl);

   if (!peer) {
      MONGOC_WARNING ("SSL Certification verification failed: %s",
                      ERR_error_string (ERR_get_error (), NULL));
      RETURN (false);
   }

   verify_status = SSL_get_verify_result (ssl);

   if (verify_status == X509_V_OK) {
      /* gets a stack of alt names that we can iterate through */
      sans = (STACK_OF (GENERAL_NAME) *) X509_get_ext_d2i (
         (X509 *) peer, NID_subject_alt_name, NULL, NULL);

      if (sans) {
         n_sans = sk_GENERAL_NAME_num (sans);

         /* loop through the stack, or until we find a match */
         for (i = 0; i < n_sans && !r; i++) {
            const GENERAL_NAME *name = sk_GENERAL_NAME_value (sans, i);

            /* skip entries that can't apply, I.e. IP entries if we've got a
             * DNS host */
            if (name->type == target) {
               const char *check;

               check = (const char *) ASN1_STRING_get0_data (name->d.ia5);
               length = ASN1_STRING_length (name->d.ia5);

               switch (target) {
               case GEN_DNS:

                  /* check that we don't have an embedded null byte */
                  if ((length == bson_strnlen (check, length)) &&
                      _mongoc_openssl_hostcheck (check, host)) {
                     r = 1;
                  }

                  break;
               case GEN_IPADD:
                  if (length == addrlen) {
                     if (length == sizeof addr6 &&
                         !memcmp (check, &addr6, length)) {
                        r = 1;
                     } else if (length == sizeof addr4 &&
                                !memcmp (check, &addr4, length)) {
                        r = 1;
                     }
                  }

                  break;
               default:
                  BSON_ASSERT (0);
                  break;
               }
            }
         }
         GENERAL_NAMES_free (sans);
      } else {
         subject_name = X509_get_subject_name (peer);

         if (subject_name) {
            i = -1;

            /* skip to the last common name */
            while ((idx = X509_NAME_get_index_by_NID (
                       subject_name, NID_commonName, i)) >= 0) {
               i = idx;
            }

            if (i >= 0) {
               entry = X509_NAME_get_entry (subject_name, i);
               entry_data = X509_NAME_ENTRY_get_data (entry);

               if (entry_data) {
                  char *check;

                  /* TODO: I've heard tell that old versions of SSL crap out
                   * when calling ASN1_STRING_to_UTF8 on already utf8 data.
                   * Check up on that */
                  length = ASN1_STRING_to_UTF8 ((unsigned char **) &check,
                                                entry_data);

                  if (length >= 0) {
                     /* check for embedded nulls */
                     if ((length == bson_strnlen (check, length)) &&
                         _mongoc_openssl_hostcheck (check, host)) {
                        r = 1;
                     }

                     OPENSSL_free (check);
                  }
               }
            }
         }
      }
   }

   X509_free (peer);
   RETURN (r);
}


static bool
_mongoc_openssl_setup_ca (SSL_CTX *ctx, const char *cert, const char *cert_dir)
{
   BSON_ASSERT (ctx);
   BSON_ASSERT (cert || cert_dir);

   if (!SSL_CTX_load_verify_locations (ctx, cert, cert_dir)) {
      MONGOC_ERROR ("Cannot load Certificate Authorities from '%s' and '%s'",
                    cert,
                    cert_dir);
      return 0;
   }

   return 1;
}


static bool
_mongoc_openssl_setup_crl (SSL_CTX *ctx, const char *crlfile)
{
   X509_STORE *store;
   X509_LOOKUP *lookup;
   int status;

   store = SSL_CTX_get_cert_store (ctx);
   X509_STORE_set_flags (store, X509_V_FLAG_CRL_CHECK);

   lookup = X509_STORE_add_lookup (store, X509_LOOKUP_file ());

   status = X509_load_crl_file (lookup, crlfile, X509_FILETYPE_PEM);

   return status != 0;
}


static bool
_mongoc_openssl_setup_pem_file (SSL_CTX *ctx,
                                const char *pem_file,
                                const char *password)
{
   if (!SSL_CTX_use_certificate_chain_file (ctx, pem_file)) {
      MONGOC_ERROR ("Cannot find certificate in '%s'", pem_file);
      return 0;
   }

   if (password) {
      SSL_CTX_set_default_passwd_cb_userdata (ctx, (void *) password);
      SSL_CTX_set_default_passwd_cb (ctx, _mongoc_openssl_password_cb);
   }

   if (!(SSL_CTX_use_PrivateKey_file (ctx, pem_file, SSL_FILETYPE_PEM))) {
      MONGOC_ERROR ("Cannot find private key in: '%s'", pem_file);
      return 0;
   }

   if (!(SSL_CTX_check_private_key (ctx))) {
      MONGOC_ERROR ("Cannot load private key: '%s'", pem_file);
      return 0;
   }

   return 1;
}

#ifdef MONGOC_ENABLE_OCSP

static X509 *
_get_issuer (X509 *cert, STACK_OF (X509) * chain)
{
   X509 *issuer = NULL, *candidate = NULL;
   X509_NAME *issuer_name = NULL, *candidate_name = NULL;
   int i;

   issuer_name = X509_get_issuer_name (cert);
   for (i = 0; i < sk_X509_num (chain) && issuer == NULL; i++) {
      candidate = sk_X509_value (chain, i);
      candidate_name = X509_get_subject_name (candidate);
      if (0 == X509_NAME_cmp (candidate_name, issuer_name)) {
         issuer = candidate;
      }
   }
   return issuer;
}

#define ERR_STR (ERR_error_string (ERR_get_error (), NULL))

int
_mongoc_ocsp_tlsext_status_cb (SSL *ssl, void *arg)
{
   const int ERROR = -1, FAILURE = 0, SUCCESS = 1;
   OCSP_RESPONSE *resp = NULL;
   OCSP_BASICRESP *basic = NULL;
   X509_STORE *store = NULL;
   X509 *peer = NULL, *issuer = NULL;
   STACK_OF (X509) *cert_chain = NULL;
   const unsigned char *r = NULL;
   int cert_status, reason, len, status, ret;
   OCSP_CERTID *id = NULL;
   mongoc_openssl_ocsp_opt_t *opts = (mongoc_openssl_ocsp_opt_t *) arg;
   ASN1_GENERALIZEDTIME *produced_at = NULL, *this_update = NULL,
                        *next_update = NULL;

   if (opts->weak_cert_validation) {
      return SUCCESS;
   }

   if (!(peer = SSL_get_peer_certificate (ssl))) {
      MONGOC_ERROR ("No certificate was presented by the peer: %s", ERR_STR);
      ret = ERROR;
      goto done;
   }

   /* Get the stapled OCSP response returned by the server */
   len = SSL_get_tlsext_status_ocsp_resp (ssl, &r);
   if (!r) {
      bool must_staple = X509_get_ext_d2i (peer, NID_tlsfeature, 0, 0) != NULL;
      if (must_staple) {
         MONGOC_ERROR ("Server must contain a stapled response: %s", ERR_STR);
         ret = FAILURE;
         goto done;
      }
      ret = SUCCESS; // TODO: contact OCSP responder
      goto done;
   }

   /* obtain an OCSP_RESPONSE object from the OCSP response */
   if (!d2i_OCSP_RESPONSE (&resp, &r, len)) {
      MONGOC_ERROR ("Failed to parse OCSP response: %s", ERR_STR);
      ret = ERROR;
      goto done;
   }

   /* Validate the OCSP response status of the OCSP_RESPONSE object */
   status = OCSP_response_status (resp);
   if (status != OCSP_RESPONSE_STATUS_SUCCESSFUL) {
      MONGOC_ERROR ("OCSP response error %d %s",
                    status,
                    OCSP_response_status_str (status));
      ret = ERROR;
      goto done;
   }

   /* Get the OCSP_BASICRESP structure contained in OCSP_RESPONSE object for the
    * peer cert */
   basic = OCSP_response_get1_basic (resp);
   if (!basic) {
      MONGOC_ERROR ("Could not find BasicOCSPResponse: %s", ERR_STR);
      ret = ERROR;
      goto done;
   }

   store = SSL_CTX_get_cert_store (SSL_get_SSL_CTX (ssl));

   /* Get a STACK_OF(X509) certs forming the cert chain of the peer, including
    * the peer's cert */
   if (!(cert_chain = SSL_get0_verified_chain (ssl))) {
      MONGOC_ERROR ("No certificate was presented by the peer: %s", ERR_STR);
      ret = ERROR;
      goto done;
   }

   /*
    * checks that the basic response message is correctly signed and that the
    * signer certificate can be validated.
    * 1. The function first verifies the signer cert of the response is in the
    * given cert chain.
    * 2. Next, the function verifies the signature of the basic response.
    * 3. Finally, the function validates the signer cert, constructing the
    * validation path via the untrusted cert chain.
    */
   if (SUCCESS != OCSP_basic_verify (basic, cert_chain, store, 0)) {
      MONGOC_ERROR ("OCSP response failed verification: %s", ERR_STR);
      ret = ERROR;
      goto done;
   }

   if (!(issuer = _get_issuer (peer, cert_chain))) {
      MONGOC_ERROR ("Could not get issuer from peer cert");
      ret = ERROR;
      goto done;
   }

   if (!(id = OCSP_cert_to_id (NULL /* SHA1 */, peer, issuer))) {
      MONGOC_ERROR ("Could not obtain a valid OCSP_CERTID for peer: %s",
                    ERR_STR);
      ret = ERROR;
      goto done;
   }

   /* searches the basic response for an OCSP response for the given cert ID */
   if (!OCSP_resp_find_status (basic,
                               id,
                               &cert_status,
                               &reason,
                               &produced_at,
                               &this_update,
                               &next_update)) {
      MONGOC_ERROR ("No OCSP response found for the peer certificate: %s",
                    ERR_STR);
      ret = ERROR;
      goto done;
   }

   /* checks the validity of this_update and next_update values */
   if (!OCSP_check_validity (this_update, next_update, 0L, -1L)) {
      MONGOC_ERROR ("OCSP response has expired: %s", ERR_STR);
      ret = ERROR;
      goto done;
   }

   switch (cert_status) {
   case V_OCSP_CERTSTATUS_GOOD:
      /* TODO: cache response */
      break;

   case V_OCSP_CERTSTATUS_REVOKED:
      MONGOC_ERROR ("OCSP Certificate Status: Revoked. Reason %d", reason);
      ret = FAILURE;
      goto done;

   default: /* V_OCSP_CERTSTATUS_UNKNOWN */
      break;
   }

   /* Validate hostname matches cert */
   if (!opts->allow_invalid_hostname &&
       X509_check_host (peer, opts->host, 0, 0, NULL) != SUCCESS &&
       X509_check_ip_asc (peer, opts->host, 0) != SUCCESS) {
      ret = FAILURE;
      goto done;
   }

   ret = SUCCESS;
done:
   if (basic)
      OCSP_BASICRESP_free (basic);
   if (resp)
      OCSP_RESPONSE_free (resp);
   if (id)
       OCSP_CERTID_free (id);
   if (peer)
       X509_free (peer);
   if (issuer)
       X509_free (issuer);
   return ret;
}
#endif

/**
 * _mongoc_openssl_ctx_new:
 *
 * Create a new ssl context declaratively
 *
 * The opt.pem_pwd parameter, if passed, must exist for the life of this
 * context object (for storing and loading the associated pem file)
 */
SSL_CTX *
_mongoc_openssl_ctx_new (mongoc_ssl_opt_t *opt)
{
   SSL_CTX *ctx = NULL;
   int ssl_ctx_options = 0;

   /*
    * Ensure we are initialized. This is safe to call multiple times.
    */
   mongoc_init ();

   ctx = SSL_CTX_new (SSLv23_method ());

   BSON_ASSERT (ctx);

   /* SSL_OP_ALL - Activate all bug workaround options, to support buggy client
    * SSL's. */
   ssl_ctx_options |= SSL_OP_ALL;

   /* SSL_OP_NO_SSLv2 - Disable SSL v2 support */
   ssl_ctx_options |= SSL_OP_NO_SSLv2;

/* Disable compression, if we can.
 * OpenSSL 0.9.x added compression support which was always enabled when built
 * against zlib
 * OpenSSL 1.0.0 added the ability to disable it, while keeping it enabled by
 * default
 * OpenSSL 1.1.0 disabled it by default.
 */
#if OPENSSL_VERSION_NUMBER >= 0x10000000L
   ssl_ctx_options |= SSL_OP_NO_COMPRESSION;
#endif

/* man SSL_get_options says: "SSL_OP_NO_RENEGOTIATION options were added in
 * OpenSSL 1.1.1". */
#ifdef SSL_OP_NO_RENEGOTIATION
   ssl_ctx_options |= SSL_OP_NO_RENEGOTIATION;
#endif

   SSL_CTX_set_options (ctx, ssl_ctx_options);

/* only defined in special build, using:
 * --enable-system-crypto-profile (autotools)
 * -DENABLE_CRYPTO_SYSTEM_PROFILE:BOOL=ON (cmake)  */
#ifndef MONGOC_ENABLE_CRYPTO_SYSTEM_PROFILE
   /* HIGH - Enable strong ciphers
    * !EXPORT - Disable export ciphers (40/56 bit)
    * !aNULL - Disable anonymous auth ciphers
    * @STRENGTH - Sort ciphers based on strength */
   SSL_CTX_set_cipher_list (ctx, "HIGH:!EXPORT:!aNULL@STRENGTH");
#endif

   /* If renegotiation is needed, don't return from recv() or send() until it's
    * successful.
    * Note: this is for blocking sockets only. */
   SSL_CTX_set_mode (ctx, SSL_MODE_AUTO_RETRY);

   /* Load my private keys to present to the server */
   if (opt->pem_file &&
       !_mongoc_openssl_setup_pem_file (ctx, opt->pem_file, opt->pem_pwd)) {
      SSL_CTX_free (ctx);
      return NULL;
   }

   /* Load in my Certificate Authority, to verify the server against
    * If none provided, fallback to the distro defaults */
   if (opt->ca_file || opt->ca_dir) {
      if (!_mongoc_openssl_setup_ca (ctx, opt->ca_file, opt->ca_dir)) {
         SSL_CTX_free (ctx);
         return NULL;
      }
   } else {
/* If the server certificate is issued by known CA we trust it by default */
#ifdef _WIN32
      _mongoc_openssl_import_cert_stores (ctx);
#else
      SSL_CTX_set_default_verify_paths (ctx);
#endif
   }

   /* Load my revocation list, to verify the server against */
   if (opt->crl_file && !_mongoc_openssl_setup_crl (ctx, opt->crl_file)) {
      SSL_CTX_free (ctx);
      return NULL;
   }

   return ctx;
}


char *
_mongoc_openssl_extract_subject (const char *filename, const char *passphrase)
{
   X509_NAME *subject = NULL;
   X509 *cert = NULL;
   BIO *certbio = NULL;
   BIO *strbio = NULL;
   char *str = NULL;
   int ret;

   if (!filename) {
      return NULL;
   }

   certbio = BIO_new (BIO_s_file ());
   strbio = BIO_new (BIO_s_mem ());
   ;

   BSON_ASSERT (certbio);
   BSON_ASSERT (strbio);


   if (BIO_read_filename (certbio, filename) &&
       (cert = PEM_read_bio_X509 (certbio, NULL, 0, NULL))) {
      if ((subject = X509_get_subject_name (cert))) {
         ret = X509_NAME_print_ex (strbio, subject, 0, XN_FLAG_RFC2253);

         if ((ret > 0) && (ret < INT_MAX)) {
            str = (char *) bson_malloc (ret + 2);
            BIO_gets (strbio, str, ret + 1);
            str[ret] = '\0';
         }
      }
   }

   if (cert) {
      X509_free (cert);
   }

   if (certbio) {
      BIO_free (certbio);
   }

   if (strbio) {
      BIO_free (strbio);
   }

   return str;
}

#if OPENSSL_VERSION_NUMBER < 0x10100000L
#ifdef _WIN32

static unsigned long
_mongoc_openssl_thread_id_callback (void)
{
   unsigned long ret;

   ret = (unsigned long) GetCurrentThreadId ();
   return ret;
}

#else

static unsigned long
_mongoc_openssl_thread_id_callback (void)
{
   unsigned long ret;

   ret = (unsigned long) pthread_self ();
   return ret;
}

#endif

static void
_mongoc_openssl_thread_locking_callback (int mode,
                                         int type,
                                         const char *file,
                                         int line)
{
   if (mode & CRYPTO_LOCK) {
      bson_mutex_lock (&gMongocOpenSslThreadLocks[type]);
   } else {
      bson_mutex_unlock (&gMongocOpenSslThreadLocks[type]);
   }
}

static void
_mongoc_openssl_thread_startup (void)
{
   int i;

   gMongocOpenSslThreadLocks = (bson_mutex_t *) OPENSSL_malloc (
      CRYPTO_num_locks () * sizeof (bson_mutex_t));

   for (i = 0; i < CRYPTO_num_locks (); i++) {
      bson_mutex_init (&gMongocOpenSslThreadLocks[i]);
   }

   if (!CRYPTO_get_locking_callback ()) {
      CRYPTO_set_locking_callback (_mongoc_openssl_thread_locking_callback);
      CRYPTO_set_id_callback (_mongoc_openssl_thread_id_callback);
   }
}

static void
_mongoc_openssl_thread_cleanup (void)
{
   int i;

   if (CRYPTO_get_locking_callback () ==
       _mongoc_openssl_thread_locking_callback) {
      CRYPTO_set_locking_callback (NULL);
   }

   if (CRYPTO_get_id_callback () == _mongoc_openssl_thread_id_callback) {
      CRYPTO_set_id_callback (NULL);
   }

   for (i = 0; i < CRYPTO_num_locks (); i++) {
      bson_mutex_destroy (&gMongocOpenSslThreadLocks[i]);
   }
   OPENSSL_free (gMongocOpenSslThreadLocks);
}
#endif

#endif
