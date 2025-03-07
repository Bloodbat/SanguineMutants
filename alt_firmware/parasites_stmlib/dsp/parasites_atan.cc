// Copyright 2014 Olivier Gillet.
//
// Author: Olivier Gillet (ol.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Fast arc-tangent routines.

#include "parasites_stmlib/dsp/parasites_atan.h"

namespace parasites_stmlib {

/* extern */
const uint16_t atan_lut[513] = {
      0,    20,    40,    61,    81,   101,   122,   142, 
    162,   183,   203,   224,   244,   264,   285,   305, 
    326,   346,   366,   387,   407,   427,   448,   468, 
    489,   509,   529,   550,   570,   591,   611,   631, 
    652,   672,   693,   713,   733,   754,   774,   795, 
    815,   836,   856,   877,   897,   917,   938,   958, 
    979,   999,  1020,  1040,  1061,  1081,  1102,  1122, 
   1143,  1163,  1184,  1204,  1225,  1245,  1266,  1286, 
   1307,  1327,  1348,  1368,  1389,  1409,  1430,  1451, 
   1471,  1492,  1512,  1533,  1554,  1574,  1595,  1615, 
   1636,  1657,  1677,  1698,  1719,  1739,  1760,  1780, 
   1801,  1822,  1843,  1863,  1884,  1905,  1925,  1946, 
   1967,  1988,  2008,  2029,  2050,  2071,  2091,  2112, 
   2133,  2154,  2175,  2195,  2216,  2237,  2258,  2279, 
   2300,  2321,  2342,  2362,  2383,  2404,  2425,  2446, 
   2467,  2488,  2509,  2530,  2551,  2572,  2593,  2614, 
   2635,  2656,  2677,  2698,  2719,  2740,  2761,  2783, 
   2804,  2825,  2846,  2867,  2888,  2910,  2931,  2952, 
   2973,  2994,  3016,  3037,  3058,  3079,  3101,  3122, 
   3143,  3165,  3186,  3207,  3229,  3250,  3272,  3293, 
   3315,  3336,  3357,  3379,  3400,  3422,  3443,  3465, 
   3487,  3508,  3530,  3551,  3573,  3595,  3616,  3638, 
   3660,  3681,  3703,  3725,  3747,  3768,  3790,  3812, 
   3834,  3856,  3877,  3899,  3921,  3943,  3965,  3987, 
   4009,  4031,  4053,  4075,  4097,  4119,  4141,  4163, 
   4185,  4207,  4230,  4252,  4274,  4296,  4318,  4341, 
   4363,  4385,  4408,  4430,  4452,  4475,  4497,  4520, 
   4542,  4565,  4587,  4610,  4632,  4655,  4677,  4700, 
   4723,  4745,  4768,  4791,  4813,  4836,  4859,  4882, 
   4905,  4927,  4950,  4973,  4996,  5019,  5042,  5065, 
   5088,  5111,  5134,  5158,  5181,  5204,  5227,  5250, 
   5274,  5297,  5320,  5344,  5367,  5390,  5414,  5437, 
   5461,  5484,  5508,  5532,  5555,  5579,  5603,  5626, 
   5650,  5674,  5698,  5721,  5745,  5769,  5793,  5817, 
   5841,  5865,  5889,  5914,  5938,  5962,  5986,  6010, 
   6035,  6059,  6084,  6108,  6132,  6157,  6181,  6206, 
   6231,  6255,  6280,  6305,  6330,  6354,  6379,  6404, 
   6429,  6454,  6479,  6504,  6529,  6554,  6580,  6605, 
   6630,  6656,  6681,  6706,  6732,  6757,  6783,  6809, 
   6834,  6860,  6886,  6912,  6937,  6963,  6989,  7015, 
   7041,  7068,  7094,  7120,  7146,  7173,  7199,  7225, 
   7252,  7278,  7305,  7332,  7358,  7385,  7412,  7439, 
   7466,  7493,  7520,  7547,  7574,  7602,  7629,  7656, 
   7684,  7711,  7739,  7767,  7795,  7822,  7850,  7878, 
   7906,  7934,  7962,  7991,  8019,  8047,  8076,  8104, 
   8133,  8162,  8190,  8219,  8248,  8277,  8306,  8335, 
   8365,  8394,  8423,  8453,  8483,  8512,  8542,  8572, 
   8602,  8632,  8662,  8692,  8723,  8753,  8784,  8814, 
   8845,  8876,  8907,  8938,  8969,  9000,  9032,  9063, 
   9095,  9127,  9158,  9190,  9223,  9255,  9287,  9319, 
   9352,  9385,  9418,  9451,  9484,  9517,  9550,  9584, 
   9617,  9651,  9685,  9719,  9753,  9788,  9822,  9857, 
   9892,  9927,  9962,  9998, 10033, 10069, 10105, 10141, 
  10177, 10213, 10250, 10287, 10324, 10361, 10399, 10436, 
  10474, 10512, 10550, 10589, 10628, 10667, 10706, 10745, 
  10785, 10825, 10865, 10906, 10946, 10988, 11029, 11070, 
  11112, 11155, 11197, 11240, 11283, 11327, 11371, 11415, 
  11460, 11505, 11550, 11596, 11642, 11688, 11736, 11783, 
  11831, 11879, 11928, 11978, 12028, 12078, 12129, 12181, 
  12233, 12286, 12340, 12394, 12449, 12505, 12561, 12618, 
  12676, 12735, 12795, 12856, 12918, 12981, 13045, 13111, 
  13177, 13245, 13315, 13386, 13459, 13533, 13610, 13688, 
  13769, 13853, 13939, 14028, 14121, 14218, 14319, 14425, 
  14537, 14657, 14785, 14925, 15079, 15254, 15461, 15731, 
  16383,
};

// Generated with:
// static void init_atan_lut() {
//   for (size_t i = 0; i < 513; ++i) {
//     atan_lut[i] = 65536.0 / (2 * M_PI) * asinf(i / 512.0f);
//     printf("%5d, ", atan_lut[i]);
//     if (i % 8 == 7) {
//       printf("\n");
//     }
//   }
//   printf("\n");
// }

}  // namespace parasites_stmlib
