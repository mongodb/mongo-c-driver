/*
 * Copyright 2016 MongoDB, Inc.
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

#ifdef MONGOC_ENABLE_SECURE_TRANSPORT

#include <bson.h>

#include "mongoc-log.h"
#include "mongoc-trace.h"
#include "mongoc-ssl.h"
#include "mongoc-stream-tls.h"
#include "mongoc-secure-transport-private.h"
#include "mongoc-stream-tls-secure-transport-private.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecKey.h>
#include <Security/SecureTransport.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/Security.h>
#include <Security/SecureTransport.h>
#include <CoreFoundation/CoreFoundation.h>

/* Jailbreak Darwin Private API */
SecIdentityRef SecIdentityCreate(CFAllocatorRef allocator, SecCertificateRef certificate, SecKeyRef privateKey);

#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-secure_transport"


void
_bson_append_cftyperef (bson_string_t *retval, const char *label, CFTypeRef str)
{
   if (str) {
      if (CFGetTypeID (str) == CFStringGetTypeID ()) {
         const char *cs = CFStringGetCStringPtr (str, kCFStringEncodingUTF8);

         bson_string_append_printf (retval, "%s%s", label, cs);
      }
   }
}

CFTypeRef
_mongoc_secure_transport_dict_get (CFArrayRef values, CFStringRef label)
{
   if (!values || CFGetTypeID (values) != CFArrayGetTypeID ()) {
      return NULL;
   }

   for (CFIndex i = 0; i < CFArrayGetCount (values); ++i) {
      CFStringRef item_label;
      CFDictionaryRef item = CFArrayGetValueAtIndex (values, i);

      if (CFGetTypeID (item) != CFDictionaryGetTypeID ()) {
         continue;
      }

      item_label = CFDictionaryGetValue (item, kSecPropertyKeyLabel);
      if (item_label && CFStringCompare (item_label, label, 0) == kCFCompareEqualTo) {
         return CFDictionaryGetValue (item, kSecPropertyKeyValue);
      }
   }

   return NULL;
}

char *
_mongoc_secure_transport_RFC2253_from_cert (SecCertificateRef cert)
{
   CFTypeRef value;
   bson_string_t *retval;
   CFTypeRef subject_name;
   CFDictionaryRef cert_dict;

   cert_dict = SecCertificateCopyValues (cert, NULL, NULL);
   if (!cert_dict) {
      return NULL;
   }

   subject_name = CFDictionaryGetValue (cert_dict, kSecOIDX509V1SubjectName);
   if (!subject_name) {
      CFRelease (cert_dict);
      return NULL;
   }

   subject_name = CFDictionaryGetValue (subject_name, kSecPropertyKeyValue);
   if (!subject_name) {
      CFRelease (cert_dict);
      return NULL;
   }

   retval = bson_string_new ("");;

   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDCommonName);
   _bson_append_cftyperef (retval, "CN=", value);

   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDOrganizationalUnitName);
   if (value) {
      /* Can be either one unit name, or array of unit names */
      if (CFGetTypeID(value) == CFStringGetTypeID()) {
         _bson_append_cftyperef (retval, ",OU=", value);
      } else if (CFGetTypeID(value) == CFArrayGetTypeID()) {
         CFIndex len = CFArrayGetCount(value);

         if (len > 0) {
            _bson_append_cftyperef (retval, ",OU=", CFArrayGetValueAtIndex(value, 0));
         }
         if (len > 1) {
            _bson_append_cftyperef (retval, ",", CFArrayGetValueAtIndex(value, 1));
         }
         if (len > 2) {
            _bson_append_cftyperef (retval, ",", CFArrayGetValueAtIndex(value, 2));
         }
      }
   }

   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDOrganizationName);
   _bson_append_cftyperef (retval, ",O=", value);

   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDLocalityName);
   _bson_append_cftyperef (retval, ",L=", value);

   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDStateProvinceName);
   _bson_append_cftyperef (retval, ",ST=", value);

   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDCountryName);
   _bson_append_cftyperef (retval, ",C=", value);

   /* This seems rarely used */
   value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDStreetAddress);
   _bson_append_cftyperef (retval, ",STREET", value);

   CFRelease (cert_dict);
   return bson_string_free (retval, false);
}

