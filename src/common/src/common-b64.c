/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <common-b64-private.h>
#include <common-thread-private.h>

#include <bson/bson.h>

#include <mlib/config.h>
#include <mlib/loop.h>

#define Assert(Cond) \
   if (!(Cond))      \
   abort ()

static const char Base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
 * The following encoding technique is taken from RFC 1521 by Borenstein
 * and Freed.  It is reproduced here in a slightly edited form for
 * convenience.
 *
 * A 65-character subset of US-ASCII is used, enabling 6 bits to be
 * represented per printable character. (The extra 65th character, "=",
 * is used to signify a special processing function.)
 *
 * The encoding process represents 24-bit groups of input bits as output
 * strings of 4 encoded characters. Proceeding from left to right, a
 * 24-bit input group is formed by concatenating 3 8-bit input groups.
 * These 24 bits are then treated as 4 concatenated 6-bit groups, each
 * of which is translated into a single digit in the base64 alphabet.
 *
 * Each 6-bit group is used as an index into an array of 64 printable
 * characters. The character referenced by the index is placed in the
 * output string.
 *
 *                       Table 1: The Base64 Alphabet
 *
 *    Value Encoding  Value Encoding  Value Encoding  Value Encoding
 *        0 A            17 R            34 i            51 z
 *        1 B            18 S            35 j            52 0
 *        2 C            19 T            36 k            53 1
 *        3 D            20 U            37 l            54 2
 *        4 E            21 V            38 m            55 3
 *        5 F            22 W            39 n            56 4
 *        6 G            23 X            40 o            57 5
 *        7 H            24 Y            41 p            58 6
 *        8 I            25 Z            42 q            59 7
 *        9 J            26 a            43 r            60 8
 *       10 K            27 b            44 s            61 9
 *       11 L            28 c            45 t            62 +
 *       12 M            29 d            46 u            63 /
 *       13 N            30 e            47 v
 *       14 O            31 f            48 w         (pad) =
 *       15 P            32 g            49 x
 *       16 Q            33 h            50 y
 *
 * Special processing is performed if fewer than 24 bits are available
 * at the end of the data being encoded.  A full encoding quantum is
 * always completed at the end of a quantity.  When fewer than 24 input
 * bits are available in an input group, zero bits are added (on the
 * right) to form an integral number of 6-bit groups.  Padding at the
 * end of the data is performed using the '=' character.
 *
 * Since all base64 input is an integral number of octets, only the
 * following cases can arise:
 *
 *     (1) the final quantum of encoding input is an integral
 *         multiple of 24 bits; here, the final unit of encoded
 *    output will be an integral multiple of 4 characters
 *    with no "=" padding,
 *     (2) the final quantum of encoding input is exactly 8 bits;
 *         here, the final unit of encoded output will be two
 *    characters followed by two "=" padding characters, or
 *     (3) the final quantum of encoding input is exactly 16 bits;
 *         here, the final unit of encoded output will be three
 *    characters followed by one "=" padding character.
 */

int
mcommon_b64_ntop (uint8_t const *src, size_t srclength, char *target, size_t targsize)
{
   size_t datalength = 0;
   uint8_t input[3];
   uint8_t output[4];

   if (!target) {
      return -1;
   }

   // While we have at least three chars to read:
   while (srclength > 2) {
      input[0] = *src++;
      input[1] = *src++;
      input[2] = *src++;
      srclength -= 3;

      output[0] = input[0] >> 2;
      output[1] = (uint8_t) (((input[0] & 0x03) << 4) + (input[1] >> 4));
      output[2] = (uint8_t) (((input[1] & 0x0f) << 2) + (input[2] >> 6));
      output[3] = input[2] & 0x3f;
      Assert (output[0] < 64);
      Assert (output[1] < 64);
      Assert (output[2] < 64);
      Assert (output[3] < 64);

      if (datalength + 4 > targsize) {
         return -1;
      }
      target[datalength++] = Base64[output[0]];
      target[datalength++] = Base64[output[1]];
      target[datalength++] = Base64[output[2]];
      target[datalength++] = Base64[output[3]];
   }

   /* Now we worry about padding. */
   if (0 != srclength) {
      /* Get what's left. */
      input[0] = input[1] = input[2] = '\0';

      memcpy (input, src, srclength);
      output[0] = input[0] >> 2;
      output[1] = (uint8_t) (((input[0] & 0x03) << 4) + (input[1] >> 4));
      output[2] = (uint8_t) (((input[1] & 0x0f) << 2) + (input[2] >> 6));
      Assert (output[0] < 64);
      Assert (output[1] < 64);
      Assert (output[2] < 64);

      if (datalength + 4 > targsize) {
         return -1;
      }
      target[datalength++] = Base64[output[0]];
      target[datalength++] = Base64[output[1]];

      if (srclength == 1) {
         target[datalength++] = Pad64;
      } else {
         target[datalength++] = Base64[output[2]];
      }
      target[datalength++] = Pad64;
   }

   if (datalength >= targsize) {
      return -1;
   }
   target[datalength] = '\0'; /* Returned value doesn't count \0. */
   return (int) datalength;
}

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

                         Table 1: The Base64 Alphabet

      Value Encoding  Value Encoding  Value Encoding  Value Encoding
          0 A            17 R            34 i            51 z
          1 B            18 S            35 j            52 0
          2 C            19 T            36 k            53 1
          3 D            20 U            37 l            54 2
          4 E            21 V            38 m            55 3
          5 F            22 W            39 n            56 4
          6 G            23 X            40 o            57 5
          7 H            24 Y            41 p            58 6
          8 I            25 Z            42 q            59 7
          9 J            26 a            43 r            60 8
         10 K            27 b            44 s            61 9
         11 L            28 c            45 t            62 +
         12 M            29 d            46 u            63 /
         13 N            30 e            47 v
         14 O            31 f            48 w         (pad) =
         15 P            32 g            49 x
         16 Q            33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
   following cases can arise:

       (1) the final quantum of encoding input is an integral
           multiple of 24 bits; here, the final unit of encoded
      output will be an integral multiple of 4 characters
      with no "=" padding,
       (2) the final quantum of encoding input is exactly 8 bits;
           here, the final unit of encoded output will be two
      characters followed by two "=" padding characters, or
       (3) the final quantum of encoding input is exactly 16 bits;
           here, the final unit of encoded output will be three
      characters followed by one "=" padding character.
   */

