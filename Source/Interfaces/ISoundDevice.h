/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023, Michel Gerritse
All rights reserved.

This source code is available under the BSD-style license.
See LICENSE.txt file in the root directory of this source tree.

*/
#ifndef _ISOUNDDEVICE_H_
#define _ISOUNDDEVICE_H_

#include "IDevice.h"

/* Abstract sound device interface */
struct __declspec(novtable) ISoundDevice : public IDevice
{
	virtual uint32_t GetClockSpeed() = 0;
};

#endif // !_SOUNDIDEVICE_H_