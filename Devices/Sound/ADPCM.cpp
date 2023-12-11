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
#include "ADPCM.h"

namespace Shared
{
	static const int16_t DeltaTable[16] =
	{
		/*
			Delta = 2 * (Nibble & 0x7) + 1
			if (Nibble & 8) Delta = -Delta
		*/
		 1,  3,  5,  7,  9,  11,  13,  15,
		-1, -3, -5, -7, -9, -11, -13, -15,
	};
}

namespace OKI::ADPCM
{
	/* Oki ADPCM decoder (a.k.a Dialogic ADPCM)

	   Reference:
	   https://wiki.multimedia.cx/index.php/Dialogic_IMA_ADPCM

	*/
	
	static int16_t DiffTable[49 * 16];

	void InitDecoder()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			const int16_t SizeTable[49] =
			{
				16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
				50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
				157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
				494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
			};

			for (auto Step = 0; Step < 49; Step++)
			{
				for (auto Nibble = 0; Nibble < 16; Nibble++)
				{
					auto Diff = (Shared::DeltaTable[Nibble] * SizeTable[Step]) / 8;
					DiffTable[(Step * 16) | Nibble] = Diff;
				}
			}

			Initialized = true;
		}
	}

	void Decode(uint8_t Nibble, int32_t* pStep, int16_t* pSignal)
	{
		static const int32_t StepAdjust[16] =
		{
			-1, -1, -1, -1, 2, 4, 6, 8,
			-1, -1, -1, -1, 2, 4, 6, 8
		};
		
		int32_t Step = *pStep;
		int16_t Signal = (*pSignal * 254) / 256;
		
		/* Adjust signal and clamp to 12-bit */
		int16_t Diff = DiffTable[(Step << 4) | Nibble];
		*pSignal = std::clamp<int16_t>(Signal + Diff, -2048, 2047);

		/* Adjust step for next decoding pass */
		int32_t NewStep = Step + StepAdjust[Nibble];
		*pStep = std::clamp(NewStep, 0, 48);
	}
}

namespace YM::ADPCMA
{
	/* Yamaha ADPCM-A decoder

	   Reference:
	   https://wiki.multimedia.cx/index.php/Yamaha_ADPCM

	*/
	
	static int16_t DiffTable[49 * 16];

	void InitDecoder()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			const int16_t SizeTable[49] =
			{
				16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
				50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
				157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
				494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
			};

			for (auto Step = 0; Step < 49; Step++)
			{
				for (auto Nibble = 0; Nibble < 16; Nibble++)
				{
					auto Diff = (Shared::DeltaTable[Nibble] * SizeTable[Step]) / 8;
					DiffTable[(Step * 16) | Nibble] = Diff << 4; /* 12 to 16-bit */
				}
			}

			Initialized = true;
		}
	}

	void Decode(uint8_t Nibble, int32_t* pStep, int16_t* pSignal)
	{
		static const int32_t StepAdjust[16] =
		{
			-1 * 16, -1 * 16, -1 * 16, -1 * 16, 2 * 16, 5 * 16, 7 * 16, 9 * 16,
			-1 * 16, -1 * 16, -1 * 16, -1 * 16, 2 * 16, 5 * 16, 7 * 16, 9 * 16
		};

		int32_t Step = *pStep;
		int16_t Signal = *pSignal << 4; /* 12 to 16-bit */

		/* Adjust signal and limit to 12-bit */
		Signal += DiffTable[Step + Nibble];
		*pSignal = Signal >> 4;

		/* Adjust step for next decoding pass */
		int32_t NewStep = Step + StepAdjust[Nibble];
		*pStep = std::clamp(NewStep, 0, 48 * 16);
	}
}

namespace YM::ADPCMB
{
	/* Yamaha ADPCM-B decoder

	   Reference:
	   https://wiki.multimedia.cx/index.php/Yamaha_ADPCM

	*/

	void Decode(uint8_t Nibble, int32_t* pStep, int16_t* pSignal)
	{
		static const int32_t StepScale[16] =
		{
			/* YM2608 manual page 44 */
			57, 57, 57, 57, 77, 102, 128, 153,
			57, 57, 57, 57, 77, 102, 128, 153
		};

		int32_t Step = *pStep;
		int32_t Signal = *pSignal;

		/* Adjust signal and clamp to 16-bit */
		int32_t Diff = (Shared::DeltaTable[Nibble] * Step) / 8;
		*pSignal = std::clamp<int32_t>(Signal + Diff, -32768, 32767);

		/* Adjust step for next decoding pass */
		int32_t NewStep = (StepScale[Nibble] * Step) >> 6;
		*pStep = std::clamp(NewStep, 127, 24576);
	}
}

namespace YM::ADPCMZ
{
	/* Yamaha AICA ADPCM decoder 
	
	   Reference:
	   https://wiki.multimedia.cx/index.php/Creative_ADPCM

	*/

	void Decode(uint8_t Nibble, int32_t* pStep, int16_t* pSignal)
	{
		static const int32_t StepScale[16] =
		{
			230, 230, 230, 230, 307, 409, 512, 614,
			230, 230, 230, 230, 307, 409, 512, 614
		};

		int32_t Step = *pStep;
		int32_t Signal = (*pSignal * 254) / 256;

		/* Adjust signal and clamp to 16-bit */
		int32_t Diff = (Shared::DeltaTable[Nibble] * Step) / 8;
		*pSignal = std::clamp<int32_t>(Signal + Diff, -32768, 32767);

		/* Adjust step for next decoding pass */
		int32_t NewStep = (StepScale[Nibble] * Step) >> 8;
		*pStep = std::clamp(NewStep, 127, 24576);
	}
}