/* skips all whitespace anywhere.
   converts characters, four at a time, starting at (or after)
   src from base - 64 numbers into three 8 bit bytes in the target area.
   it returns the number of data bytes stored at the target, or -1 on error.
 */

static uint8_t mongoc_b64rmap[256];

static const uint8_t mongoc_b64rmap_special = 0xf0;
static const uint8_t mongoc_b64rmap_end = 0xfd;
static const uint8_t mongoc_b64rmap_space = 0xfe;
static const uint8_t mongoc_b64rmap_invalid = 0xff;

/* initializing the reverse map isn't thread safe, do it in pthread_once */
static BSON_ONCE_FUN (bson_b64_initialize_rmap)
{
   /* Null: end of string, stop parsing */
   mongoc_b64rmap[0] = mongoc_b64rmap_end;

   mlib_foreach_urange (i, 1, 256) {
      const uint8_t ch = (uint8_t) i;
      /* Whitespaces */
      if (bson_isspace (ch))
         mongoc_b64rmap[ch] = mongoc_b64rmap_space;
      /* Padding: stop parsing */
      else if (ch == Pad64)
         mongoc_b64rmap[ch] = mongoc_b64rmap_end;
      /* Non-base64 char */
      else
         mongoc_b64rmap[ch] = mongoc_b64rmap_invalid;
   }

   /* Fill reverse mapping for base64 chars */
   for (uint8_t i = 0; Base64[i] != '\0'; ++i)
      mongoc_b64rmap[(uint8_t) Base64[i]] = i;

   BSON_ONCE_RETURN;
}

static int
mongoc_b64_pton_do (char const *src, uint8_t *target, size_t targsize)
{
   int tarindex, state;
   uint8_t ch, ofs;

   state = 0;
   tarindex = 0;

   while (1) {
      ch = (uint8_t) *src++;
      ofs = mongoc_b64rmap[ch];

      if (ofs >= mongoc_b64rmap_special) {
         /* Ignore whitespaces */
         if (ofs == mongoc_b64rmap_space)
            continue;
         /* End of base64 characters */
         if (ofs == mongoc_b64rmap_end)
            break;
         /* A non-base64 character. */
         return (-1);
      }

      switch (state) {
      case 0:
         if ((size_t) tarindex >= targsize)
            return (-1);
         target[tarindex] = ofs << 2;
         state = 1;
         break;
      case 1:
         if ((size_t) tarindex + 1 >= targsize)
            return (-1);
         target[tarindex] |= ofs >> 4;
         target[tarindex + 1] = (ofs & 0x0f) << 4;
         tarindex++;
         state = 2;
         break;
      case 2:
         if ((size_t) tarindex + 1 >= targsize)
            return (-1);
         target[tarindex] |= ofs >> 2;
         target[tarindex + 1] = (ofs & 0x03) << 6;
         tarindex++;
         state = 3;
         break;
      case 3:
         if ((size_t) tarindex >= targsize)
            return (-1);
         target[tarindex] |= ofs;
         tarindex++;
         state = 0;
         break;
      default:
         abort ();
      }
   }

   /*
    * We are done decoding Base-64 chars.  Let's see if we ended
    * on a byte boundary, and/or with erroneous trailing characters.
    */

   if (ch == Pad64) {        /* We got a pad char. */
      ch = (uint8_t) *src++; /* Skip it, get next. */
      switch (state) {
      case 0: /* Invalid = in first position */
      case 1: /* Invalid = in second position */
         return (-1);

      case 2: /* Valid, means one byte of info */
         /* Skip any number of spaces. */
         for ((void) NULL; ch != '\0'; ch = (uint8_t) *src++)
            if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
               break;
         /* Make sure there is another trailing = sign. */
         if (ch != Pad64)
            return (-1);
         ch = (uint8_t) *src++; /* Skip the = */
         /* Fall through to "single trailing =" case. */
         /* FALLTHROUGH */

      case 3: /* Valid, means two bytes of info */
         /*
          * We know this char is an =.  Is there anything but
          * whitespace after it?
          */
         for ((void) NULL; ch != '\0'; ch = (uint8_t) *src++)
            if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
               return (-1);

         /*
          * Now make sure for cases 2 and 3 that the "extra"
          * bits that slopped past the last full byte were
          * zeros.  If we don't check them, they become a
          * subliminal channel.
          */
         if (target[tarindex] != 0)
            return (-1);
      default:
         break;
      }
   } else {
      /*
       * We ended by seeing the end of the string.  Make sure we
       * have no partial bytes lying around.
       */
      if (state != 0)
         return (-1);
   }

   return (tarindex);
}


