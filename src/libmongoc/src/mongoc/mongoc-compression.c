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


#include <common-thread-private.h>
#include <mongoc/mongoc-compression-private.h>
#include <mongoc/mongoc-trace-private.h>
#include <mongoc/mongoc-util-private.h>

#include <mongoc/mongoc-config.h>

#include <mlib/cmp.h>

#ifdef MONGOC_ENABLE_COMPRESSION
#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
#include <zlib.h>
#endif
#ifdef MONGOC_ENABLE_COMPRESSION_SNAPPY
#include <snappy-c.h>
#endif
#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
#include <zstd.h>
#endif
#endif

#if defined(MONGOC_ENABLE_COMPRESSION_ZLIB) || defined(MONGOC_ENABLE_COMPRESSION_ZSTD)
/* Compression contexts are kept for reuse because creating one allocates and
 * initializes hundreds of KiB of state, which dominates the cost of a small
 * message. Each pool hands a context to one caller at a time. */
#define COMPRESSION_POOL_CAPACITY 16

typedef struct {
   bson_mutex_t lock;
   void *contexts[COMPRESSION_POOL_CAPACITY];
   size_t count;
} compression_pool_t;

static compression_pool_t *
_compression_pool_init(compression_pool_t *pool)
{
   bson_mutex_init(&pool->lock);
   pool->count = 0u;
   return pool;
}

static void *
_compression_pool_take(compression_pool_t *pool)
{
   void *context = NULL;

   bson_mutex_lock(&pool->lock);
   if (pool->count > 0u) {
      context = pool->contexts[--pool->count];
   }
   bson_mutex_unlock(&pool->lock);

   return context;
}

/* Returns false when the pool is full, in which case the caller frees the context. */
static bool
_compression_pool_give(compression_pool_t *pool, void *context)
{
   bool kept = false;

   bson_mutex_lock(&pool->lock);
   if (pool->count < COMPRESSION_POOL_CAPACITY) {
      pool->contexts[pool->count++] = context;
      kept = true;
   }
   bson_mutex_unlock(&pool->lock);

   return kept;
}
#endif

#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
typedef struct {
   z_stream stream;
   int32_t level;
} deflater_t;

static compression_pool_t _deflaters;

static void
_deflater_destroy(deflater_t *deflater)
{
   deflateEnd(&deflater->stream);
   bson_free(deflater);
}

static bool
_zlib_compress(
   int32_t level, const char *uncompressed, size_t uncompressed_len, char *compressed, size_t *compressed_len)
{
   if (BSON_UNLIKELY(!mlib_in_range(unsigned int, uncompressed_len) || !mlib_in_range(unsigned int, *compressed_len))) {
      return false;
   }

   deflater_t *deflater = _compression_pool_take(&_deflaters);
   if (deflater && deflater->level != level) {
      _deflater_destroy(deflater);
      deflater = NULL;
   }
   if (!deflater) {
      deflater = bson_malloc0(sizeof *deflater);
      if (deflateInit(&deflater->stream, level) != Z_OK) {
         bson_free(deflater);
         return false;
      }
      deflater->level = level;
   } else if (deflateReset(&deflater->stream) != Z_OK) {
      _deflater_destroy(deflater);
      return false;
   }

   deflater->stream.next_in = (Bytef *)uncompressed;
   deflater->stream.avail_in = (uInt)uncompressed_len;
   deflater->stream.next_out = (Bytef *)compressed;
   deflater->stream.avail_out = (uInt)*compressed_len;

   if (deflate(&deflater->stream, Z_FINISH) != Z_STREAM_END) {
      _deflater_destroy(deflater);
      return false;
   }
   *compressed_len = (size_t)deflater->stream.total_out;

   if (!_compression_pool_give(&_deflaters, deflater)) {
      _deflater_destroy(deflater);
   }
   return true;
}
#endif

#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
static compression_pool_t _zstd_compressors;
static compression_pool_t _zstd_decompressors;
#endif

static BSON_ONCE_FUN(_mongoc_compression_pools_init)
{
#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
   _compression_pool_init(&_deflaters);
#endif
#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
   _compression_pool_init(&_zstd_compressors);
   _compression_pool_init(&_zstd_decompressors);
#endif
   BSON_ONCE_RETURN;
}

static bson_once_t _mongoc_compression_pools_once = BSON_ONCE_INIT;

static void
_mongoc_compression_pools_ensure(void)
{
   bson_once(&_mongoc_compression_pools_once, _mongoc_compression_pools_init);
}