bool
_mongoc_secure_transport_import_pem (const char *filename, const char *passphrase, CFArrayRef *items, SecExternalItemType *type)
{
   SecExternalFormat format = kSecFormatPEMSequence;
   SecItemImportExportKeyParameters params;
   SecTransformRef sec_transform;
   CFReadStreamRef read_stream;
   CFDataRef dataref;
   CFErrorRef error;
   CFURLRef url;
   OSStatus res;


   if (!filename) {
      MONGOC_WARNING("%s", "No certificate provided");
      return false;
   }

   params.version = SEC_KEY_IMPORT_EXPORT_PARAMS_VERSION;
   params.flags = 0;
   params.passphrase = NULL;
   params.alertTitle = NULL;
   params.alertPrompt = NULL;
   params.accessRef = NULL;
   params.keyUsage = NULL;
   params.keyAttributes = NULL;

   if (passphrase) {
      params.passphrase = CFStringCreateWithCString (kCFAllocatorDefault, passphrase, kCFStringEncodingUTF8);
   }

   url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const UInt8 *)filename, strlen (filename), false);
   read_stream = CFReadStreamCreateWithFile (kCFAllocatorDefault, url);
   sec_transform = SecTransformCreateReadTransformWithReadStream (read_stream);
   dataref = SecTransformExecute (sec_transform, &error);


   if (error) {
      CFStringRef str = CFErrorCopyDescription (error);
      MONGOC_WARNING ("Failed importing PEM '%s': %s", filename, CFStringGetCStringPtr (str, kCFStringEncodingUTF8));
      CFRelease (str);
      CFRelease (sec_transform);
      CFRelease (read_stream);
      CFRelease (url);

      if (passphrase) {
         CFRelease (params.passphrase);
      }
      return false;
   }

   res = SecItemImport (dataref, CFSTR(".pem"), &format, type, 0, &params, NULL, items);
   CFRelease (dataref);
   CFRelease (sec_transform);
   CFRelease (read_stream);
   CFRelease (url);

   if (passphrase) {
      CFRelease (params.passphrase);
   }
   if (res) {
      return false;
   }

   return true;
}

char *
_mongoc_secure_transport_extract_subject (const char *filename, const char *passphrase)
{
   char *retval = NULL;
   CFArrayRef items = NULL;
   SecExternalItemType type = kSecItemTypeCertificate;

   bool success = _mongoc_secure_transport_import_pem (filename, passphrase, &items, &type);

   if (!success) {
      MONGOC_WARNING("Can't find certificate in '%s'", filename);
      return NULL;
   }

   if (type == kSecItemTypeAggregate) {
      for (CFIndex i = 0; i < CFArrayGetCount (items); ++i) {
         CFTypeID item_id = CFGetTypeID (CFArrayGetValueAtIndex (items, i));

         if (item_id == SecCertificateGetTypeID()) {
            retval = _mongoc_secure_transport_RFC2253_from_cert ((SecCertificateRef)CFArrayGetValueAtIndex (items, i));
            break;
         }
      }
   } else if (type == kSecItemTypeCertificate) {
      retval = _mongoc_secure_transport_RFC2253_from_cert ((SecCertificateRef)items);
   }

   if (items) {
      CFRelease (items);
   }

   return retval;
}


const SSLCipherSuite suites40[] = {
   SSL_RSA_EXPORT_WITH_RC4_40_MD5,
   SSL_RSA_EXPORT_WITH_RC2_CBC_40_MD5,
   SSL_RSA_EXPORT_WITH_DES40_CBC_SHA,
   SSL_DH_DSS_EXPORT_WITH_DES40_CBC_SHA,
   SSL_DH_RSA_EXPORT_WITH_DES40_CBC_SHA,
   SSL_DHE_DSS_EXPORT_WITH_DES40_CBC_SHA,
   SSL_DHE_RSA_EXPORT_WITH_DES40_CBC_SHA,
   SSL_DH_anon_EXPORT_WITH_RC4_40_MD5,
   SSL_DH_anon_EXPORT_WITH_DES40_CBC_SHA,
   SSL_NO_SUCH_CIPHERSUITE
};

/*
 * Given an SSLContextRef and an array of SSLCipherSuites, terminated by
 * SSL_NO_SUCH_CIPHERSUITE, select those SSLCipherSuites which the library
 * supports and do a SSLSetEnabledCiphers() specifying those.
 */
