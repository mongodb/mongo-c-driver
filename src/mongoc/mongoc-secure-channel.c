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

#ifdef MONGOC_ENABLE_SSL_SECURE_CHANNEL

#include <bson.h>

#include "mongoc-log.h"
#include "mongoc-trace.h"
#include "mongoc-ssl.h"
#include "mongoc-stream-tls.h"
#include "mongoc-secure-channel-private.h"
#include "mongoc-stream-tls-secure-channel-private.h"
#include "mongoc-errno-private.h"


#undef MONGOC_LOG_DOMAIN
#define MONGOC_LOG_DOMAIN "stream-secure-channel"


char *
_mongoc_secure_channel_extract_subject (const char *filename,
                                        const char *passphrase)
{
   return NULL;
}

PCCERT_CONTEXT
mongoc_secure_channel_setup_certificate (mongoc_stream_tls_secure_channel_t *secure_channel,
                                         mongoc_ssl_opt_t                   *opt)
{
   HCERTSTORE cert_store = NULL;
   PCCERT_CONTEXT cert = NULL;

   cert_store = CertOpenStore(CERT_STORE_PROV_SYSTEM,                       /* provider */
                              X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,      /* certificate encoding */
                              NULL,                                         /* unused */
                              CERT_SYSTEM_STORE_LOCAL_MACHINE,              /* dwFlags */
                              L"MY");                                       /* system store name. "My" or "Root" */

   if (cert_store == NULL) {
      MONGOC_ERROR("Error retrieving certificate");
      return cert;
   }

   cert = CertFindCertificateInStore(cert_store,                              /* certificate store */
                                     X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, /* certificate encoding */
                                     0,                                       /* unused */
                                     CERT_FIND_SUBJECT_STR_A,                 /* Search type, ASCII Only */
                                     "client",                                /* Search string */
                                     NULL);                                   /* Last certificate found, strtok(3) style */
   CertCloseStore(cert_store, 0);

   return cert;
}

bool
mongoc_secure_channel_setup_ca (mongoc_stream_tls_secure_channel_t *secure_channel,
                                mongoc_ssl_opt_t                   *opt)
{
   return false;
}

size_t
mongoc_secure_channel_read (mongoc_stream_tls_t *tls,
                            void                *data,
                            size_t               data_length)
{
   ssize_t length;

   errno = 0;
   TRACE ("Wanting to read: %d", data_length);
   /* 4th argument is minimum bytes, while the data_length is the
    * size of the buffer. We are totally fine with just one TLS record (few bytes)
    **/
   length = mongoc_stream_read (tls->base_stream, data, data_length, 0,
                                tls->timeout_msec);

   TRACE ("Got %d", length);

   if (length > 0) {
      return length;
   }

   return 0;
}

size_t
mongoc_secure_channel_write (mongoc_stream_tls_t *tls,
                             const void          *data,
                             size_t               data_length)
{
   ssize_t length;

   errno = 0;
   TRACE ("Wanting to write: %d", data_length);
   length = mongoc_stream_write (tls->base_stream, (void *)data, data_length,
                                 tls->timeout_msec);
   TRACE ("Wrote: %d", length);


   return length;
}

/**
 * The follow functions comes from one of my favorite project, cURL!
 * Thank you so much for having gone through the Secure Channel pain for me.
 *
 *
 * Copyright (C) 2012 - 2015, Marc Hoersken, <info@marc-hoersken.de>
 * Copyright (C) 2012, Mark Salisbury, <mark.salisbury@hp.com>
 * Copyright (C) 2012 - 2015, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

/*
 * Based upon the PolarSSL implementation in polarssl.c and polarssl.h:
 *   Copyright (C) 2010, 2011, Hoi-Ho Chan, <hoiho.chan@gmail.com>
 *
 * Based upon the CyaSSL implementation in cyassl.c and cyassl.h:
 *   Copyright (C) 1998 - 2012, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * Thanks for code and inspiration!
 */

void
_mongoc_secure_channel_init_sec_buffer (SecBuffer     *buffer,
                                        unsigned long  buf_type,
                                        void          *buf_data_ptr,
                                        unsigned long  buf_byte_size)
{
   buffer->cbBuffer = buf_byte_size;
   buffer->BufferType = buf_type;
   buffer->pvBuffer = buf_data_ptr;
}

void
_mongoc_secure_channel_init_sec_buffer_desc (SecBufferDesc *desc,
                                             SecBuffer     *buffer_array,
                                             unsigned long  buffer_count)
{
   desc->ulVersion = SECBUFFER_VERSION;
   desc->pBuffers = buffer_array;
   desc->cBuffers = buffer_count;
}