void
_mongoc_compression_cleanup(void)
{
   _mongoc_compression_pools_ensure();
#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
   for (void *deflater; (deflater = _compression_pool_take(&_deflaters));) {
      _deflater_destroy(deflater);
   }
   bson_mutex_destroy(&_deflaters.lock);
#endif
#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
   for (void *cctx; (cctx = _compression_pool_take(&_zstd_compressors));) {
      ZSTD_freeCCtx(cctx);
   }
   for (void *dctx; (dctx = _compression_pool_take(&_zstd_decompressors));) {
      ZSTD_freeDCtx(dctx);
   }
   bson_mutex_destroy(&_zstd_compressors.lock);
   bson_mutex_destroy(&_zstd_decompressors.lock);
#endif
}

size_t
mongoc_compressor_max_compressed_length(int32_t compressor_id, size_t len)
{
   TRACE("Getting compression length for '%s' (%d)", mongoc_compressor_id_to_name(compressor_id), compressor_id);
   switch (compressor_id) {
#ifdef MONGOC_ENABLE_COMPRESSION_SNAPPY
   case MONGOC_COMPRESSOR_SNAPPY_ID:
      return snappy_max_compressed_length(len);
#endif

#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
   case MONGOC_COMPRESSOR_ZLIB_ID:
      BSON_ASSERT(mlib_in_range(unsigned long, len));
      return compressBound((unsigned long)len);
#endif

#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
   case MONGOC_COMPRESSOR_ZSTD_ID:
      return ZSTD_compressBound(len);
#endif
   case MONGOC_COMPRESSOR_NOOP_ID:
      return len;
   default:
      return 0;
   }
}

bool
mongoc_compressor_supported(mstr_view compressor)
{
   bool have_snappy = false, have_zlib = false, have_zstd = false;
#ifdef MONGOC_ENABLE_COMPRESSION_SNAPPY
   have_snappy = true;
#endif
#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
   have_zlib = true;
#endif
#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
   have_zstd = true;
#endif

   if (mstr_latin_casecmp(compressor, ==, mstr_cstring("snappy"))) {
      return have_snappy;
   }
   if (mstr_latin_casecmp(compressor, ==, mstr_cstring("zlib"))) {
      return have_zlib;
   }
   if (mstr_latin_casecmp(compressor, ==, mstr_cstring("zstd"))) {
      return have_zstd;
   }
   if (mstr_latin_casecmp(compressor, ==, mstr_cstring("noop"))) {
      return true; // We always have "noop"
   }

   // Any other compressor name is unrecognized
   return false;
}

const char *
mongoc_compressor_id_to_name(int32_t compressor_id)
{
   switch (compressor_id) {
   case MONGOC_COMPRESSOR_SNAPPY_ID:
      return MONGOC_COMPRESSOR_SNAPPY_STR;

   case MONGOC_COMPRESSOR_ZLIB_ID:
      return MONGOC_COMPRESSOR_ZLIB_STR;

   case MONGOC_COMPRESSOR_ZSTD_ID:
      return MONGOC_COMPRESSOR_ZSTD_STR;

   case MONGOC_COMPRESSOR_NOOP_ID:
      return MONGOC_COMPRESSOR_NOOP_STR;

   default:
      return "unknown";
   }
}

int
mongoc_compressor_name_to_id(const char *compressor)
{
#ifdef MONGOC_ENABLE_COMPRESSION_SNAPPY
   if (strcasecmp(MONGOC_COMPRESSOR_SNAPPY_STR, compressor) == 0) {
      return MONGOC_COMPRESSOR_SNAPPY_ID;
   }
#endif

#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
   if (strcasecmp(MONGOC_COMPRESSOR_ZLIB_STR, compressor) == 0) {
      return MONGOC_COMPRESSOR_ZLIB_ID;
   }
#endif

#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
   if (strcasecmp(MONGOC_COMPRESSOR_ZSTD_STR, compressor) == 0) {
      return MONGOC_COMPRESSOR_ZSTD_ID;
   }
#endif

   if (strcasecmp(MONGOC_COMPRESSOR_NOOP_STR, compressor) == 0) {
      return MONGOC_COMPRESSOR_NOOP_ID;
   }

   return -1;
}

// To support unchecked casts from `unsigned long` to `size_t`.
BSON_STATIC_ASSERT2(size_t_gte_ulong, SIZE_MAX >= ULONG_MAX);

