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
#include "mongoc-ssl.h"
#include "mongoc-secure-transport-private.h"

#include <Security/Security.h>
#include <Security/SecKey.h>
#include <CoreFoundation/CoreFoundation.h>

void
_bson_append_cftyperef (bson_string_t *retval, const char *label, CFTypeRef str) {
   if (str) {
      if (CFGetTypeID(str) == CFStringGetTypeID()) {
         const char *cs = CFStringGetCStringPtr(str, kCFStringEncodingMacRoman) ;

         bson_string_append_printf (retval, "%s%s", label, cs);
      }
   }
}

CFTypeRef
_mongoc_secure_transport_dict_get (CFArrayRef values, CFStringRef label)
{
   if (!values || CFGetTypeID(values) != CFArrayGetTypeID()) {
      return NULL;
   }

   for (CFIndex i = 0; i < CFArrayGetCount(values); ++i) {
      CFStringRef item_label;
      CFDictionaryRef item = CFArrayGetValueAtIndex(values, i);

      if (CFGetTypeID(item) != CFDictionaryGetTypeID()) {
         continue;
      }

      item_label = CFDictionaryGetValue(item, kSecPropertyKeyLabel);
      if (item_label && CFStringCompare(item_label, label, 0) == kCFCompareEqualTo) {
         return CFDictionaryGetValue(item, kSecPropertyKeyValue);
      }
   }

   return NULL;
}

char *
_mongoc_secure_transport_RFC2253_from_cert (SecCertificateRef cert)
{
   CFTypeRef subject_name;
   CFDictionaryRef cert_dict;

   cert_dict = SecCertificateCopyValues (cert, NULL, NULL);
   if (!cert_dict) {
      return NULL;
   }

   subject_name = CFDictionaryGetValue (cert_dict, kSecOIDX509V1SubjectName);
   if (subject_name) {
      subject_name = CFDictionaryGetValue (subject_name, kSecPropertyKeyValue);
   }

   if (subject_name) {
      CFTypeRef value;
      bson_string_t *retval = bson_string_new ("");;

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDCommonName);
      _bson_append_cftyperef (retval, "CN=", value);

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDOrganizationalUnitName);
      if (value) {
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

      value = _mongoc_secure_transport_dict_get (subject_name, kSecOIDStreetAddress);
      _bson_append_cftyperef (retval, ",STREET", value);

      CFRelease (cert_dict);
      return bson_string_free (retval, false);
   }

   CFRelease (cert_dict);
   return NULL;
}

char *
_mongoc_secure_transport_extract_subject (const char *filename)
{
   SecExternalItemType item_type = kSecItemTypeUnknown;
   SecExternalFormat format = kSecFormatUnknown;
   CFArrayRef cert_items = NULL;
   CFDataRef dataref;
   OSStatus res;
   CFErrorRef error;
   int n = 0;
   CFURLRef url;
   CFReadStreamRef read_stream;
   SecTransformRef sec_transform;


   url = CFURLCreateFromFileSystemRepresentation (kCFAllocatorDefault, (const UInt8 *)filename, strlen(filename), false);
   read_stream = CFReadStreamCreateWithFile (kCFAllocatorDefault, url);
   sec_transform = SecTransformCreateReadTransformWithReadStream (read_stream);
   dataref = SecTransformExecute (sec_transform, &error);
   if (error) {
      MONGOC_WARNING("Can't read '%s'", filename);
      return NULL;
   }

   res = SecItemImport(dataref, CFSTR(".pem"), &format, &item_type, 0, NULL, NULL, &cert_items);
   if (res) {
      MONGOC_WARNING("Invalid X.509 PEM file '%s'", filename);
      return NULL;
   }

   if (item_type == kSecItemTypeAggregate) {
      for (n=CFArrayGetCount(cert_items); n > 0; n--) {
         CFTypeID item_id = CFGetTypeID (CFArrayGetValueAtIndex (cert_items, n-1));

         if (item_id == SecCertificateGetTypeID()) {
            SecCertificateRef certificate = (SecCertificateRef)CFArrayGetValueAtIndex(cert_items, n-1);

            return _mongoc_secure_transport_RFC2253_from_cert (certificate);
         }
      }
      MONGOC_WARNING("Can't find certificate in '%s'", filename);
   } else {
      MONGOC_WARNING("Unexpected certificate format in '%s'", filename);
   }

   return NULL;
}

#endif
