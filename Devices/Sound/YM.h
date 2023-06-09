/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright � 2023, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#ifndef _YM_H_
#define _YM_H_

#include <cstdint>

namespace YM /* Yamaha */
{
	/* Half period log-sin table */
	static const uint16_t SineTable[512] =
	{
		/*
		x = 0..255, y = round(-log(sin((x + 0.5) * pi / 256 / 2)) / log(2) * 256)

		This table has been constructed from actual YM3812 die shots :
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665

		Note: The table has been extended from a quarter to a half period
		*/
		0x0859, 0x06C3, 0x0607, 0x058B, 0x052E, 0x04E4, 0x04A6, 0x0471,
		0x0443, 0x041A, 0x03F5, 0x03D3, 0x03B5, 0x0398, 0x037E, 0x0365,
		0x034E, 0x0339, 0x0324, 0x0311, 0x02FF, 0x02ED, 0x02DC, 0x02CD,
		0x02BD, 0x02AF, 0x02A0, 0x0293, 0x0286, 0x0279, 0x026D, 0x0261,
		0x0256, 0x024B, 0x0240, 0x0236, 0x022C, 0x0222, 0x0218, 0x020F,
		0x0206, 0x01FD, 0x01F5, 0x01EC, 0x01E4, 0x01DC, 0x01D4, 0x01CD,
		0x01C5, 0x01BE, 0x01B7, 0x01B0, 0x01A9, 0x01A2, 0x019B, 0x0195,
		0x018F, 0x0188, 0x0182, 0x017C, 0x0177, 0x0171, 0x016B, 0x0166,
		0x0160, 0x015B, 0x0155, 0x0150, 0x014B, 0x0146, 0x0141, 0x013C,
		0x0137, 0x0133, 0x012E, 0x0129, 0x0125, 0x0121, 0x011C, 0x0118,
		0x0114, 0x010F, 0x010B, 0x0107, 0x0103, 0x00FF, 0x00FB, 0x00F8,
		0x00F4, 0x00F0, 0x00EC, 0x00E9, 0x00E5, 0x00E2, 0x00DE, 0x00DB,
		0x00D7, 0x00D4, 0x00D1, 0x00CD, 0x00CA, 0x00C7, 0x00C4, 0x00C1,
		0x00BE, 0x00BB, 0x00B8, 0x00B5, 0x00B2, 0x00AF, 0x00AC, 0x00A9,
		0x00A7, 0x00A4, 0x00A1, 0x009F, 0x009C, 0x0099, 0x0097, 0x0094,
		0x0092, 0x008F, 0x008D, 0x008A, 0x0088, 0x0086, 0x0083, 0x0081,
		0x007F, 0x007D, 0x007A, 0x0078, 0x0076, 0x0074, 0x0072, 0x0070,
		0x006E, 0x006C, 0x006A, 0x0068, 0x0066, 0x0064, 0x0062, 0x0060,
		0x005E, 0x005C, 0x005B, 0x0059, 0x0057, 0x0055, 0x0053, 0x0052,
		0x0050, 0x004E, 0x004D, 0x004B, 0x004A, 0x0048, 0x0046, 0x0045,
		0x0043, 0x0042, 0x0040, 0x003F, 0x003E, 0x003C, 0x003B, 0x0039,
		0x0038, 0x0037, 0x0035, 0x0034, 0x0033, 0x0031, 0x0030, 0x002F,
		0x002E, 0x002D, 0x002B, 0x002A, 0x0029, 0x0028, 0x0027, 0x0026,
		0x0025, 0x0024, 0x0023, 0x0022, 0x0021, 0x0020, 0x001F, 0x001E,
		0x001D, 0x001C, 0x001B, 0x001A, 0x0019, 0x0018, 0x0017, 0x0017,
		0x0016, 0x0015, 0x0014, 0x0014, 0x0013, 0x0012, 0x0011, 0x0011,
		0x0010, 0x000F, 0x000F, 0x000E, 0x000D, 0x000D, 0x000C, 0x000C,
		0x000B, 0x000A, 0x000A, 0x0009, 0x0009, 0x0008, 0x0008, 0x0007,
		0x0007, 0x0007, 0x0006, 0x0006, 0x0005, 0x0005, 0x0005, 0x0004,
		0x0004, 0x0004, 0x0003, 0x0003, 0x0003, 0x0002, 0x0002, 0x0002,
		0x0002, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
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

	/* Exponential (or pow2) table */
	static const uint32_t ExpTable[256] =
	{
		/*
		x = 0..255, y = round((power(2, x/256)-1)*1024)

		This table has been constructed from actual YM3812 die shots:
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665

		Notes: 
		1. Table is reversed
		2. The implicit bit10 has been set
		3. Values are shifted left by 2
		*/

		0x1FE8, 0x1FD4, 0x1FBC, 0x1FA8, 0x1F90, 0x1F7C, 0x1F68, 0x1F50,
		0x1F3C, 0x1F24, 0x1F10, 0x1EFC, 0x1EE4, 0x1ED0, 0x1EB8, 0x1EA4,
		0x1E90, 0x1E7C, 0x1E64, 0x1E50, 0x1E3C, 0x1E28, 0x1E10, 0x1DFC,
		0x1DE8, 0x1DD4, 0x1DC0, 0x1DA8, 0x1D94, 0x1D80, 0x1D6C, 0x1D58,
		0x1D44, 0x1D30, 0x1D1C, 0x1D08, 0x1CF4, 0x1CE0, 0x1CCC, 0x1CB8,
		0x1CA4, 0x1C90, 0x1C7C, 0x1C68, 0x1C54, 0x1C40, 0x1C2C, 0x1C18,
		0x1C08, 0x1BF4, 0x1BE0, 0x1BCC, 0x1BB8, 0x1BA4, 0x1B94, 0x1B80,
		0x1B6C, 0x1B58, 0x1B48, 0x1B34, 0x1B20, 0x1B10, 0x1AFC, 0x1AE8,
		0x1AD4, 0x1AC4, 0x1AB0, 0x1AA0, 0x1A8C, 0x1A78, 0x1A68, 0x1A54,
		0x1A44, 0x1A30, 0x1A20, 0x1A0C, 0x19FC, 0x19E8, 0x19D8, 0x19C4,
		0x19B4, 0x19A0, 0x1990, 0x197C, 0x196C, 0x195C, 0x1948, 0x1938,
		0x1924, 0x1914, 0x1904, 0x18F0, 0x18E0, 0x18D0, 0x18C0, 0x18AC,
		0x189C, 0x188C, 0x1878, 0x1868, 0x1858, 0x1848, 0x1838, 0x1824,
		0x1814, 0x1804, 0x17F4, 0x17E4, 0x17D4, 0x17C0, 0x17B0, 0x17A0,
		0x1790, 0x1780, 0x1770, 0x1760, 0x1750, 0x1740, 0x1730, 0x1720,
		0x1710, 0x1700, 0x16F0, 0x16E0, 0x16D0, 0x16C0, 0x16B0, 0x16A0,
		0x1690, 0x1680, 0x1670, 0x1664, 0x1654, 0x1644, 0x1634, 0x1624,
		0x1614, 0x1604, 0x15F8, 0x15E8, 0x15D8, 0x15C8, 0x15BC, 0x15AC,
		0x159C, 0x158C, 0x1580, 0x1570, 0x1560, 0x1550, 0x1544, 0x1534,
		0x1524, 0x1518, 0x1508, 0x14F8, 0x14EC, 0x14DC, 0x14D0, 0x14C0,
		0x14B0, 0x14A4, 0x1494, 0x1488, 0x1478, 0x146C, 0x145C, 0x1450,
		0x1440, 0x1430, 0x1424, 0x1418, 0x1408, 0x13FC, 0x13EC, 0x13E0,
		0x13D0, 0x13C4, 0x13B4, 0x13A8, 0x139C, 0x138C, 0x1380, 0x1370,
		0x1364, 0x1358, 0x1348, 0x133C, 0x1330, 0x1320, 0x1314, 0x1308,
		0x12F8, 0x12EC, 0x12E0, 0x12D4, 0x12C4, 0x12B8, 0x12AC, 0x12A0,
		0x1290, 0x1284, 0x1278, 0x126C, 0x1260, 0x1250, 0x1244, 0x1238,
		0x122C, 0x1220, 0x1214, 0x1208, 0x11F8, 0x11EC, 0x11E0, 0x11D4,
		0x11C8, 0x11BC, 0x11B0, 0x11A4, 0x1198, 0x118C, 0x1180, 0x1174,
		0x1168, 0x115C, 0x1150, 0x1144, 0x1138, 0x112C, 0x1120, 0x1114,
		0x1108, 0x10FC, 0x10F0, 0x10E4, 0x10D8, 0x10CC, 0x10C0, 0x10B4,
		0x10A8, 0x10A0, 0x1094, 0x1088, 0x107C, 0x1070, 0x1064, 0x1058,
		0x1050, 0x1044, 0x1038, 0x102C, 0x1020, 0x1018, 0x100C, 0x1000
	};

namespace OPL /* FM Operator Type-L */
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

namespace OPN /* FM Operator Type-N */
{
	static uint32_t LfoAmTable[128][4];
	static  int32_t LfoPmTable[128][32][8];
	
	/* Note table */
	static const uint32_t Note[16] =
	{
		/*
		This table uses the upper 4 bits of FNUM(F11 - F8) to
		generate a 2-bit note. Together with BLOCK this creates
		a 5-bit keycode :

		Keycode = (BLOCK << 2) | NoteTable[F11 : F8]

		This note table is generated as follows:
		b0 = F11 & (F10 | F9 | F8) | !F11 & F10 & F9 & F8 
		b1 = F11

		YM2608 manual page 25
		*/

		0, 0, 0, 0, 0, 0, 0, 1,
		2, 3, 3, 3, 3, 3, 3, 3
	};

	/* Detune table */
	static const int32_t Detune[32][8] =
	{
		/*
		This table uses the 5-bit keycode and 3-bit detune
		values to look-up phase adjustment. Each step
		corresponds to 0.053Hz (with a master clock of 8MHz)

		YM2608 manual page 26
		*/

		{0, 0,  1,  2, 0,  0,  -1,  -2},
		{0, 0,  1,  2, 0,  0,  -1,  -2},
		{0, 0,  1,  2, 0,  0,  -1,  -2},
		{0, 0,  1,  2, 0,  0,  -1,  -2},
		{0, 1,  2,  2, 0, -1,  -2,  -2},
		{0, 1,  2,  3, 0, -1,  -2,  -3},
		{0, 1,  2,  3, 0, -1,  -2,  -3},
		{0, 1,  2,  3, 0, -1,  -2,  -3},
		{0, 1,  2,  4, 0, -1,  -2,  -4},
		{0, 1,  3,  4, 0, -1,  -3,  -4},
		{0, 1,  3,  4, 0, -1,  -3,  -4},
		{0, 1,  3,  5, 0, -1,  -3,  -5},
		{0, 2,  4,  5, 0, -2,  -4,  -5},
		{0, 2,  4,  6, 0, -2,  -4,  -6},
		{0, 2,  4,  6, 0, -2,  -4,  -6},
		{0, 2,  5,  7, 0, -2,  -5,  -7},
		{0, 2,  5,  8, 0, -2,  -5,  -8},
		{0, 3,  6,  8, 0, -3,  -6,  -8},
		{0, 3,  6,  9, 0, -3,  -6,  -9},
		{0, 3,  7, 10, 0, -3,  -7, -10},
		{0, 4,  8, 11, 0, -4,  -8, -11},
		{0, 4,  8, 12, 0, -4,  -8, -12},
		{0, 4,  9, 13, 0, -4,  -9, -13},
		{0, 5, 10, 14, 0, -5, -10, -14},
		{0, 5, 11, 16, 0, -5, -11, -16},
		{0, 6, 12, 17, 0, -6, -12, -17},
		{0, 6, 13, 19, 0, -6, -13, -19},
		{0, 7, 14, 20, 0, -7, -14, -20},
		{0, 8, 16, 22, 0, -8, -16, -22},
		{0, 8, 16, 22, 0, -8, -16, -22},
		{0, 8, 16, 22, 0, -8, -16, -22},
		{0, 8, 16, 22, 0, -8, -16, -22}
	};

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

	/* LFO period table */
	static const uint32_t LfoPeriod[8] =
	{
		/*
			This table defines the period (in samples) of a given frequency
			The following values are valid (YM2608 manual page 33):
			0 = 3.98 Hz
			1 = 5.56 Hz
			2 = 6.02 Hz
			3 = 6.37 Hz
			4 = 6.88 Hz
			5 = 9.63 Hz
			6 = 48.1 Hz
			7 = 72.2 Hz

			The period is calculated as follows:
			Period = (Clock / Divider) / (Steps * Freq)

			The LFO counter is 7-bit (128 steps):
			Period = (8MHz / Prescaler(6) / 24) / (128 * Freq)
		*/

		109, 78, 72, 68, 63, 45, 9, 6
	};

	/* LFO AM shift table */
	const uint32_t LfoAmShift[4] =
	{
		/*
			This table is used to calculate the LFO AM attenuation adjustment
			The following values are valid (YM2608 manual page 33):
			0 =    0 dB (valid bits: 0x00)
			1 =  1.4 dB (valid bits: 0x0F)
			2 =  5.9 dB (valid bits: 0x3F)
			3 = 11.8 dB (valid bits: 0x7E)

			Weighting of each valid bit is:
			6dB - 3dB - 1.5dB - 0.75dB - 0.375dB - 0.1875dB - 0.09375dB

			The AM wave runs from 126 -> 0 -> 126 (7-bit)
			This creates an inverted triangular shaped wave.

			At the maximum amplitude, the AM adjustment should be 0x7E.

			The following function is used to calculate the AM adjustment

			VolumeAdjust(dB) = AM phase >> AM shift

			Since we know what the resulting value (dB) will be (see above),
			we can now get the shift values used in the calculation
		*/

		7, 3, 1, 0
	};

	/* LFO PM shift table 1 */
	const uint32_t LfoPmShift1[8][8] =
	{
		/*
		Credits to nukeykt:
		https://github.com/nukeykt/Nuked-OPN2
		*/
		{ 7, 7, 7, 7, 7, 7, 7, 7 },	/* PMS = 0 */
		{ 7, 7, 7, 7, 7, 7, 7, 7 },	/* PMS = 1 */
		{ 7, 7, 7, 7, 7, 7, 1, 1 },	/* PMS = 2 */
		{ 7, 7, 7, 7, 1, 1, 1, 1 },	/* PMS = 3 */
		{ 7, 7, 7, 1, 1, 1, 1, 0 },	/* PMS = 4 */
		{ 7, 7, 1, 1, 0, 0, 0, 0 },	/* PMS = 5 */
		{ 7, 7, 1, 1, 0, 0, 0, 0 },	/* PMS = 6 */
		{ 7, 7, 1, 1, 0, 0, 0, 0 }	/* PMS = 7 */
	};

	/* LFO PM shift table 2 */
	const uint32_t LfoPmShift2[8][8] =
	{
		/*
		Credits to nukeykt:
		https://github.com/nukeykt/Nuked-OPN2
		*/
		{ 7, 7, 7, 7, 7, 7, 7, 7 },	/* PMS = 0 */
		{ 7, 7, 7, 7, 2, 2, 2, 2 },	/* PMS = 1 */
		{ 7, 7, 7, 2, 2, 2, 7, 7 },	/* PMS = 2 */
		{ 7, 7, 2, 2, 7, 7, 2, 2 },	/* PMS = 3 */
		{ 7, 7, 2, 7, 7, 7, 2, 7 },	/* PMS = 4 */
		{ 7, 7, 7, 2, 7, 7, 2, 1 },	/* PMS = 5 */
		{ 7, 7, 7, 2, 7, 7, 2, 1 },	/* PMS = 6 */
		{ 7, 7, 7, 2, 7, 7, 2, 1 }	/* PMS = 7 */
	};

	/* LFO PM shift table 3 */
	const uint32_t LfoPmShift3[8] =
	{
		2, 2, 2, 2, 2, 2, 1, 0
	};

	/* Build all OPN related tables */
	static void BuildTables()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			/* Build LFO AM table */
			for (auto lfo = 0; lfo < 128; lfo++)
			{
				/*
					The LFO AM waveform is an inverted triangle (128 steps)
					It runs from 126 -> 0 -> 126 (adjusted 2 per step)
				*/
				uint32_t step;

				if (lfo & 0x40) step = (lfo & 0x3F) << 1;
				else step = (lfo ^ 0x3F) << 1;

				for (auto ams = 0; ams < 4; ams++)
				{
					YM::OPN::LfoAmTable[lfo][ams] = step >> YM::OPN::LfoAmShift[ams];
				}
			}

			/* Build LFO PM table */
			for (auto fnum = 0; fnum < 128; fnum++)
			{
				for (auto lfo = 0; lfo < 32; lfo++)
				{
					/*
						The LFO PM waveform is a triangle (32 steps)
						It runs from 0 -> 7 -> 0 -> -7 -> 0
					*/
					uint32_t step = lfo & 0x0F;
					if (lfo & 0x08) step = lfo ^ 0x0F;

					for (auto pms = 0; pms < 8; pms++)
					{
						int32_t value = (fnum >> YM::OPN::LfoPmShift1[pms][step]) + (fnum >> YM::OPN::LfoPmShift2[pms][step]);
						value >>= YM::OPN::LfoPmShift3[pms];

						YM::OPN::LfoPmTable[fnum][lfo][pms] = (lfo & 0x10) ? -value : value;
					}
				}
			}

			Initialized = true;
		}
	}
} // namespace OPN

