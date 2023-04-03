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
#ifndef _ADPCM_H_
#define _ADPCM_H_

#include "../../TritonCore.h"

namespace OKI::ADPCM
{
	/* Initialize the Dialogic ADPCM decoder */
	void InitDecoder();
	
	/* Decode a nibble for the current step, return the calculated difference */
	int16_t Decode(uint8_t Nibble, int32_t Step);
	
	/* Adjust the step for the next decoding pass, limit the resulting step to 0 - 48 */
	int32_t AdjustStep(uint8_t Nibble, int32_t Step);
}

#endif // !_ADPCM_H_