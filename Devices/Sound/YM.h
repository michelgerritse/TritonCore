/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#ifndef _YM_H_
#define _YM_H_

#include <cstdint>

/* Yamaha */
namespace YM
{
	/* Quarter log-sin table */
	static const uint32_t SineTable[256] =
	{
		/*
		x = 0..255, y = round(-log(sin((x + 0.5) * pi / 256 / 2)) / log(2) * 256)

		This table has been constructed from actual YM3812 die shots :
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665

		*/
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0002,
		0x0002, 0x0002, 0x0002, 0x0003, 0x0003, 0x0003, 0x0004, 0x0004,
		0x0004, 0x0005, 0x0005, 0x0005, 0x0006, 0x0006, 0x0007, 0x0007,
		0x0007, 0x0008, 0x0008, 0x0009, 0x0009, 0x000A, 0x000A, 0x000B,
		0x000C, 0x000C, 0x000D, 0x000D, 0x000E, 0x000F, 0x000F, 0x0010,
		0x0011, 0x0011, 0x0012, 0x0013, 0x0014, 0x0014, 0x0015, 0x0016,
		0x0017, 0x0017, 0x0018, 0x0019, 0x001A, 0x001B, 0x001C, 0x001D,
		0x001E, 0x001F, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025,
		0x0026, 0x0027, 0x0028, 0x0029, 0x002A, 0x002B, 0x002D, 0x002E,
		0x002F, 0x0030, 0x0031, 0x0033, 0x0034, 0x0035, 0x0037, 0x0038,
		0x0039, 0x003B, 0x003C, 0x003E, 0x003F, 0x0040, 0x0042, 0x0043,
		0x0045, 0x0046, 0x0048, 0x004A, 0x004B, 0x004D, 0x004E, 0x0050,
		0x0052, 0x0053, 0x0055, 0x0057, 0x0059, 0x005B, 0x005C, 0x005E,
		0x0060, 0x0062, 0x0064, 0x0066, 0x0068, 0x006A, 0x006C, 0x006E,
		0x0070, 0x0072, 0x0074, 0x0076, 0x0078, 0x007A, 0x007D, 0x007F,
		0x0081, 0x0083, 0x0086, 0x0088, 0x008A, 0x008D, 0x008F, 0x0092,
		0x0094, 0x0097, 0x0099, 0x009C, 0x009F, 0x00A1, 0x00A4, 0x00A7,
		0x00A9, 0x00AC, 0x00AF, 0x00B2, 0x00B5, 0x00B8, 0x00BB, 0x00BE,
		0x00C1, 0x00C4, 0x00C7, 0x00CA, 0x00CD, 0x00D1, 0x00D4, 0x00D7,
		0x00DB, 0x00DE, 0x00E2, 0x00E5, 0x00E9, 0x00EC, 0x00F0, 0x00F4,
		0x00F8, 0x00FB, 0x00FF, 0x0103, 0x0107, 0x010B, 0x010F, 0x0114,
		0x0118, 0x011C, 0x0121, 0x0125, 0x0129, 0x012E, 0x0133, 0x0137,
		0x013C, 0x0141, 0x0146, 0x014B, 0x0150, 0x0155, 0x015B, 0x0160,
		0x0166, 0x016B, 0x0171, 0x0177, 0x017C, 0x0182, 0x0188, 0x018F,
		0x0195, 0x019B, 0x01A2, 0x01A9, 0x01B0, 0x01B7, 0x01BE, 0x01C5,
		0x01CD, 0x01D4, 0x01DC, 0x01E4, 0x01EC, 0x01F5, 0x01FD, 0x0206,
		0x020F, 0x0218, 0x0222, 0x022C, 0x0236, 0x0240, 0x024B, 0x0256,
		0x0261, 0x026D, 0x0279, 0x0286, 0x0293, 0x02A0, 0x02AF, 0x02BD,
		0x02CD, 0x02DC, 0x02ED, 0x02FF, 0x0311, 0x0324, 0x0339, 0x034E,
		0x0365, 0x037E, 0x0398, 0x03B5, 0x03D3, 0x03F5, 0x041A, 0x0443,
		0x0471, 0x04A6, 0x04E4, 0x052E, 0x058B, 0x0607, 0x06C3, 0x0859
	};