static int
mongoc_b64_pton_len (char const *src)
{
   uint8_t ch = 0;
   uint8_t ofs = 0;
   int state = 0;
   int tarindex = 0;

   while (1) {
      ch = (uint8_t) *src++;
      ofs = mongoc_b64rmap[ch];

      if (ofs >= mongoc_b64rmap_special) {
         /* Ignore whitespaces */
         if (ofs == mongoc_b64rmap_space)
            continue;
         /* End of base64 characters */
         if (ofs == mongoc_b64rmap_end)
            break;
         /* A non-base64 character. */
         return (-1);
      }

      switch (state) {
      case 0:
         state = 1;
         break;
      case 1:
         tarindex++;
         state = 2;
         break;
      case 2:
         tarindex++;
         state = 3;
         break;
      case 3:
         tarindex++;
         state = 0;
         break;
      default:
         abort ();
      }
   }

   /*
    * We are done decoding Base-64 chars.  Let's see if we ended
    * on a byte boundary, and/or with erroneous trailing characters.
    */

   if (ch == Pad64) {        /* We got a pad char. */
      ch = (uint8_t) *src++; /* Skip it, get next. */
      switch (state) {
      case 0: /* Invalid = in first position */
      case 1: /* Invalid = in second position */
         return (-1);

      case 2: /* Valid, means one byte of info */
         /* Skip any number of spaces. */
         for ((void) NULL; ch != '\0'; ch = (uint8_t) *src++)
            if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
               break;
         /* Make sure there is another trailing = sign. */
         if (ch != Pad64)
            return (-1);
         ch = (uint8_t) *src++; /* Skip the = */
         /* Fall through to "single trailing =" case. */
         /* FALLTHROUGH */

      case 3: /* Valid, means two bytes of info */
         /*
          * We know this char is an =.  Is there anything but
          * whitespace after it?
          */
         for (; ch != '\0'; ch = (uint8_t) *src++)
            if (mongoc_b64rmap[ch] != mongoc_b64rmap_space)
               return (-1);

      default:
         break;
      }
   } else {
      /*
       * We ended by seeing the end of the string.  Make sure we
       * have no partial bytes lying around.
       */
      if (state != 0)
         return (-1);
   }

   return (tarindex);
}


int
mcommon_b64_pton (char const *src, uint8_t *target, size_t targsize)
{
   static bson_once_t once = BSON_ONCE_INIT;

   bson_once (&once, bson_b64_initialize_rmap);

   if (!src) {
      return -1;
   }

   if (target)
      return mongoc_b64_pton_do (src, target, targsize);
   else
      return mongoc_b64_pton_len (src);
}

size_t
mcommon_b64_ntop_calculate_target_size (size_t raw_size)
{
   size_t num_bits = raw_size * 8;
   /* Calculate how many groups of six bits this contains, adding 5 to round up
    * to the nearest group of 6. */
   size_t num_b64_chars = (num_bits + 5) / 6;
   /* Round to nearest set of four. */
   size_t num_b64_chars_with_padding = 4 * ((num_b64_chars + 3) / 4);
   /* Add one for NULL byte. */
   return num_b64_chars_with_padding + 1;
}

size_t
mcommon_b64_pton_calculate_target_size (size_t base64_encoded_size)
{
   /* Without inspecting the data, we don't know how many padding characters
    * there are. Assuming none, that means each character represents 6 bits of
    * data. */
   size_t num_bits = base64_encoded_size * 6;
   /* Round down to the nearest group of eight. */
   return num_bits / 8;
}
