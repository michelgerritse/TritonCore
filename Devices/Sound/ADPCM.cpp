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

namespace OKI::ADPCM
{
	static int16_t StepSize[49 * 16];

	void InitDecoder()
	{
		static bool Initialized = false;

		if (!Initialized)
		{
			const int16_t Size[49] =
			{
				/* floor(16 * pow(1.1, Step)) */
				
				16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
				50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
				157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449,
				494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552
			};

			for (auto Step = 0; Step < 49; Step++)
			{
				auto Value = Size[Step];

				for (auto Nibble = 0; Nibble < 16; Nibble++)
				{
					int16_t Diff = ( (2 * (Nibble & 0x07) + 1) * Value) >> 3;
					StepSize[(Step << 4) | Nibble] = (Nibble & 0x08) ? -Diff : Diff;
				}
			}

			Initialized = true;
		}
	}

	int16_t Decode(uint8_t Nibble, int32_t Step)
	{
		return StepSize[(Step << 4) | Nibble];
	}
	
	int32_t AdjustStep(uint8_t Nibble, int32_t Step)
	{
		static const int32_t StepAdjust[16] =
		{
			-1, -1, -1, -1, 2, 4, 6, 8,
			-1, -1, -1, -1, 2, 4, 6, 8
		};

		return std::clamp<int32_t>(Step + StepAdjust[Nibble], 0, 48);
	}
}