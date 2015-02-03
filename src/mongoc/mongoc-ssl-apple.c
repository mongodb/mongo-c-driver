/*
 * Copyright 2015 MongoDB, Inc.
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

#ifdef MONGOC_ENABLE_SSL
#ifdef MONGOC_APPLE_NATIVE_TLS

#include <bson.h>

#include <stdio.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <sys/mman.h>

/*
 *-------------------------------------------------------------------------
 *
 * mongoc_ssl_apple_cfdata_from_filename --
 *
 *-------------------------------------------------------------------------
 */

static CFDataRef
mongoc_ssl_apple_cfdata_from_filename(const char *filename)
{
    int fileDescriptor;
    CFDataRef result = NULL;

    fileDescriptor = open(filename, O_RDONLY, 0);
    if (fileDescriptor != -1) {
        struct stat fileStat;

        if (stat(filename, &fileStat) != -1) {
            unsigned char *fileContent;

            fileContent = mmap(0, (size_t)fileStat.st_size, PROT_READ, MAP_FILE|MAP_PRIVATE, fileDescriptor, 0);
            if (MAP_FAILED != fileContent) {
                result = CFDataCreate(NULL, fileContent, (CFIndex)fileStat.st_size);
                munmap(fileContent, (size_t)fileStat.st_size);
            }
        }
        close(fileDescriptor);
    }
    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_apple_extract_subject --
 *
 *       Extract human-readable subject information from the certificate
 *       in @filename.
 *
 *       Depending on the OS version, we try several different ways of
 *       accessing this data, and the string returned may be a summary
 *       of the certificate, a long description of the certificate, or
 *       just the common name from the cert.
 *
 * Returns:
 *       Certificate data, or NULL if filename could not be processed.
 *
 *-------------------------------------------------------------------------
 */

char *
_mongoc_ssl_apple_extract_subject (const char *filename)
{
    CFDataRef data;
    char *result = NULL;
    OSStatus err;

    data = mongoc_ssl_apple_cfdata_from_filename(filename);
    if (data) {
        SecCertificateRef certificate;

        certificate = SecCertificateCreateWithData(NULL, data);
        if (certificate) {
            CFStringRef subject = NULL;

            /* we'll try several things here */
            if (SecCertificateCopySubjectSummary != NULL) {
               subject = SecCertificateCopySubjectSummary(certificate);
            }
            if (!subject) {
               subject = SecCertificateCopyLongDescription(NULL, certificate, NULL);
            }
            if (!subject) {
               err = SecCertificateCopyCommonName(certificate, &subject);
               if (err != noErr) {
                  printf("failed to parse out common name, error code %d\n", (int)err);
               }
            }

            if (subject) {
                CFIndex length =
                   CFStringGetMaximumSizeForEncoding(CFStringGetLength(subject), kCFStringEncodingUTF8) + 1;

                result = bson_malloc(length);
                if (!CFStringGetCString(subject, result, length, kCFStringEncodingUTF8)) {
                    bson_free(result);
                    result = NULL;
                }
                CFRelease(subject);
            }
            CFRelease(certificate);
        }
    }
    CFRelease(data);

    return result;
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_apple_init --
 *
 *       No-op.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_ssl_apple_init (void)
{
   /* no-op */
   // TODO why is this a no-op?
}

/*
 *-------------------------------------------------------------------------
 *
 * _mongoc_ssl_apple_cleanup --
 *
 *       No-op.
 *
 *-------------------------------------------------------------------------
 */

void
_mongoc_ssl_apple_cleanup (void)
{
   /* no-op */
   // TODO why is this a no-op?
}


#endif /* MONGOC_APPLE_NATIVE_TLS */
#endif /* MONGOC_ENABLE_SSL */
