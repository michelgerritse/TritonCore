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
#pragma once
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

		Note: The implicit bit10 has not been set yet
		*/

		0x0000, 0x0003, 0x0006, 0x0008, 0x000B, 0x000E, 0x0011, 0x0014,
		0x0016, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0028, 0x002A,
		0x002D, 0x0030, 0x0033, 0x0036, 0x0039, 0x003C, 0x003F, 0x0042,
		0x0045, 0x0048, 0x004B, 0x004E, 0x0051, 0x0054, 0x0057, 0x005A,
		0x005D, 0x0060, 0x0063, 0x0066, 0x0069, 0x006C, 0x006F, 0x0072,
		0x0075, 0x0078, 0x007B, 0x007E, 0x0082, 0x0085, 0x0088, 0x008B,
		0x008E, 0x0091, 0x0094, 0x0098, 0x009B, 0x009E, 0x00A1, 0x00A4,
		0x00A8, 0x00AB, 0x00AE, 0x00B1, 0x00B5, 0x00B8, 0x00BB, 0x00BE,
		0x00C2, 0x00C5, 0x00C8, 0x00CC, 0x00CF, 0x00D2, 0x00D6, 0x00D9,
		0x00DC, 0x00E0, 0x00E3, 0x00E7, 0x00EA, 0x00ED, 0x00F1, 0x00F4,
		0x00F8, 0x00FB, 0x00FF, 0x0102, 0x0106, 0x0109, 0x010C, 0x0110,
		0x0114, 0x0117, 0x011B, 0x011E, 0x0122, 0x0125, 0x0129, 0x012C,
		0x0130, 0x0134, 0x0137, 0x013B, 0x013E, 0x0142, 0x0146, 0x0149,
		0x014D, 0x0151, 0x0154, 0x0158, 0x015C, 0x0160, 0x0163, 0x0167,
		0x016B, 0x016F, 0x0172, 0x0176, 0x017A, 0x017E, 0x0181, 0x0185,
		0x0189, 0x018D, 0x0191, 0x0195, 0x0199, 0x019C, 0x01A0, 0x01A4,
		0x01A8, 0x01AC, 0x01B0, 0x01B4, 0x01B8, 0x01BC, 0x01C0, 0x01C4,
		0x01C8, 0x01CC, 0x01D0, 0x01D4, 0x01D8, 0x01DC, 0x01E0, 0x01E4,
		0x01E8, 0x01EC, 0x01F0, 0x01F5, 0x01F9, 0x01FD, 0x0201, 0x0205,
		0x0209, 0x020E, 0x0212, 0x0216, 0x021A, 0x021E, 0x0223, 0x0227,
		0x022B, 0x0230, 0x0234, 0x0238, 0x023C, 0x0241, 0x0245, 0x0249,
		0x024E, 0x0252, 0x0257, 0x025B, 0x025F, 0x0264, 0x0268, 0x026D,
		0x0271, 0x0276, 0x027A, 0x027F, 0x0283, 0x0288, 0x028C, 0x0291,
		0x0295, 0x029A, 0x029E, 0x02A3, 0x02A8, 0x02AC, 0x02B1, 0x02B5,
		0x02BA, 0x02BF, 0x02C4, 0x02C8, 0x02CD, 0x02D2, 0x02D6, 0x02DB,
		0x02E0, 0x02E5, 0x02E9, 0x02EE, 0x02F3, 0x02F8, 0x02FD, 0x0302,
		0x0306, 0x030B, 0x0310, 0x0315, 0x031A, 0x031F, 0x0324, 0x0329,
		0x032E, 0x0333, 0x0338, 0x033D, 0x0342, 0x0347, 0x034C, 0x0351,
		0x0356, 0x035B, 0x0360, 0x0365, 0x036A, 0x0370, 0x0375, 0x037A,
		0x037F, 0x0384, 0x038A, 0x038F, 0x0394, 0x0399, 0x039F, 0x03A4,
		0x03A9, 0x03AE, 0x03B4, 0x03B9, 0x03BF, 0x03C4, 0x03C9, 0x03CF,
		0x03D4, 0x03DA, 0x03DF, 0x03E4, 0x03EA, 0x03EF, 0x03F5, 0x03FA
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
	static uint32_t PowTable[256];
	static uint32_t TremoloTable[256][8];
	static  int32_t VibratoTable[64][8];

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
		*/

		1025, 85, 53, 40, 33, 29, 27, 24
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

	/* Build all AWM related tables */
	static void BuildTables()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			/* Linear to dB table */
			for (auto i = 0; i < 256; i++)
			{
				PowTable[i] = (YM::ExpTable[i ^ 255] | 0x400) << 2;
			}

			/* Tremolo table (AM) */
			for (auto i = 0; i < 256; i++)
			{
				uint32_t Step = i; /* 256 steps */

				/* Create triangular shaped wave (0x00 .. 0x7F, 0x7F .. 0x00) */
				if (Step & 0x80) Step ^= 0xFF;

				//TODO: is this an inverted triangle (like OPN) ?
				//		eg. starting at maximum amplitude
				//		Als need to confirm on the step increment (which is 2 for OPN)

				for (auto j = 0; j < 8; j++)
				{
					TremoloTable[i][j] = (Step * YM::AWM::LfoAmDepth[j]) >> 7;
				}
			}

			/* Vibrato table (PM) */
			for (auto i = 0; i < 64; i++)
			{
				uint32_t Step = i; /* 64 steps (32 pos, 32 neg) */

				/* Create triangular shaped wave (0x0 .. 0xF, 0xF .. 0x0) */
				if (Step & 0x10) Step ^= 0x1F;

				for (auto j = 0; j < 8; j++)
				{
					if (Step & 0x20) /* Negative phase */
					{
						VibratoTable[i][j] = 0 - (((Step & 0x0F) * YM::AWM::LfoPmDepth[j]) >> 4);
					}
					else /* Positive phase */
					{
						VibratoTable[i][j] = ((Step & 0x0F) * YM::AWM::LfoPmDepth[j]) >> 4;
					}
				}
			}

			Initialized = true;
		}
	}
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