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
#ifndef _IAUDIO_BUFFER_H_
#define _IAUDIO_BUFFER_H_

#include <cstdint>

/* Abstract audio buffer interface */
struct __declspec(novtable) IAudioBuffer
{
	virtual void WriteSampleS16(int16_t Sample) = 0;
};

#endif // !_IAUDIO_BUFFER_H_