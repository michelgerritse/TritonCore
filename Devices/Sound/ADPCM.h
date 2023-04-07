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
	
	/* Decode a nibble */
	void Decode(uint8_t Nibble, int32_t* pStep, int16_t* pSignal);
}

namespace YM::ADPCMZ
{
	/* Decode a nibble */
	void Decode(uint8_t Nibble, int32_t* pStep, int16_t* pSignal);
}

#endif // !_ADPCM_H_