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

#include "bson-fnv-private.h"
#include "TestSuite.h"

/* REPEAT500 - repeat a string 500 times */
#define R500(x) R100 (x) R100 (x) R100 (x) R100 (x) R100 (x)
#define R100(x) \
   R10 (x)      \
   R10 (x) R10 (x) R10 (x) R10 (x) R10 (x) R10 (x) R10 (x) R10 (x) R10 (x)
#define R10(x) x x x x x x x x x x

struct hash_test_vector {
   char *str;     /* start of test vector buffer */
   uint32_t hash; /* length of test vector */
};

/* Source for below tests: http://www.isthe.com/chongo/src/fnv/test_fnv.c */
static void
test_fnv_check_hashes (void)
{
   unsigned i;

   struct hash_test_vector v[] = {
      {"", (uint32_t) 0x1c9d44},
      {"a", (uint32_t) 0x0c29c8},
      {"b", (uint32_t) 0x0c2d02},
      {"c", (uint32_t) 0x0c2cb4},
      {"d", (uint32_t) 0x0c2492},
      {"e", (uint32_t) 0x0c2200},
      {"f", (uint32_t) 0x0c277a},
      {"fo", (uint32_t) 0x22e820},
      {"foo", (uint32_t) 0xf37e7e},
      {"foob", (uint32_t) 0x5076d0},
      {"fooba", (uint32_t) 0xaaa1b3},
      {"foobar", (uint32_t) 0x9cf9d7},
      {"ch", (uint32_t) 0x299f11},
      {"cho", (uint32_t) 0x85801c},
      {"chon", (uint32_t) 0x29778b},
      {"chong", (uint32_t) 0x46b985},
      {"chongo", (uint32_t) 0x564ec0},
      {"chongo ", (uint32_t) 0xdd5c0c},
      {"chongo w", (uint32_t) 0x77eded},
      {"chongo wa", (uint32_t) 0xca9677},
      {"chongo was", (uint32_t) 0xeb9b9a},
      {"chongo was ", (uint32_t) 0xe67a30},
      {"chongo was h", (uint32_t) 0xd32f6a},
      {"chongo was he", (uint32_t) 0x743fc8},
      {"chongo was her", (uint32_t) 0x006376},
      {"chongo was here", (uint32_t) 0x9c99cb},
      {"chongo was here!", (uint32_t) 0x8524b9},
      {"chongo was here!\n", (uint32_t) 0x993001},
      {"cu", (uint32_t) 0x298129},
      {"cur", (uint32_t) 0x5637c9},
      {"curd", (uint32_t) 0xb9140f},
      {"curds", (uint32_t) 0x5bf5a7},
      {"curds ", (uint32_t) 0xc42805},
      {"curds a", (uint32_t) 0xcc0e97},
      {"curds an", (uint32_t) 0x3b4c5d},
      {"curds and", (uint32_t) 0x59f0a7},
      {"curds and ", (uint32_t) 0x94de0b},
      {"curds and w", (uint32_t) 0x5a0a72},
      {"curds and wh", (uint32_t) 0xbee56f},
      {"curds and whe", (uint32_t) 0x8363fd},
      {"curds and whey", (uint32_t) 0xd5346c},
      {"curds and whey\n", (uint32_t) 0xa14715},
      {"hi", (uint32_t) 0x3af6f2},
      {"hello", (uint32_t) 0x9f2ce4},
      {"\x40\x51\x4e\x44", (uint32_t) 0x17906a},
      {"\x44\x4e\x51\x40", (uint32_t) 0x0bfece},
      {"\x40\x51\x4e\x4a", (uint32_t) 0x178d02},
      {"\x4a\x4e\x51\x40", (uint32_t) 0xaddad9},
      {"\x40\x51\x4e\x54", (uint32_t) 0x17a9ca},
      {"\x54\x4e\x51\x40", (uint32_t) 0x2633a1},
      {"127.0.0.1", (uint32_t) 0xa3d116},
      {"127.0.0.2", (uint32_t) 0xa3cf8c},
      {"127.0.0.3", (uint32_t) 0xa3cdfe},
      {"64.81.78.68", (uint32_t) 0x5636ba},
      {"64.81.78.74", (uint32_t) 0x53e841},
      {"64.81.78.84", (uint32_t) 0x5b8948},
      {"feedface", (uint32_t) 0x88b139},
      {"feedfacedaffdeed", (uint32_t) 0x364109},
      {"feedfacedeadbeef", (uint32_t) 0x7604b9},
      {"line 1\nline 2\nline 3", (uint32_t) 0xb4eab4},
      {"chongo <Landon Curt Noll> /\\../\\", (uint32_t) 0x4e927c},
      {"chongo (Landon Curt Noll) /\\../\\", (uint32_t) 0x1b25e1},
      {"Evgeni was here :D", (uint32_t) 0xebf05e},
      {"http://antwrp.gsfc.nasa.gov/apod/astropix.html", (uint32_t) 0x524a34},
      {"http://en.wikipedia.org/wiki/Fowler_Noll_Vo_hash", (uint32_t) 0x16ef98},
      {"http://epod.usra.edu/", (uint32_t) 0x648bd3},
      {"http://exoplanet.eu/", (uint32_t) 0xa4bc83},
      {"http://hvo.wr.usgs.gov/cam3/", (uint32_t) 0x53ae47},
      {"http://hvo.wr.usgs.gov/cams/HMcam/", (uint32_t) 0x302859},
      {"http://hvo.wr.usgs.gov/kilauea/update/deformation.html",
       (uint32_t) 0x6deda7},
      {"http://hvo.wr.usgs.gov/kilauea/update/images.html",
       (uint32_t) 0x36db15},
      {"http://hvo.wr.usgs.gov/kilauea/update/maps.html", (uint32_t) 0x9d33fc},
      {"http://hvo.wr.usgs.gov/volcanowatch/current_issue.html",
       (uint32_t) 0xbb6ce2},
      {"http://neo.jpl.nasa.gov/risk/", (uint32_t) 0xf83893},
      {"http://norvig.com/21-days.html", (uint32_t) 0x08bf51},
      {"http://primes.utm.edu/curios/home.php", (uint32_t) 0xcc8e5f},
      {"http://slashdot.org/", (uint32_t) 0xe20f9f},
      {"http://tux.wr.usgs.gov/Maps/155.25-19.5.html", (uint32_t) 0xe97f2e},
      {"http://volcano.wr.usgs.gov/kilaueastatus.php", (uint32_t) 0x37b27b},
      {"http://www.avo.alaska.edu/activity/Redoubt.php", (uint32_t) 0x9e874a},
      {"http://www.dilbert.com/fast/", (uint32_t) 0xe63f5a},
      {"http://www.fourmilab.ch/gravitation/orbits/", (uint32_t) 0xb50b11},
      {"http://www.fpoa.net/", (uint32_t) 0xd678e6},
      {"http://www.ioccc.org/index.html", (uint32_t) 0xd5b723},
      {"http://www.isthe.com/cgi-bin/number.cgi", (uint32_t) 0x450bb7},
      {"http://www.isthe.com/chongo/bio.html", (uint32_t) 0x72d79d},
      {"http://www.isthe.com/chongo/index.html", (uint32_t) 0x06679c},
      {"http://www.isthe.com/chongo/src/calc/lucas-calc", (uint32_t) 0x52e15c},
      {"http://www.isthe.com/chongo/tech/astro/venus2004.html",
       (uint32_t) 0x9664f7},
      {"http://www.isthe.com/chongo/tech/astro/vita.html", (uint32_t) 0x3258b6},
      {"http://www.isthe.com/chongo/tech/comp/c/expert.html",
       (uint32_t) 0xed6ea7},
      {"http://www.isthe.com/chongo/tech/comp/calc/index.html",
       (uint32_t) 0x7d7ce2},
      {"http://www.isthe.com/chongo/tech/comp/fnv/index.html",
       (uint32_t) 0xc71ba1},
      {"http://www.isthe.com/chongo/tech/math/number/howhigh.html",
       (uint32_t) 0x84f14b},
      {"http://www.isthe.com/chongo/tech/math/number/number.html",
       (uint32_t) 0x8ecf2e},
      {"http://www.isthe.com/chongo/tech/math/prime/mersenne.html",
       (uint32_t) 0x94f673},
      {"http://www.isthe.com/chongo/tech/math/prime/mersenne.html#largest",
       (uint32_t) 0x970112},
      {"http://www.lavarnd.org/cgi-bin/corpspeak.cgi", (uint32_t) 0x6e172a},
      {"http://www.lavarnd.org/cgi-bin/haiku.cgi", (uint32_t) 0xf8f6e7},
      {"http://www.lavarnd.org/cgi-bin/rand-none.cgi", (uint32_t) 0xf58843},
      {"http://www.lavarnd.org/cgi-bin/randdist.cgi", (uint32_t) 0x17b6b2},
      {"http://www.lavarnd.org/index.html", (uint32_t) 0xad4cfb},
      {"http://www.lavarnd.org/what/nist-test.html", (uint32_t) 0x256811},
      {"http://www.macosxhints.com/", (uint32_t) 0xb18dd8},
      {"http://www.mellis.com/", (uint32_t) 0x61c153},
      {"http://www.nature.nps.gov/air/webcams/parks/havoso2alert/havoalert.cfm",
       (uint32_t) 0x47d20d},
      {"http://www.nature.nps.gov/air/webcams/parks/havoso2alert/"
       "timelines_24.cfm",
       (uint32_t) 0x8b689f},
      {"http://www.paulnoll.com/", (uint32_t) 0xd2a40b},
      {"http://www.pepysdiary.com/", (uint32_t) 0x549b0a},
      {"http://www.sciencenews.org/index/home/activity/view",
       (uint32_t) 0xe1b55b},
      {"http://www.skyandtelescope.com/", (uint32_t) 0x0cd3d1},
      {"http://www.sput.nl/~rob/sirius.html", (uint32_t) 0x471605},
      {"http://www.systemexperts.com/", (uint32_t) 0x5eef10},
      {"http://www.tq-international.com/phpBB3/index.php", (uint32_t) 0xed3629},
      {"http://www.travelquesttours.com/index.htm", (uint32_t) 0x624952},
      {"http://www.wunderground.com/global/stations/89606.html",
       (uint32_t) 0x9b8688},
      {R10 ("21701"), (uint32_t) 0x15e25f},
      {R10 ("M21701"), (uint32_t) 0xa98d05},
      {R10 ("2^21701-1"), (uint32_t) 0xdf8bcc},
      {R10 ("\x54\xc5"), (uint32_t) 0x1e9051},
      {R10 ("\xc5\x54"), (uint32_t) 0x3f70db},
      {R10 ("23209"), (uint32_t) 0x95aedb},
      {R10 ("M23209"), (uint32_t) 0xa7f7d7},
      {R10 ("2^23209-1"), (uint32_t) 0x3bc660},
      {R10 ("\x5a\xa9"), (uint32_t) 0x610967},
      {R10 ("\xa9\x5a"), (uint32_t) 0x157785},
      {R10 ("391581216093"), (uint32_t) 0x2b2800},
      {R10 ("391581*2^216093-1"), (uint32_t) 0x8239ef},
      {R10 ("\x05\xf9\x9d\x03\x4c\x81"), (uint32_t) 0x5869f5},
      {R10 ("FEDCBA9876543210"), (uint32_t) 0x415c76},
      {R10 ("\xfe\xdc\xba\x98\x76\x54\x32\x10"), (uint32_t) 0xe4ff6f},
      {R10 ("EFCDAB8967452301"), (uint32_t) 0xb7977d},
      {R10 ("\xef\xcd\xab\x89\x67\x45\x23\x01"), (uint32_t) 0xa43a7b},
      {R10 ("0123456789ABCDEF"), (uint32_t) 0xb3be1e},
      {R10 ("\x01\x23\x45\x67\x89\xab\xcd\xef"), (uint32_t) 0x777aaf},
      {R10 ("1032547698BADCFE"), (uint32_t) 0x21c38a},
      {R10 ("\x10\x32\x54\x76\x98\xba\xdc\xfe"), (uint32_t) 0x9d0839},
      {R500 ("\x07"), (uint32_t) 0xa27250},
      {R500 ("~"), (uint32_t) 0xc5c656},
      {R500 ("\x7f"), (uint32_t) 0x3b0800},
      {NULL, 0} /* MUST BE LAST */
   };

   for (i = 0; v[i].str != NULL; ++i) {
      ASSERT_CMPUINT32 (_mongoc_fnv_24a_str (v[i].str), ==, v[i].hash);
   }
}

void
test_fnv_install (TestSuite *suite)
{
   TestSuite_Add (suite, "/fnv/check_hashes", test_fnv_check_hashes);
}