bool
mongoc_secure_channel_handshake_step_1 (mongoc_stream_tls_t *tls,
                                        char                *hostname)
{
   SecBuffer outbuf;
   ssize_t written = -1;
   SecBufferDesc outbuf_desc;
   SECURITY_STATUS sspi_status = SEC_E_OK;
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *)tls->ctx;

   TRACE ("SSL/TLS connection with '%s' (step 1/3)", hostname);

   /* setup output buffer */
   _mongoc_secure_channel_init_sec_buffer (&outbuf, SECBUFFER_EMPTY, NULL, 0);
   _mongoc_secure_channel_init_sec_buffer_desc (&outbuf_desc, &outbuf, 1);

   /* setup request flags */
   secure_channel->req_flags = ISC_REQ_SEQUENCE_DETECT |
                               ISC_REQ_REPLAY_DETECT |
                               ISC_REQ_CONFIDENTIALITY |
                               ISC_REQ_ALLOCATE_MEMORY |
                               ISC_REQ_STREAM;

   /* allocate memory for the security context handle */
   secure_channel->ctxt = (mongoc_secure_channel_ctxt *)bson_malloc0 (sizeof (mongoc_secure_channel_ctxt));

   /* https://msdn.microsoft.com/en-us/library/windows/desktop/aa375924.aspx */
   sspi_status = InitializeSecurityContext (&secure_channel->cred->cred_handle, /* phCredential */
                                            NULL,                               /* phContext */
                                            hostname,                           /* pszTargetName */
                                            secure_channel->req_flags,          /* fContextReq */
                                            0,                                  /* Reserved1, must be 0 */
                                            0,                                  /* TargetDataRep, unused */
                                            NULL,                               /* pInput */
                                            0,                                  /* Reserved2, must be 0 */
                                            &secure_channel->ctxt->ctxt_handle, /* phNewContext OUT param */
                                            &outbuf_desc,                       /* pOutput OUT param */
                                            &secure_channel->ret_flags,         /* pfContextAttr OUT param */
                                            &secure_channel->ctxt->time_stamp   /* ptsExpiry OUT param */
   );

   if (sspi_status != SEC_I_CONTINUE_NEEDED) {
      MONGOC_ERROR ("initial InitializeSecurityContext failed: %d", sspi_status);
      return false;
   }

   TRACE ("sending initial handshake data: sending %lu bytes...", outbuf.cbBuffer);

   /* send initial handshake data which is now stored in output buffer */
   written = mongoc_secure_channel_write (tls, outbuf.pvBuffer, outbuf.cbBuffer);
   FreeContextBuffer (outbuf.pvBuffer);

   if (outbuf.cbBuffer != (size_t)written) {
      MONGOC_ERROR ("failed to send initial handshake data: "
                    "sent %zd of %lu bytes", written, outbuf.cbBuffer);
      return false;
   }

   TRACE ("sent initial handshake data: sent %zd bytes", written);

   secure_channel->recv_unrecoverable_err = 0;
   secure_channel->recv_sspi_close_notify = false;
   secure_channel->recv_connection_closed = false;

   /* continue to second handshake step */
   secure_channel->connecting_state = ssl_connect_2;

   return true;
}

