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
#ifndef _YM_GEW_H_
#define _YM_GEW_H_

#include <algorithm>
#include <cmath>
#include <numbers>

namespace YM::GEW8 /* Yamaha - GEW8 */
{
	/* Exponent generator */
	static uint16_t GenerateExponent(uint32_t Value)
	{
		/*
		x = [0:255]
		y = round((power(2, x / 256) - 1) * 1024)

		The exponent table has been re-constructed from actual YM3812 die shots:
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665
		*/

		return (uint16_t)round((exp2(Value / 256.0) - 1) * 1024.0);
	};

	/* Maximum attenuation level */
	constexpr uint32_t MaxAttenuation = 0x3FF;

	/* Maximum envelope level */
	constexpr uint32_t MaxEgLevel = MaxAttenuation & ~((1 << 4) - 1);

	/* Channel data type */
	struct channel_t
	{
		uint32_t	KeyState;		/* Key on/off state */
		uint32_t	KeyLatch;		/* Latched key on/off flag */
		
		pair16_t	WaveNr;			/* Wave table number (9-bit) */
		uint32_t	FNum;			/* Frequency number (10-bit) */
		uint32_t	FNum9;			/* Copy of FNum bit 9 */
		int32_t		Octave;			/* Octave (signed 4-bit) */
		
		uint32_t	TotalLevel;		/* Total level (7-bit: 3.4) */
		uint32_t	DecayLvl;		/* Decay level (5-bit: 4.1) */
		uint32_t	TargetTL;		/* Interpolated TL */
		uint32_t	PanAttnL;		/* Pan attenuation left */
		uint32_t	PanAttnR;		/* Pan attenuation right */

		uint32_t	EgPhase;		/* Envelope phase */
		uint32_t	EgRate[4];		/* Envelope rates (4-bit) */
		uint32_t	EgRateCorrect;	/* Rate correction (4-bit) */
		uint32_t	EgLevel;		/* Envelope internal level (10-bit: 4.6) */
		uint32_t	EgOutputL;		/* Envelope output (left)  (12-bit: 4.8) */
		uint32_t	EgOutputR;		/* Envelope output (right) (12-bit: 4.8) */

		uint32_t	SampleCount;	/* Sample address (whole part) */
		uint32_t	SampleDelta;	/* Sample address (fractional) */
		uint32_t	PgReset;		/* Phase reset flag */

		uint32_t	Format;			/* Wave format (2-bit) */
		uint32_t	Start;			/* Start address (22-bit) */
		uint32_t	Loop;			/* Loop address (16-bit) */
		uint32_t	End;			/* End address (16-bit) */

		uint32_t	LfoCounter;		/* LFO counter */
		uint32_t	LfoPeriod;		/* LFO period */
		uint8_t		LfoStep;		/* LFO step counter (8-bit) */
		uint32_t	PmDepth;		/* Vibrato depth (3-bit) */
		uint32_t	AmDepth;		/* Tremolo depth (3-bit) */

		int16_t		SampleT0;		/* Sample interpolation T0 */
		int16_t		SampleT1;		/* Sample interpolation T1 */
		int16_t		Sample;			/* Interpolated sample */
		int16_t		OutputL;		/* Channel output (left) */
		int16_t		OutputR;		/* Channel output (right) */
	};

	static uint16_t ExpTable[256];
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
		0, 32, 64, 96, 128, 160, 192, MaxAttenuation, MaxAttenuation, 0, 0, 0, 0, 0, 0, 0
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
		0, 0, 0, 0, 0, 0, 0, 0, MaxAttenuation, MaxAttenuation, 192, 160, 128, 96, 64, 32
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

	/* Build all GEW8 related tables */
	static void BuildTables()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			/*
				Build exponent table:
				1. Reverse the original table
				2. Set the implicit bit10
				3. Shift left by 2
			*/
			for (uint32_t i = 0; i < 256; i++)
			{
				ExpTable[i] = (GenerateExponent(i ^ 0xFF) | 0x400) << 2;
			}

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
					TremoloTable[lfo][ams] = (step * LfoAmDepth[ams]) >> 7;
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
					int32_t value = ((step & 0x0F) * LfoPmDepth[pms]) >> 4;

					VibratoTable[lfo][pms] = (lfo & 0x20) ? -value : value;
				}
			}

			Initialized = true;
		}
	}
}
#endif // !_YM_GEW_H_