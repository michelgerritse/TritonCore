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
#ifndef _YM3014_H_
#define _YM3014_H_

#include "..\..\..\TritonCore.h"

/* Yamaha YM3014 (DAC-SS) */
class YM3014
{
public:
	YM3014() = default;
	~YM3014() = default;

	int16_t Process(uint16_t Data);

private:

};

#endif // !_YM3014_H_