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
#ifndef _IDEVICE_H_
#define _IDEVICE_H_

#include "IBase.h"

/* Abstract base interface */
struct __declspec(novtable) IDevice : public IBase
{
	/* Get device name from object */
	virtual const wchar_t* GetDeviceName() = 0;
};

#endif // !_IDEVICE_H_