	/* Exponential (or pow) table */
	static const uint32_t ExpTable[256] =
	{
		/*
		x = 0..255, y = round((power(2, x/256)-1)*1024)

		This table has been constructed from actual YM3812 die shots:
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665
		*/

		0x000, 0x003, 0x006, 0x008, 0x00b, 0x00e, 0x011, 0x014,
		0x016, 0x019, 0x01c, 0x01f, 0x022, 0x025, 0x028, 0x02a,
		0x02d, 0x030, 0x033, 0x036, 0x039, 0x03c, 0x03f, 0x042,
		0x045, 0x048, 0x04b, 0x04e, 0x051, 0x054, 0x057, 0x05a,
		0x05d, 0x060, 0x063, 0x066, 0x069, 0x06c, 0x06f, 0x072,
		0x075, 0x078, 0x07b, 0x07e, 0x082, 0x085, 0x088, 0x08b,
		0x08e, 0x091, 0x094, 0x098, 0x09b, 0x09e, 0x0a1, 0x0a4,
		0x0a8, 0x0ab, 0x0ae, 0x0b1, 0x0b5, 0x0b8, 0x0bb, 0x0be,
		0x0c2, 0x0c5, 0x0c8, 0x0cc, 0x0cf, 0x0d2, 0x0d6, 0x0d9,
		0x0dc, 0x0e0, 0x0e3, 0x0e7, 0x0ea, 0x0ed, 0x0f1, 0x0f4,
		0x0f8, 0x0fb, 0x0ff, 0x102, 0x106, 0x109, 0x10c, 0x110,
		0x114, 0x117, 0x11b, 0x11e, 0x122, 0x125, 0x129, 0x12c,
		0x130, 0x134, 0x137, 0x13b, 0x13e, 0x142, 0x146, 0x149,
		0x14d, 0x151, 0x154, 0x158, 0x15c, 0x160, 0x163, 0x167,
		0x16b, 0x16f, 0x172, 0x176, 0x17a, 0x17e, 0x181, 0x185,
		0x189, 0x18d, 0x191, 0x195, 0x199, 0x19c, 0x1a0, 0x1a4,
		0x1a8, 0x1ac, 0x1b0, 0x1b4, 0x1b8, 0x1bc, 0x1c0, 0x1c4,
		0x1c8, 0x1cc, 0x1d0, 0x1d4, 0x1d8, 0x1dc, 0x1e0, 0x1e4,
		0x1e8, 0x1ec, 0x1f0, 0x1f5, 0x1f9, 0x1fd, 0x201, 0x205,
		0x209, 0x20e, 0x212, 0x216, 0x21a, 0x21e, 0x223, 0x227,
		0x22b, 0x230, 0x234, 0x238, 0x23c, 0x241, 0x245, 0x249,
		0x24e, 0x252, 0x257, 0x25b, 0x25f, 0x264, 0x268, 0x26d,
		0x271, 0x276, 0x27a, 0x27f, 0x283, 0x288, 0x28c, 0x291,
		0x295, 0x29a, 0x29e, 0x2a3, 0x2a8, 0x2ac, 0x2b1, 0x2b5,
		0x2ba, 0x2bf, 0x2c4, 0x2c8, 0x2cd, 0x2d2, 0x2d6, 0x2db,
		0x2e0, 0x2e5, 0x2e9, 0x2ee, 0x2f3, 0x2f8, 0x2fd, 0x302,
		0x306, 0x30b, 0x310, 0x315, 0x31a, 0x31f, 0x324, 0x329,
		0x32e, 0x333, 0x338, 0x33d, 0x342, 0x347, 0x34c, 0x351,
		0x356, 0x35b, 0x360, 0x365, 0x36a, 0x370, 0x375, 0x37a,
		0x37f, 0x384, 0x38a, 0x38f, 0x394, 0x399, 0x39f, 0x3a4,
		0x3a9, 0x3ae, 0x3b4, 0x3b9, 0x3bf, 0x3c4, 0x3c9, 0x3cf,
		0x3d4, 0x3da, 0x3df, 0x3e4, 0x3ea, 0x3ef, 0x3f5, 0x3fa
	};

namespace OPL
{
	/* Envelope counter shift table */
	static const uint32_t EgShift[64] =
	{
		12, 12, 12, 12,
		11, 11, 11, 11,
		10, 10, 10, 10,
		9,  9,  9,  9,
		8,  8,  8,  8,
		7,  7,  7,  7,
		6,  6,  6,  6,
		5,  5,  5,  5,
		4,  4,  4,  4,
		3,  3,  3,  3,
		2,  2,  2,  2,
		1,  1,  1,  1,
		0,  0,  0,  0,
		0,  0,  0,  0,
		0,  0,  0,  0,
		0,  0,  0,  0
	};