OSStatus sslSetEnabledCiphers(
   SSLContextRef ssl_ctx_ref,
   const SSLCipherSuite *ciphers)
{
   size_t numSupported;
   OSStatus status;
   SSLCipherSuite *supported = NULL;
   SSLCipherSuite *enabled = NULL;
   unsigned enabledDex = 0;   // index into enabled
   unsigned supportedDex = 0; // index into supported
   unsigned inDex = 0;        // index into ciphers

   /* first get all the supported ciphers */
   status = SSLGetNumberSupportedCiphers(ssl_ctx_ref, &numSupported);
   if(status) {
      return status;
   }
   supported = (SSLCipherSuite *)malloc(numSupported * sizeof(SSLCipherSuite));
   status = SSLGetSupportedCiphers(ssl_ctx_ref, supported, &numSupported);
   if(status) {
      return status;
   }

   /*
    * Malloc an array we'll use for SSLGetEnabledCiphers - this will  be
    * bigger than the number of suites we actually specify
    */
   enabled = (SSLCipherSuite *)malloc(numSupported * sizeof(SSLCipherSuite));

   /*
    * For each valid suite in ciphers, see if it's in the list of
    * supported ciphers. If it is, add it to the list of ciphers to be
    * enabled.
    */
   for(inDex=0; ciphers[inDex] != SSL_NO_SUCH_CIPHERSUITE; inDex++) {
      for(supportedDex=0; supportedDex<numSupported; supportedDex++) {
         if(ciphers[inDex] == supported[supportedDex]) {
            enabled[enabledDex++] = ciphers[inDex];
            break;
         }
      }
   }

   /* send it on down. */
   status = SSLSetEnabledCiphers(ssl_ctx_ref, enabled, enabledDex);
   free(enabled);
   free(supported);
   return status;
}

bool
mongoc_secure_transport_verify_trust (mongoc_stream_tls_secure_transport_t *secure_transport,
                                      bool weak_cert_validation)
{
   SecTrustRef peerTrust = NULL;
   SecTrustResultType trustResult;
   OSStatus status;

   if (weak_cert_validation) {
      return true;
   }

   status = SSLCopyPeerTrust (secure_transport->ssl_ctx_ref, &peerTrust);
   if (status || !peerTrust) {
      MONGOC_WARNING("Failed to get peer trust");
      return false;
   }

   status = SecTrustSetAnchorCertificates (peerTrust, secure_transport->anchors);
#if 0
      /* This will add back the OS built-in anchors */
      SecTrustSetAnchorCertificatesOnly (peerTrust, false);
#endif

   SecTrustEvaluate (peerTrust, &trustResult);
   CFRelease (peerTrust);

   if (trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified) {
      return true;
   }
   if (trustResult == kSecTrustResultRecoverableTrustFailure) {
      MONGOC_WARNING("Recoverable trust failure."
                     " Probably mismatched hostname or expired certificate."
                     " Or Unknown Certificate Authority");
      return false;
   }

   MONGOC_WARNING("Failed to evaluate trust: %d", trustResult);
   return false;
}