namespace AWM /* Advanced Wave Memory */
{
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
			/* Tremolo table (AM) */
			for (auto lfo = 0; lfo < 256; lfo++)
			{
				uint32_t step = lfo; /* 256 steps */

				/* Create triangular shaped wave (0x00 .. 0x7F, 0x7F .. 0x00) */
				if (step & 0x80) step ^= 0xFF;

				//TODO: is this an inverted triangle (like OPN) ?
				//		eg. starting at maximum amplitude
				//		Als need to confirm on the step increment (which is 2 for OPN)

				for (auto ams = 0; ams < 8; ams++)
				{
					TremoloTable[lfo][ams] = (step * YM::AWM::LfoAmDepth[ams]) >> 7;
				}
			}

			/* Vibrato table (PM) */
			for (auto lfo = 0; lfo < 64; lfo++)
			{
				uint32_t step = lfo; /* 64 steps (32 pos, 32 neg) */

				/* Create triangular shaped wave (0x0 .. 0xF, 0xF .. 0x0) */
				if (step & 0x10) step ^= 0x1F;

				for (auto pms = 0; pms < 8; pms++)
				{
					int32_t value = ((step & 0x0F) * YM::AWM::LfoPmDepth[pms]) >> 4;
					
					VibratoTable[lfo][pms] = (lfo & 0x20) ? -value : value;
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