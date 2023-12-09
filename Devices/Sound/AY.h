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
#ifndef _AY_H_
#define _AY_H_

#include <cstdint>

namespace AY /* AY8910 family / clones */
{
	/* Data type for tone generator */
	struct tone_t
	{
		uint32_t	Counter;		/* Tone counter (12-bit) */
		pair32_t	Period;			/* Tone period (12-bit) */
		uint32_t	Output;			/* Tone output (1-bit) */
		int16_t		Amplitude;		/* Tone amplitude */
		uint32_t	ToneDisable;	/* Tone mixer control (1-bit) */
		uint32_t	NoiseDisable;	/* Noise mixer control (1-bit) */
		uint32_t	AmpCtrl;		/* Amplitude control mode (1-bit) */
	};

	/* Noise generator data type */
	struct noise_t
	{
		uint32_t	Counter;		/* Noise counter (12-bit) */
		uint32_t	Period;			/* Noise period (12-bit) */
		uint32_t	Output;			/* Noise output (1-bit) */
		uint32_t	Prescaler;		/* Noise /2 prescaler */
		uint32_t	LFSR;			/* Shift register (17-bit) */
	};

	/* Envelope generator data type */
	struct envelope_t
	{
		uint32_t	Counter;		/* Envelope counter (16-bit) */
		pair32_t	Period;			/* Envelope period (16-bit) */
		int16_t		Amplitude;		/* Envelope volume */
		uint32_t	Prescaler;		/* Envelope /2 prescaler */
		uint32_t	Step;			/* Current envelope step (0 - 15) */
		uint32_t	StepDec;		/* Envelope step decrement */
		uint32_t	Hld;			/* Envelope hold bit */
		uint32_t	Alt;			/* Envelope alternate bit */
		uint32_t	Inv;			/* Envelope output inversion */
	};

	/* SSG data type (OPN chips) */
	struct ssg_t
	{
		tone_t		Tone[3];
		noise_t		Noise;
		envelope_t	Envelope;
		uint8_t		Register[16];
	};

	/* Mask table for unused / undefined register bits (AY only) */
	static const uint32_t Mask[16] =
	{
		0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF
	};

	/* 0.2V DC offset for channels with envelope enabled (AY only) */
	constexpr int16_t DCOffset02V = (int16_t)(double)((0.2) * (32767.f / 5.f));
	
	/* Amplitude table (AY variants) */
	static const int16_t Amplitude16[16] =
	{
		/*	AY-3-8910 output measurements:
			https://github.com/michelgerritse/YM-research/blob/main/AY8910%20-%20Output.xlsx
		*/
		
#define V(x) {(int16_t) (double) ((x) * (32767.f / 5.f))}

		V(0.000), V(0.015), V(0.022), V(0.031), V(0.045), V(0.066), V(0.091), V(0.152),
		V(0.189), V(0.310), V(0.426), V(0.560), V(0.735), V(0.913), V(1.173), V(1.433)

#undef V
	};

	/* Amplitude table (YM variants) */
	static const int16_t Amplitude32[32] =
	{
		/*	YM2149 output measurements:
			https://github.com/michelgerritse/YM-research/blob/main/AY8910%20-%20Output.xlsx
		*/

#define V(x) {(int16_t) (double) ((x) * (32767.f / 5.f))}

		V(0.000), V(0.008), V(0.012), V(0.017), V(0.020), V(0.024), V(0.027), V(0.031),
		V(0.036), V(0.042), V(0.048), V(0.054), V(0.064), V(0.074), V(0.085), V(0.096),
		V(0.115), V(0.134), V(0.155), V(0.177), V(0.212), V(0.248), V(0.288), V(0.328),
		V(0.395), V(0.464), V(0.539), V(0.617), V(0.741), V(0.871), V(1.005), V(1.146)

#undef V
	};

	/* Map 4-bit amplitude level to 5-bit (YM only) */
	static const uint32_t MapLvl4to5[16] =
	{
		/*
			We assume:
			Lvl 00 = 00
			Lvl 15 = 31
		
			This contradicts the YM2608 manual (page38) , where it is stated that:
			+-------+-------+-------+-------+-------+-------+
			|		|	D3	|	D2	|	D1	|	D0	|		|
			|-------+-------+-------+-------+-------+-------|
			| Data	|	L5	|	L4	|	L3	|	L2	|	L1	|
			+-------+-------+-------+-------+-------+-------+
			| Level	|	16	|	8	|	4	|	2	|	1	|
			+-------+-------+-------+-------+-------+-------+
			So the formula would be: Lvl = (Lvl << 1) | 1
		*/
		0, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31
	};


/* Test code to validate the envelope generator output for all possible shapes
#include <cstdio>
#include <cstdint>

struct AYENVELOPE
{
	uint32_t	Step;
	uint32_t	StepDec;
	uint32_t	Hld;
	uint32_t	Alt;
	uint32_t	Inv;
};

int main()
{
	AYENVELOPE m_Envelope;

	for (auto i = 0; i < 16; i++)
	{
		//Simulate register write
		uint32_t Data = i;

		m_Envelope.Step = 15;
		m_Envelope.StepDec = 1;

		m_Envelope.Inv = (Data & 0x04) ? 15 : 0;

		if (Data & 0x08) //Continuous cycles
		{
			m_Envelope.Hld = Data & 0x01;

			if (m_Envelope.Hld)
				m_Envelope.Alt = (Data & 0x02) ? 0 : 15;
			else
				m_Envelope.Alt = (Data & 0x02) ? 15 : 0;
		}
		else //Single cycle
		{
			m_Envelope.Hld = 1;
			m_Envelope.Alt = m_Envelope.Inv ^ 15;
		}

		//Simulate envelope period expiration
		printf("0x%X - %.2d ", Data, m_Envelope.Step ^ m_Envelope.Inv);

		for (auto j = 0; j < 64; j++)
		{
			//Count down step counter (15 -> 0)
			m_Envelope.Step -= m_Envelope.StepDec;

			if (m_Envelope.Step & 16)
			{
				m_Envelope.Step = 15;
				m_Envelope.StepDec = m_Envelope.Hld ^ 1;
				m_Envelope.Inv ^= m_Envelope.Alt;
			}

			printf("%.2d ", m_Envelope.Step ^ m_Envelope.Inv);
		}

		printf("\n");
	}
}

Console output:

0x0 - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x1 - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x2 - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x3 - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x4 - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x5 - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x6 - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x7 - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0x8 - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 15
0x9 - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
0xA - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 15
0xB - 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15
0xC - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00
0xD - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15 15
0xE - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 15 14 13 12 11 10 09 08 07 06 05 04 03 02 01 00 00
0xF - 00 01 02 03 04 05 06 07 08 09 10 11 12 13 14 15 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
*/

} // namespace AY

#endif // !_AY_H_