bool
mongoc_uncompress(int32_t compressor_id,
                  const uint8_t *compressed,
                  size_t compressed_len,
                  uint8_t *uncompressed,
                  size_t *uncompressed_len)
{
   BSON_ASSERT_PARAM(compressed);
   BSON_ASSERT_PARAM(uncompressed);
   BSON_ASSERT_PARAM(uncompressed_len);

   TRACE("Uncompressing with '%s' (%d)", mongoc_compressor_id_to_name(compressor_id), compressor_id);

   switch (compressor_id) {
   case MONGOC_COMPRESSOR_SNAPPY_ID: {
#ifdef MONGOC_ENABLE_COMPRESSION_SNAPPY
      const snappy_status status =
         snappy_uncompress((const char *)compressed, compressed_len, (char *)uncompressed, uncompressed_len);

      return status == SNAPPY_OK;
#else
      MONGOC_WARNING("Received snappy compressed opcode, but snappy "
                     "compression is not compiled in");
      return false;
#endif
   }

   case MONGOC_COMPRESSOR_ZLIB_ID: {
#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
      // Malformed message: unrepresentable.
      if (BSON_UNLIKELY(!mlib_in_range(unsigned long, compressed_len))) {
         return false;
      }

      // Malformed message: unrepresentable.
      if (BSON_UNLIKELY(!mlib_in_range(unsigned long, *uncompressed_len))) {
         return false;
      }

      uLong actual_uncompressed_len = (uLong)*uncompressed_len;

      const int res =
         uncompress(uncompressed, &actual_uncompressed_len, (const Bytef *)compressed, (uLong)compressed_len);

      if (BSON_UNLIKELY(res != Z_OK)) {
         return false;
      }

      *uncompressed_len = (size_t)actual_uncompressed_len;

      return true;
#else
      MONGOC_WARNING("Received zlib compressed opcode, but zlib "
                     "compression is not compiled in");
      return false;
#endif
   }

   case MONGOC_COMPRESSOR_ZSTD_ID: {
#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
      _mongoc_compression_pools_ensure();
      ZSTD_DCtx *dctx = _compression_pool_take(&_zstd_decompressors);
      if (!dctx && !(dctx = ZSTD_createDCtx())) {
         return false;
      }

      const size_t res = ZSTD_decompressDCtx(dctx, uncompressed, *uncompressed_len, compressed, compressed_len);

      if (!_compression_pool_give(&_zstd_decompressors, dctx)) {
         ZSTD_freeDCtx(dctx);
      }

      if (BSON_UNLIKELY(ZSTD_isError(res))) {
         return false;
      }

      *uncompressed_len = res;

      return true;
#else
      MONGOC_WARNING("Received zstd compressed opcode, but zstd "
                     "compression is not compiled in");
      return false;
#endif
   }
   case MONGOC_COMPRESSOR_NOOP_ID:
      // Malformed message: not enough space.
      if (BSON_UNLIKELY(*uncompressed_len < compressed_len)) {
         return false;
      }
      memcpy(uncompressed, compressed, compressed_len);
      *uncompressed_len = compressed_len;
      return true;

   default:
      MONGOC_WARNING("Unknown compressor ID %d", compressor_id);
   }

   return false;
}

bool
mongoc_compress(int32_t compressor_id,
                int32_t compression_level,
                char *uncompressed,
                size_t uncompressed_len,
                char *compressed,
                size_t *compressed_len)
{
   TRACE("Compressing with '%s' (%d)", mongoc_compressor_id_to_name(compressor_id), compressor_id);
   switch (compressor_id) {
   case MONGOC_COMPRESSOR_SNAPPY_ID:
#ifdef MONGOC_ENABLE_COMPRESSION_SNAPPY
      /* No compression_level option for snappy */
      return snappy_compress(uncompressed, uncompressed_len, compressed, compressed_len) == SNAPPY_OK;
#else
      MONGOC_ERROR("Client attempting to use compress with snappy, but snappy "
                   "compression is not compiled in");
      return false;
#endif

   case MONGOC_COMPRESSOR_ZLIB_ID:
#ifdef MONGOC_ENABLE_COMPRESSION_ZLIB
      _mongoc_compression_pools_ensure();
      return _zlib_compress(compression_level, uncompressed, uncompressed_len, compressed, compressed_len);
#else
      MONGOC_ERROR("Client attempting to use compress with zlib, but zlib "
                   "compression is not compiled in");
      return false;
#endif

   case MONGOC_COMPRESSOR_ZSTD_ID: {
#ifdef MONGOC_ENABLE_COMPRESSION_ZSTD
      _mongoc_compression_pools_ensure();
      ZSTD_CCtx *cctx = _compression_pool_take(&_zstd_compressors);
      if (!cctx && !(cctx = ZSTD_createCCtx())) {
         return false;
      }

      const size_t res =
         ZSTD_compressCCtx(cctx, (void *)compressed, *compressed_len, (const void *)uncompressed, uncompressed_len, 0);

      if (!_compression_pool_give(&_zstd_compressors, cctx)) {
         ZSTD_freeCCtx(cctx);
      }

      if (ZSTD_isError(res)) {
         return false;
      }
      *compressed_len = res;
      return true;
#else
      MONGOC_ERROR("Client attempting to use compress with zstd, but zstd "
                   "compression is not compiled in");
      return false;
#endif
   }
   case MONGOC_COMPRESSOR_NOOP_ID:
      memcpy(compressed, uncompressed, uncompressed_len);
      *compressed_len = uncompressed_len;
      return true;

   default:
      return false;
   }
}
