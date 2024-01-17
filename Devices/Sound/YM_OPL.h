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
#ifndef _YM_OPL_H_
#define _YM_OPL_H_

#include <algorithm>
#include <cmath>
#include <numbers>

namespace YM::OPL /* Yamaha - FM Operator Type-L */
{
	/* Sine generator */
	static uint16_t GenerateSine(uint32_t Value)
	{
		/*
		x = [0:255]
		y = round(-log(sin((x + 0.5) * pi / 2 / 256)) / log(2) * 256)

		The sine table has been re-constructed from actual YM3812 die shots:
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665
		*/
		
		return (uint16_t) round(-log(sin((Value + 0.5) * std::numbers::pi / 2.0 / 256.0)) / std::numbers::ln2 * 256.0);
	};

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
		
		return (uint16_t) round((exp2(Value / 256.0) - 1) * 1024.0);
	};
	
	/* Maximum attenuation level */
	constexpr uint32_t MaxAttenuation = 0x1FF;

	/* Maximum envelope level */
	constexpr uint32_t MaxEgLevel = MaxAttenuation & ~7;

	/*
		This constant defines the total steps of the LFO-AM generator
		Frequency = 3.7Hz (OPL4 manual page 43)

		The constant is calculated as follows:
		Steps = (Clock / Divider) / (Period * Freq)

		Steps = (3.58Mhz / 4 / 18) / (64 * 3.7Hz)
	*/
	constexpr uint32_t LfoAmSteps = 210;
	constexpr uint32_t LfoAmPeriod = 64 - 1;

	/*
		This constant defines the total steps of the LFO-PM generator
		Frequency = 6.0Hz (OPL4 manual page 44)

		The constant is calculated as follows:
		Steps = (Clock / Divider) / (Period * Freq)

		Steps = (3.58Mhz / 4 / 18) / (1024 * 6.0Hz)
	*/
	constexpr uint32_t LfoPmSteps = 8 - 1;
	constexpr uint32_t LfoPmPeriod = 1024 - 1;

	/*
		This constant defines the resolution for Timer 1
		It is implemented as a mask over the global timer

		~80us = 1 / (Clock / Divider) * Period
	*/
	constexpr uint32_t Timer1Mask = 4 - 1;

	/*
		This constant defines the resolution for Timer 2
		It is implemented as a mask over the global timer

		~322us = 1 / (Clock / Divider) * Period
	*/
	constexpr uint32_t Timer2Mask = 16 - 1;

	/* Operator data type */
	struct operator_t
	{
		uint32_t	KeyState;		/* Key on/off state */
		uint32_t	CsmLatch;		/* Latched CSM key on/off flag */
		uint32_t	DrumLatch;		/* Latched drum key on/off flag */

		uint32_t	LfoAmOn;		/* LFO-AM on/off mask */
		uint32_t	LfoPmOn;		/* LFO-PM on/off mask */
		uint32_t	EgType;			/* Envelope type (1-bit) */
		uint32_t	KeyScaling;		/* Key scaling on/off flag */
		uint32_t	Multi;			/* Multiple (4-bit) */

		uint32_t	TotalLevel;		/* Total level (6-bit: 3.3) */
		uint32_t	SustainLvl;		/* Sustain level (5-bit: 4.1) */
		uint32_t	KeyScaleShift;	/* Key scale level shift */

		uint32_t	EgPhase;		/* Envelope phase */
		uint32_t	EgRate[4];		/* Envelope rates (4-bit) */
		uint32_t	EgLevel;		/* Envelope internal level (9-bit: 4.5) */
		uint32_t	EgOutput;		/* Envelope output (12-bit: 4.8) */

		uint32_t	PgPhase;		/* Phase counter (19-bit: 10.9) */
		uint32_t	PgOutput;		/* Phase output (10-bit) */
		uint32_t	PgReset;		/* Phase reset flag */

		uint16_t*	WaveTable;		/* Wave table pointer */
		uint16_t	WaveSign;		/* Wave sign mask */

		int16_t		Output[2];		/* Operator output (14-bit) */
	};

	/* Channel data type */
	struct channel_t
	{
		uint32_t	KeyLatch;		/* Latched Key on/off flag */
		uint32_t	FNum;			/* Frequency Nr. (10-bit) */
		uint32_t	Block;			/* Block (3-bit) */
		uint32_t	KeyCode;		/* Key code (4-bit) */
		uint32_t	Algo;			/* Algorithm (1-bit) */
		uint32_t	FB;				/* Feedback (3-bit) */
	};

	/* Timer data type */
	struct timer_t
	{
		uint32_t	Start;			/* Start / stop state */
		uint32_t	Mask;			/* Overflow flag mask */
		uint32_t	Period;			/* Period */
		uint32_t	Counter;		/* Counter */
	};

	static uint16_t ExpTable[256];
	static uint16_t WaveTable[4][1024];
	
	static const uint16_t WaveSign[8] =
	{
		0x200, 0, 0, 0, 0x200, 0, 0x200, 0x200
	};

	consteval uint32_t KSL(uint32_t Fnum, uint32_t Block)
	{
		constexpr uint32_t KSLROM[16] =
		{
			0, 32, 40, 45, 48, 51, 53, 55, 56, 58, 59, 60, 61, 62, 63, 64
		};

		int32_t Level = KSLROM[Fnum & 15] - ((8 - (Block & 7)) << 3);
		return std::max<int32_t>(0, Level) << 2;
	};

	/* Key scale level table */
	static const uint32_t KeyScaleLevel[16][8] =
	{
		{ KSL( 0,0), KSL( 0,1), KSL( 0,2), KSL( 0,3), KSL( 0,4), KSL( 0,5), KSL( 0,6), KSL( 0,7) },
		{ KSL( 1,0), KSL( 1,1), KSL( 1,2), KSL( 1,3), KSL( 1,4), KSL( 1,5), KSL( 1,6), KSL( 1,7) },
		{ KSL( 2,0), KSL( 2,1), KSL( 2,2), KSL( 2,3), KSL( 2,4), KSL( 2,5), KSL( 2,6), KSL( 2,7) },
		{ KSL( 3,0), KSL( 3,1), KSL( 3,2), KSL( 3,3), KSL( 3,4), KSL( 3,5), KSL( 3,6), KSL( 3,7) },
		{ KSL( 4,0), KSL( 4,1), KSL( 4,2), KSL( 4,3), KSL( 4,4), KSL( 4,5), KSL( 4,6), KSL( 4,7) },
		{ KSL( 5,0), KSL( 5,1), KSL( 5,2), KSL( 5,3), KSL( 5,4), KSL( 5,5), KSL( 5,6), KSL( 5,7) },
		{ KSL( 6,0), KSL( 6,1), KSL( 6,2), KSL( 6,3), KSL( 6,4), KSL( 6,5), KSL( 6,6), KSL( 6,7) },
		{ KSL( 7,0), KSL( 7,1), KSL( 7,2), KSL( 7,3), KSL( 7,4), KSL( 7,5), KSL( 7,6), KSL( 7,7) },
		{ KSL( 8,0), KSL( 8,1), KSL( 8,2), KSL( 8,3), KSL( 8,4), KSL( 8,5), KSL( 8,6), KSL( 8,7) },
		{ KSL( 9,0), KSL( 9,1), KSL( 9,2), KSL( 9,3), KSL( 9,4), KSL( 9,5), KSL( 9,6), KSL( 9,7) },
		{ KSL(10,0), KSL(10,1), KSL(10,2), KSL(10,3), KSL(10,4), KSL(10,5), KSL(10,6), KSL(10,7) },
		{ KSL(11,0), KSL(11,1), KSL(11,2), KSL(11,3), KSL(11,4), KSL(11,5), KSL(11,6), KSL(11,7) },
		{ KSL(12,0), KSL(12,1), KSL(12,2), KSL(12,3), KSL(12,4), KSL(12,5), KSL(12,6), KSL(12,7) },
		{ KSL(13,0), KSL(13,1), KSL(13,2), KSL(13,3), KSL(13,4), KSL(13,5), KSL(13,6), KSL(13,7) },
		{ KSL(14,0), KSL(14,1), KSL(14,2), KSL(14,3), KSL(14,4), KSL(14,5), KSL(14,6), KSL(14,7) },
		{ KSL(15,0), KSL(15,1), KSL(15,2), KSL(15,3), KSL(15,4), KSL(15,5), KSL(15,6), KSL(15,7) },
	};

	/* Key scale shift table */
	static const uint32_t KeyScaleShift[4] =
	{
		8, /* No damping  */
		1, /* 3.0dB / oct */
		2, /* 1.5dB / oct */
		0  /* 6.0dB / oct */

		/* Note: OPLL has 1 and 2 swapped */
	};

	/* Multiplication table */
	static const uint32_t Multiply[16] =
	{
		1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
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

	/* Rhythm phase input */
	static const uint32_t PhaseIn[32] =
	{
		/*
			Input:
			b4 = High hat phase bit7 (HH7)
			b3 = High hat phase bit3 (HH3)
			b2 = High hat phase bit2 (HH2)
			b1 = Top cymbal phase bit5 (TC5)
			b0 = Top cymbal phase bit3 (TC3)

			Output is generated by the following expression:
			PhaseIn = (HH7 ^ HH2) | (HH3 ^ TC5) | (TC5 ^ TC3)
		*/

		0, 1, 1, 1, 1, 1, 1, 1,
		1, 1, 1, 0, 1, 1, 1, 1,
		1, 1, 1, 1, 0, 1, 1, 1,
		1, 1, 1, 1, 1, 1, 1, 0
	};

	/* Snare drum phase output */
	static const uint32_t PhaseOutSD[4] =
	{
		/*
			Input:
			b1 = PhaseIn (HH8)
			b0 = NoiseOut

			Output is generated by the following expression:
			PhaseOut = (PhaseIn << 9) | ((PhaseIn ^ NoiseOut) << 8);
		*/

		0x000,
		0x100,
		0x300,
		0x200
	};

	/* High hat phase output */
	static const uint32_t PhaseOutHH[4] =
	{
		/*
			Input:
			b1 = PhaseIn
			b0 = NoiseOut

			Output is generated by the following expression:
			PhaseOut = (PhaseIn << 9) | (0xD0 >> ((PhaseIn ^ NoiseOut) << 1));
		*/

		0x0D0,
		0x234,
		0x034,
		0x2D0
	};

	/* Build all OPL related tables */
	static void BuildTables()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			/*
				Build wave tables
			*/
			for (uint32_t i = 0; i < 1024; i++)
			{
				auto Zero = 0x1000;

				/* Wave 0: Sine */
				if ((i & 0x100) == 0)
					WaveTable[0][i] = GenerateSine(i & 0xFF); /* 1st quarter */
				else
					WaveTable[0][i] = GenerateSine((i & 0xFF) ^ 0xFF); /* 2nd quarter */

				/* Wave 1: Half-sine */
				if ((i & 0x200) == 0)
					WaveTable[1][i] = WaveTable[0][i]; /* 1st half */
				else
					WaveTable[1][i] = Zero; /* 2nd half */

				/* Wave 2: Absolute-sine */
				WaveTable[2][i] = WaveTable[0][i];

				/* Wave 3: Quarter-sine */
				if ((i & 0x100) == 0)
					WaveTable[3][i] = WaveTable[0][i]; /* 1st quarter */
				else
					WaveTable[3][i] = Zero; /* 2nd quarter */
			}

			/*
				Build exponent table:
				1. Reverse the original table
				2. Set the implicit bit10
				3. Shift left by 1
			*/
			for (uint32_t i = 0; i < 256; i++)
			{
				ExpTable[i] = (GenerateExponent(i ^ 0xFF) | 0x400) << 1;
			}

			Initialized = true;
		}
	}
}
#endif // !_YM_OPL_H_