bool
mongoc_secure_transport_setup_certificate (mongoc_stream_tls_secure_transport_t *secure_transport,
                                           mongoc_ssl_opt_t *opt)
{
   bool success;
   CFArrayRef items;
   SecIdentityRef id;
   SecKeyRef key = NULL;
   SecCertificateRef cert = NULL;
   SecExternalItemType type = kSecItemTypeCertificate;

   if (!opt->pem_file) {
      MONGOC_WARNING ("No private key provided, the server won't be able to verify us");
   }

   success = _mongoc_secure_transport_import_pem (opt->pem_file, opt->pem_pwd, &items, &type);
   if (!success) {
      MONGOC_WARNING("Can't find certificate in '%s'", opt->pem_file);
      return false;
   }

   if (type != kSecItemTypeAggregate) {
      MONGOC_WARNING ("What sort of privat key is '%d'that?", type);
      CFRelease (items);
      return false;
   }

   for (CFIndex i = 0; i < CFArrayGetCount (items); ++i) {
      CFTypeID item_id = CFGetTypeID (CFArrayGetValueAtIndex (items, i));

      if (item_id == SecCertificateGetTypeID()) {
         cert = (SecCertificateRef) CFArrayGetValueAtIndex (items, i);
      } else if (item_id == SecKeyGetTypeID()) {
         key = (SecKeyRef) CFArrayGetValueAtIndex (items, i);
      }
   }

   if (!cert || !key) {
      MONGOC_WARNING("Couldn't find valid private key");
      CFRelease (items);
      return false;
   }

   id = SecIdentityCreate (kCFAllocatorDefault, cert, key);
   secure_transport->my_cert = CFArrayCreateMutableCopy(kCFAllocatorDefault, (CFIndex)2, items);

   CFArraySetValueAtIndex(secure_transport->my_cert, 0, id);
   CFArraySetValueAtIndex(secure_transport->my_cert, 1, cert);

   /*
    *  Secure Transport assumes the following:
    *    * The certificate references remain valid for the lifetime of the session.
    *    * The identity specified in certRefs[0] is capable of signing.
    */
   success = !SSLSetCertificate (secure_transport->ssl_ctx_ref, secure_transport->my_cert);
   MONGOC_DEBUG("Setting client certificate %s", success ? "succeeded" : "failed");

   CFRelease (items);
   return true;
}

bool
mongoc_secure_transport_setup_ca (mongoc_stream_tls_secure_transport_t *secure_transport,
                                  mongoc_ssl_opt_t *opt)
{
   if (opt->ca_file) {
      CFArrayRef items;
      SecExternalItemType type = kSecItemTypeCertificate;
      CFMutableArrayRef anchors = CFArrayCreateMutable (kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
      bool success = _mongoc_secure_transport_import_pem (opt->ca_file, NULL, &items, &type);

      if (!success) {
         MONGOC_WARNING("Can't find certificate in '%s'", opt->ca_file);
         return false;
      }

      if (type == kSecItemTypeAggregate) {
         for (CFIndex i = 0; i < CFArrayGetCount (items); ++i) {
            CFTypeID item_id = CFGetTypeID (CFArrayGetValueAtIndex (items, i));

            if (item_id == SecCertificateGetTypeID()) {
               CFArrayAppendValue (anchors, CFArrayGetValueAtIndex (items, i));
            }
         }
         secure_transport->anchors = CFArrayCreateCopy (kCFAllocatorDefault, anchors);
      } else if (type == kSecItemTypeCertificate) {
         secure_transport->anchors = CFArrayCreateCopy (kCFAllocatorDefault, items);
      }

      CFRelease (anchors);
      CFRelease (items);
      return true;
   }

   secure_transport->anchors = CFArrayCreateMutable (kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
   MONGOC_WARNING("No CA provided, using defaults");
   return false;
}

OSStatus
mongoc_secure_transport_read (SSLConnectionRef connection,
                              void             *data,
                              size_t           *data_length)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)connection;
   ssize_t length;
   ENTRY;

   errno = 0;
   length = mongoc_stream_read (tls->base_stream, data, *data_length, *data_length,
                                tls->timeout_msec);

   if (length > 0) {
      *data_length = length;
      RETURN(noErr);
   }

   if (length == 0) {
      RETURN(errSSLClosedGraceful);
   }

   switch (errno) {
   case ENOENT:
      RETURN(errSSLClosedGraceful);
      break;
   case ECONNRESET:
      RETURN(errSSLClosedAbort);
      break;
   case EAGAIN:
      RETURN(errSSLWouldBlock);
      break;
   default:
      RETURN(-36); /* ioErr */
      break;
   }
}

OSStatus
mongoc_secure_transport_write (SSLConnectionRef connection,
                               const void       *data,
                               size_t           *data_length)
{
   mongoc_stream_tls_t *tls = (mongoc_stream_tls_t *)connection;
   ssize_t length;
   ENTRY;

   errno = 0;
   length = mongoc_stream_write (tls->base_stream, (void *)data, *data_length,
                                 tls->timeout_msec);

   if (length >= 0) {
      *data_length = length;
      RETURN(noErr);
   }

   switch (errno) {
      case EAGAIN:
         RETURN(errSSLWouldBlock);
         break;
      default:
         RETURN(-36); /* ioErr */
         break;
   }
}
#endif
