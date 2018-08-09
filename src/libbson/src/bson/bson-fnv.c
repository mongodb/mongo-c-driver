/*
 * Please do not copyright this code.  This code is in the public domain.
 *
 * LANDON CURT NOLL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO
 * EVENT SHALL LANDON CURT NOLL BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * By:
 *  chongo <Landon Curt Noll> /\oo/\
 *      http://www.isthe.com/chongo/
 */

/*
 * Sources: http://www.isthe.com/chongo/src/fnv/hash_32a.c
 *          http://www.isthe.com/chongo/tech/comp/fnv/index.html#xor-fold
 */

#include "bson/bson-fnv-private.h"

/*
 * 32 bit FNV-1a non-zero initial basis
 *
 * The FNV-1 initial basis is the FNV-0 hash of the following 32 octets:
 *
 *              chongo <Landon Curt Noll> /\../\
 *
 * NOTE: The \'s above are not back-slashing escape characters.
 * They are literal ASCII  backslash 0x5c characters.
 *
 * NOTE: The FNV-1a initial basis is the same value as FNV-1 by definition.
 */
#define FNV1_32A_INIT ((uint32_t) 0x811c9dc5)

/*
 * 32 bit magic FNV-1a prime
 * NOTE: define NO_FNV_GCC_OPTIMIZATION to use this prime
 */
#define FNV_32_PRIME ((Fnv32_t) 0x01000193)

/*
 * For producing 24 bit FNV-1a hash using xor-fold on a 32 bit FNV-1a hash
 */
#define MASK_24 (((uint32_t) 1 << 24) - 1) /* i.e., (uint32_t) 0xffffff */

/*---------------------------------------------------------------------------
 *
 * _mongoc_fnv_24a_str --
 *
 *       perform a 32 bit Fowler/Noll/Vo FNV-1a hash on a string and
 *       xor-fold it into a 24 bit hash
 *
 * Return:
 *       24 bit hash as a static hash type
 *
 * Note:
 *       input strings with multiple null characters will stop being
 *       processed at the first null
 *
 *--------------------------------------------------------------------------
 */
uint32_t
_mongoc_fnv_24a_str (char *str)
{
   uint32_t hval = FNV1_32A_INIT;            /* initial 32 bit hash basis */
   unsigned char *s = (unsigned char *) str; /* unsigned string */

   /* FNV-1a hash each octet in the buffer */
   while (*s) {
      /* xor the bottom with the current octet */
      hval ^= (uint32_t) *s++;

/* multiply by the 32 bit FNV magic prime mod 2^32 */
#if defined(NO_FNV_GCC_OPTIMIZATION)
      hval *= FNV_32_PRIME;
#else
      hval +=
         (hval << 1) + (hval << 4) + (hval << 7) + (hval << 8) + (hval << 24);
#endif
   }

   /* xor-fold the result to a 24 bit value */
   hval = (hval >> 24) ^ (hval & MASK_24);

   /* return our new 24 bit hash value */
   return hval;
}