	/* Envelope generator level adjust table */
	static const uint32_t EgLevelAdjust[64][8] =
	{
		/* EG timing calculations: https://github.com/michelgerritse/YM-research */
		{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, /* Rate 00 - 03 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 04 - 07 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 08 - 11 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 12 - 15 */

		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 16 - 19 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 20 - 23 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 24 - 27 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 28 - 31 */

		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 32 - 35 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 36 - 39 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 40 - 43 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 44 - 47 */

		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 48 - 51 */
		{1,1,1,1,1,1,1,1}, {1,1,1,2,1,1,1,2}, {1,2,1,2,1,2,1,2}, {1,2,2,2,1,2,2,2}, /* Rate 52 - 55 */
		{2,2,2,2,2,2,2,2}, {2,2,2,4,2,2,2,4}, {2,4,2,4,2,4,2,4}, {2,4,4,4,2,4,4,4}, /* Rate 56 - 59 */
		{4,4,4,4,4,4,4,4}, {4,4,4,4,4,4,4,4}, {4,4,4,4,4,4,4,4}, {4,4,4,4,4,4,4,4}  /* Rate 60 - 63 */
	};

} // namespace OPL

namespace OPN
{
	/* Envelope counter shift table */
	static const uint32_t EgShift[64] =
	{
		11, 11, 11, 11,
		10, 10, 10, 10,
		9,  9,  9,  9,
		8,  8,  8,  8,
		7,  7,  7,  7,
		6,  6,  6,  6,
		5,  5,  5,  5,
		4,  4,  4,  4,
		3,  3,  3,  3,
		2,  2,  2,  2,
		1,  1,  1,  1,
		0,  0,  0,  0,
		0,  0,  0,  0,
		0,  0,  0,  0,
		0,  0,  0,  0,
		0,  0,  0,  0
	};

	/* Envelope generator level adjust table */
	static const uint32_t EgLevelAdjust[64][8] =
	{
		/* EG timing calculations: https://github.com/michelgerritse/YM-research */
		{0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,1,0,1,0,1,0,1}, {0,1,0,1,0,1,0,1}, /* Rate 00 - 03 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,0,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,0,1,1,1}, /* Rate 04 - 07 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 08 - 11 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 12 - 15 */

		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 16 - 19 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 20 - 23 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 24 - 27 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 28 - 31 */

		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 32 - 35 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 36 - 39 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 40 - 43 */
		{0,1,0,1,0,1,0,1}, {0,1,0,1,1,1,0,1}, {0,1,1,1,0,1,1,1}, {0,1,1,1,1,1,1,1}, /* Rate 44 - 47 */

		{1,1,1,1,1,1,1,1}, {1,1,1,2,1,1,1,2}, {1,2,1,2,1,2,1,2}, {1,2,2,2,1,2,2,2}, /* Rate 48 - 51 */
		{2,2,2,2,2,2,2,2}, {2,2,2,4,2,2,2,4}, {2,4,2,4,2,4,2,4}, {2,4,4,4,2,4,4,4}, /* Rate 52 - 55 */
		{4,4,4,4,4,4,4,4}, {4,4,4,8,4,4,4,8}, {4,8,4,8,4,8,4,8}, {4,8,8,8,4,8,8,8}, /* Rate 56 - 59 */
		{8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}, {8,8,8,8,8,8,8,8}  /* Rate 60 - 63 */
	};
} // namespace OPN

namespace AWM
{
	/* Pan attenuation (left) table */
	static const uint32_t PanAttnL[16] =
	{
		/* See OPL4 datasheet page 22 (section PANPOT)
		
		+--------+---------------------------------------------------------------+
		| Panpot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
		+----+---+---------------------------------------------------------------+
		| dB | L | 0 |-3 |-6 |-9 |-12|-15|-18|inf|inf| 0 | 0 | 0 | 0 | 0 | 0 | 0 |
		+----+---+---------------------------------------------------------------+
		
		*/
		//0, 8, 16, 24, 32, 40, 48, 127, 127, 0, 0, 0, 0, 0, 0, 0
		0, 32, 64, 96, 128, 160, 192, 1023, 1023, 0, 0, 0, 0, 0, 0, 0
	};