bool
mongoc_secure_channel_handshake_step_2 (mongoc_stream_tls_t *tls,
                                        char                *hostname)
{
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *)tls->ctx;
   SECURITY_STATUS sspi_status = SEC_E_OK;
   unsigned char *reallocated_buffer;
   ssize_t nread = -1, written = -1;
   size_t reallocated_length;
   SecBufferDesc outbuf_desc;
   SecBufferDesc inbuf_desc;
   SecBuffer outbuf[3];
   SecBuffer inbuf[2];
   bool doread;
   int i;

   doread = (secure_channel->connecting_state != ssl_connect_2_writing) ? true : false;

   TRACE ("SSL/TLS connection with endpoint (step 2/3)");

   if (!secure_channel->cred || !secure_channel->ctxt) {
      return false;
   }

   /* buffer to store previously received and decrypted data */
   if (secure_channel->decdata_buffer == NULL) {
      secure_channel->decdata_offset = 0;
      secure_channel->decdata_length = MONGOC_SCHANNEL_BUFFER_INIT_SIZE;
      secure_channel->decdata_buffer = bson_malloc0 (secure_channel->decdata_length);
   }

   /* buffer to store previously received and encrypted data */
   if (secure_channel->encdata_buffer == NULL) {
      secure_channel->encdata_offset = 0;
      secure_channel->encdata_length = MONGOC_SCHANNEL_BUFFER_INIT_SIZE;
      secure_channel->encdata_buffer = bson_malloc0 (secure_channel->encdata_length);
   }

   /* if we need a bigger buffer to read a full message, increase buffer now */
   if (secure_channel->encdata_length - secure_channel->encdata_offset <
       MONGOC_SCHANNEL_BUFFER_FREE_SIZE) {
      /* increase internal encrypted data buffer */
      reallocated_length = secure_channel->encdata_offset +
                           MONGOC_SCHANNEL_BUFFER_FREE_SIZE;
      reallocated_buffer = bson_realloc (secure_channel->encdata_buffer, reallocated_length);

      secure_channel->encdata_buffer = reallocated_buffer;
      secure_channel->encdata_length = reallocated_length;
   }

   for (;; ) {
      if (doread) {
         /* read encrypted handshake data from socket */
         nread = mongoc_secure_channel_read (tls,
            (char *)(secure_channel-> encdata_buffer + secure_channel->encdata_offset),
            secure_channel->encdata_length - secure_channel->encdata_offset);

         if (!nread) {
            if (MONGOC_ERRNO_IS_AGAIN (errno)) {
               if (secure_channel->connecting_state != ssl_connect_2_writing) {
                  secure_channel->connecting_state = ssl_connect_2_reading;
               }

               TRACE ("failed to receive handshake, need more data");
               return true;
            }

            MONGOC_ERROR ("failed to receive handshake, SSL/TLS connection failed");
            return false;
         }

         /* increase encrypted data buffer offset */
         secure_channel->encdata_offset += nread;
      }

      TRACE ("encrypted data buffer: offset %zu length %zu",
             secure_channel->encdata_offset, secure_channel->encdata_length);

      /* setup input buffers */
      _mongoc_secure_channel_init_sec_buffer (&inbuf[0], SECBUFFER_TOKEN,
                     malloc (secure_channel->encdata_offset),
                     (unsigned long)(secure_channel->encdata_offset & (size_t)0xFFFFFFFFUL));
      _mongoc_secure_channel_init_sec_buffer (&inbuf[1], SECBUFFER_EMPTY, NULL, 0);
      _mongoc_secure_channel_init_sec_buffer_desc (&inbuf_desc, inbuf, 2);

      /* setup output buffers */
      _mongoc_secure_channel_init_sec_buffer (&outbuf[0], SECBUFFER_TOKEN, NULL, 0);
      _mongoc_secure_channel_init_sec_buffer (&outbuf[1], SECBUFFER_ALERT, NULL, 0);
      _mongoc_secure_channel_init_sec_buffer (&outbuf[2], SECBUFFER_EMPTY, NULL, 0);
      _mongoc_secure_channel_init_sec_buffer_desc (&outbuf_desc, outbuf, 3);

      if (inbuf[0].pvBuffer == NULL) {
         MONGOC_ERROR ("unable to allocate memory");
         return false;
      }

      /* copy received handshake data into input buffer */
      memcpy (inbuf[0].pvBuffer, secure_channel->encdata_buffer,
              secure_channel->encdata_offset);

      /* https://msdn.microsoft.com/en-us/library/windows/desktop/aa375924.aspx
       */
      sspi_status = InitializeSecurityContext (
         &secure_channel->cred->cred_handle, &secure_channel->ctxt->ctxt_handle,
         hostname, secure_channel->req_flags, 0, 0, &inbuf_desc, 0, NULL,
         &outbuf_desc, &secure_channel->ret_flags,
         &secure_channel->ctxt->time_stamp);

      /* free buffer for received handshake data */
      free (inbuf[0].pvBuffer);

      /* check if the handshake was incomplete */
      if (sspi_status == SEC_E_INCOMPLETE_MESSAGE) {
         secure_channel->connecting_state = ssl_connect_2_reading;
         TRACE ("received incomplete message, need more data");
         return true;
      }

      /* If the server has requested a client certificate, attempt to continue
       * the handshake without one. This will allow connections to servers which
       * request a client certificate but do not require it. */
      if (sspi_status == SEC_I_INCOMPLETE_CREDENTIALS &&
          !(secure_channel->req_flags & ISC_REQ_USE_SUPPLIED_CREDS)) {
         secure_channel->req_flags |= ISC_REQ_USE_SUPPLIED_CREDS;
         secure_channel->connecting_state = ssl_connect_2_writing;
         MONGOC_WARNING ("a client certificate has been requested");
         return true;
      }

      /* check if the handshake needs to be continued */
      if (sspi_status == SEC_I_CONTINUE_NEEDED || sspi_status == SEC_E_OK) {
         for (i = 0; i < 3; i++) {
            /* search for handshake tokens that need to be send */
            if (outbuf[i].BufferType == SECBUFFER_TOKEN && outbuf[i].cbBuffer >
                0) {
               TRACE ("sending next handshake data: sending %lu bytes...", outbuf[i].cbBuffer);

               /* send handshake token to server */
               written = mongoc_secure_channel_write (tls, outbuf[i].pvBuffer,
                                                      outbuf[i].cbBuffer);

               if (outbuf[i].cbBuffer != (size_t)written) {
                  MONGOC_ERROR ("failed to send next handshake data: "
                                "sent %zd of %lu bytes", written,
                                outbuf[i].cbBuffer);
                  return false;
               }
            }

            /* free obsolete buffer */
            if (outbuf[i].pvBuffer != NULL) {
               FreeContextBuffer (outbuf[i].pvBuffer);
            }
         }
      } else {
         switch (sspi_status) {
         case SEC_E_WRONG_PRINCIPAL:
            MONGOC_ERROR ("SSL Certification verification failed: hostname doesn't match certificate");
            break;

         case SEC_E_UNTRUSTED_ROOT:
            MONGOC_ERROR ("SSL Certification verification failed: Untrusted root certificate");
            break;

         case SEC_E_CERT_EXPIRED:
            MONGOC_ERROR ("SSL Certification verification failed: certificate has expired");
            break;
         case CRYPT_E_NO_REVOCATION_CHECK:
            /* This seems to be raised also when hostname doesn't match the certificate */
            MONGOC_ERROR ("SSL Certification verification failed: failed revocation/hostname check");
            break;

         case SEC_E_INSUFFICIENT_MEMORY:
         case SEC_E_INTERNAL_ERROR:
         case SEC_E_INVALID_HANDLE:
         case SEC_E_INVALID_TOKEN:
         case SEC_E_LOGON_DENIED:
         case SEC_E_NO_AUTHENTICATING_AUTHORITY:
         case SEC_E_NO_CREDENTIALS:
         case SEC_E_TARGET_UNKNOWN:
         case SEC_E_UNSUPPORTED_FUNCTION:
         case SEC_E_APPLICATION_PROTOCOL_MISMATCH:


         default: {
            LPTSTR msg = NULL;
            FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_ARGUMENT_ARRAY,
                           NULL, GetLastError(), LANG_NEUTRAL, (LPTSTR)&msg, 0, NULL );
            MONGOC_ERROR ("Failed to initialize security context, error code: 0x%04X%04X: ",
                          (sspi_status >> 16) & 0xffff, sspi_status & 0xffff, msg);
            LocalFree (msg);
         }
         }
         return false;
      }

      /* check if there was additional remaining encrypted data */
      if (inbuf[1].BufferType == SECBUFFER_EXTRA && inbuf[1].cbBuffer > 0) {
         TRACE ("encrypted data length: %lu", inbuf[1].cbBuffer);

         /*
          * There are two cases where we could be getting extra data here:
          * 1) If we're renegotiating a connection and the handshake is already
          * complete (from the server perspective), it can encrypted app data
          * (not handshake data) in an extra buffer at this point.
          * 2) (sspi_status == SEC_I_CONTINUE_NEEDED) We are negotiating a
          * connection and this extra data is part of the handshake.
          * We should process the data immediately; waiting for the socket to
          * be ready may fail since the server is done sending handshake data.
          */
         /* check if the remaining data is less than the total amount
          * and therefore begins after the already processed data */
         if (secure_channel->encdata_offset > inbuf[1].cbBuffer) {
            memmove (secure_channel->encdata_buffer,
                     (secure_channel->encdata_buffer +
                      secure_channel->encdata_offset) -
                     inbuf[1].cbBuffer, inbuf[1].cbBuffer);
            secure_channel->encdata_offset = inbuf[1].cbBuffer;

            if (sspi_status == SEC_I_CONTINUE_NEEDED) {
               doread = FALSE;
               continue;
            }
         }
      }else  {
         secure_channel->encdata_offset = 0;
      }

      break;
   }

   /* check if the handshake needs to be continued */
   if (sspi_status == SEC_I_CONTINUE_NEEDED) {
      secure_channel->connecting_state = ssl_connect_2_reading;
      return true;
   }

   /* check if the handshake is complete */
   if (sspi_status == SEC_E_OK) {
      secure_channel->connecting_state = ssl_connect_3;
      TRACE ("SSL/TLS handshake complete\n");
   }

   return true;
}

bool
mongoc_secure_channel_handshake_step_3 (mongoc_stream_tls_t *tls,
                                        char                *hostname)
{
   mongoc_stream_tls_secure_channel_t *secure_channel = (mongoc_stream_tls_secure_channel_t *)tls->ctx;

   BSON_ASSERT (ssl_connect_3 == secure_channel->connecting_state);

   TRACE ("SSL/TLS connection with %s (step 3/3)\n", hostname);

   if (!secure_channel->cred) {
      return false;
   }

   /* check if the required context attributes are met */
   if (secure_channel->ret_flags != secure_channel->req_flags) {
      MONGOC_ERROR ("Failed handshake");

      return false;
   }

   secure_channel->connecting_state = ssl_connect_done;

   return true;
}
#endif

