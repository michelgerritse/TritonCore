/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2024, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#ifndef _YM_OPN_H_
#define _YM_OPN_H_

#include "YM.h"

namespace YM::OPN /* Yamaha - FM Operator Type-N */
{
	/* Maximum attenuation level */
	constexpr uint32_t MaxAttenuation = 0x3FF;

	/* Maximum envelope level */
	constexpr uint32_t MaxEgLevel = MaxAttenuation & ~((1 << 4) - 1);

	/* Memory type enumeration */
	enum Memory : uint32_t
	{
		ADPCMA = 0,
		ADPCMB
	};

	/* Operator data type */
	struct operator_t
	{
		uint32_t	KeyState;		/* Key on/off state */
		uint32_t	KeyLatch;		/* Latched key on/off flag */
		uint32_t	CsmLatch;		/* Latched CSM key on/off flag */

		uint32_t	FNum;			/* Frequency Nr. (11-bit) */
		uint32_t	Block;			/* Block (3-bit) */
		uint32_t	KeyCode;		/* Key code (5-bit) */

		uint32_t	Detune;			/* Detune (3-bit) */
		uint32_t	Multi;			/* Multiplier (4-bit) */
		uint32_t	TotalLevel;		/* Total level (7-bit) */
		uint32_t	KeyScale;		/* Key scale (2-bit) */
		uint16_t	SustainLvl;		/* Sustain level (4-bit) */
		uint32_t	AmOn;			/* LFO-AM on/off mask */

		uint32_t	SsgEnable;		/* SSG-EG Enable flag */
		uint32_t	SsgEgInv;		/* SSG-EG Inversion mode flag */
		uint32_t	SsgEgAlt;		/* SSG-EG Alternate mode flag */
		uint32_t	SsgEgHld;		/* SSG-EG Hold mode flag */
		uint32_t	SsgEgInvOut;	/* SSG-EG Inverted output flag */

		uint32_t	EgRate[4];		/* Envelope rates (5-bit) */
		uint32_t	EgPhase;		/* Envelope phase */
		uint16_t	EgLevel;		/* Envelope internal level (10-bit) */
		uint16_t	EgOutput;		/* Envelope output (12-bit) */

		uint32_t	PgPhase;		/* Phase counter (20-bit) */
		uint32_t	PgOutput;		/* Phase output (10-bit) */
		uint32_t	PgReset;		/* Phase reset flag */

		int16_t		Output[2];		/* Operator output (14-bit) */
	};

	/* Channel data type */
	struct channel_t
	{
		uint32_t	FNum;			/* Frequency Nr. (11-bit) */
		uint32_t	Block;			/* Block (3-bit) */
		uint32_t	KeyCode;		/* Key code (5-bit) */
		uint32_t	Algo;			/* Algorithm (3-bit) */
		uint32_t	AMS;			/* LFO-AM sensitivity (2-bit) */
		uint32_t	PMS;			/* LFO-PM sensitivity (3-bit) */
		uint32_t	FB;				/* Feedback (3-bit) */
		uint32_t	MaskL;			/* Channel L output mask */
		uint32_t	MaskR;			/* Channel R output mask */
	};

	/* Timer data type */
	struct timer_t
	{
		uint32_t	Load;			/* Start / stop state */
		uint32_t	Enable;			/* Overflag flag generation enable */
		uint32_t	Period;			/* Period */
		uint32_t	Counter;		/* Counter */
	};

	/* LFO data type */
	struct lfo_t
	{
		uint32_t	Enable;			/* Enable flag */
		uint32_t	Period;			/* Period */
		uint32_t	Counter;		/* Counter */
		uint32_t	Step;			/* Step counter (7-bit) */
	};

	static uint16_t ExpTable[256];
	static uint16_t SineTable[512];
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

	/* Build all OPL related tables */
	static void BuildTables()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			/*
				Build sine & exponent tables
			*/
			for (uint32_t i = 0; i < 256; i++)
			{
				SineTable[i + 000] = YM::GenerateSine(i, 256);
				SineTable[i + 256] = YM::GenerateSine(i ^ 0xFF, 256);

				ExpTable[i] = (GenerateExponent(i ^ 0xFF) | 0x400) << 2;
			}

			/*
				Build LFO-AM table
			*/

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
					YM::OPN::LfoAmTable[lfo][ams] = step >> LfoAmShift[ams];
				}
			}

			/*
				Build LFO-PM table
			*/

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
						int32_t value = (fnum >> LfoPmShift1[pms][step]) + (fnum >> LfoPmShift2[pms][step]);
						value >>= LfoPmShift3[pms];

						YM::OPN::LfoPmTable[fnum][lfo][pms] = (lfo & 0x10) ? -value : value;
					}
				}
			}

			Initialized = true;
		}
	}
}
#endif // !_YM_OPN_H_