	/* Pan attenuation (right) table */
	static const uint32_t PanAttnR[16] =
	{
		/* See OPL4 datasheet page 22 (section PANPOT)
		
		+--------+---------------------------------------------------------------+
		| Panpot | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | A | B | C | D | E | F |
		+----+---+---------------------------------------------------------------+
		| db | R | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |inf|inf|-18|-15|-12|-9 |-6 |-3 |
		+----+---+---------------------------------------------------------------+
		
		*/
		//0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 48, 40, 32, 24, 16, 8
		0, 0, 0, 0, 0, 0, 0, 0, 1023, 1023, 192, 160, 128, 96, 64, 32
	};

	/* LFO period table */
	static const uint32_t LfoPeriod[8] =
	{
		/*
			This table defines the period (in samples) of a given frequency
			The following values are valid (OPL4 manual page 19):
			0 = 0.168 Hz
			1 = 2.019 Hz
			2 = 3.196 Hz
			3 = 4.206 Hz
			4 = 5.215 Hz
			5 = 5.888 Hz
			6 = 6.224 Hz
			7 = 7.066 Hz

			The period is calculated as follows:
			Period = (Clock / Divider) / (Steps * Freq)

			I'm assuming a 8-bit LFO counter (256 steps)

			With this information the period values are:
			1025, 85, 53, 40, 33, 29, 27, 24

			Since I think the width of the period register
			is 11-bit, you will get those values:
		*/

		1024, 85, 53, 40, 33, 29, 27, 24
	};

	/* LFO AM (tremolo) depth table */
	static const uint32_t LfoAmDepth[8] =
	{
		/*
			This table is used to calculate the LFO AM (tremolo) attenuation adjustment
			The following values are valid (OPL4 manual page 24):
			0 = 0.000 dB (valid bits: 0x00)
			1 = 1.781 dB (valid bits: 0x13)
			2 = 2.906 dB (valid bits: 0x1F)
			3 = 3.656 dB (valid bits: 0x27)
			4 = 4.406 dB (valid bits: 0x2F)
			5 = 5.906 dB (valid bits: 0x3F)
			6 = 7.406 dB (valid bits: 0x4F)
			7 = 11.91 dB (valid bits: 0x7F)

			Weighting of each valid bit is:
			6dB - 3dB - 1.5dB - 0.75dB - 0.375dB - 0.1875dB - 0.09375dB

			The AM phase runs from 0 -> 0x7F, 0x7F -> 0
			This creates a triangular shaped wave.

			At the maximum amplitude, the AM adjustment should be 0x7F.

			The following function is used to calculate the AM adjustment

			VolumeAdjust(dB) = (AM phase * AM depth) >> 7

			Since we know what the resulting value (dB) will be (see above),
			we can now get the depth values used in the calculation
		*/

		0x00, 0x14, 0x20, 0x28, 0x30, 0x40, 0x50, 0x80
	};

	
	/* LFO PM (vibrato) depth table */
	static const uint32_t LfoPmDepth[8] =
	{
		/*
			This table is used to calculate the LFO PM (vibrato) frequency adjustment
			The following values are valid (OPL4 manual page 20):
			0 =  0.000 cents
			1 =  3.378 cents
			2 =  5.065 cents
			3 =  6.750 cents
			4 = 10.114 cents
			5 = 20.170 cents
			6 = 40.108 cents
			7 = 79.370 cents

			Function used by Yamaha (OPL4 manual page 17) to calculate frequency:

			F (cents) = 1200 * log2((1024 + X) / 1024)) (note: ignoring the octave)

			This can be re-written into the following function:
			F = 1200 * (log2(1024 + X) - 10)

			Which is:
			(F / 1200) + 10 = log2(1024 + X)

			So finally:
			X = (2 ^ ((F / 1200) + 10)) - 1024

			With this function we can now calculate the PM depths for the
			given frequencies (in cents):
			0, 2, 3, 4, 6, 12, 24, 48

			However, those numbers are the results of the PM calculation:

			result = (PM phase * value) >> 4

			So we can now calculate the actual values used:
		*/

		0, 3, 4, 5, 7, 13, 26, 52
	};
} // namespace AWM

namespace PCMD8
{
	/* Pan attenuation (left) table */
	static const uint32_t PanAttnL[16] =
	{
		0, 0, 0, 0, 0, 0, 0, 0,
		0, 8, 16, 24, 32, 40, 48, 127
	};

	/* Pan attenuation (right) table */
	static const uint32_t PanAttnR[16] =
	{
		0, 127, 48, 40, 32, 24, 16, 8,
		0, 0, 0, 0, 0, 0, 0, 0		
	};
} // namespace PCMD8

} // namespace YM

#endif // !_YM_H_