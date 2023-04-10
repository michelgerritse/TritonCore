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
#ifndef _IDEVICE_H_
#define _IDEVICE_H_

#include "TritonCore.h"

/* Reset type enumeration */
enum class ResetType : uint32_t
{
	/* Power ON defaults (eg. hard reset) */
	PowerOnDefaults = 0,

	/* Reset to default state (eg. /IC pin on a chip) */
	InitialClear,
	
	/* Soft reset (eg. reset button) */
	Soft
};

/* Abstract base interface */
struct __declspec(novtable) IDevice
{
	/* Get device name from object */
	virtual const wchar_t* GetDeviceName() = 0;

	/* Reset device */
	virtual void Reset(ResetType Type) = 0;

	/* Send an exclusive command to the device */
	virtual void SendExclusiveCommand(uint32_t Command, uint32_t Value) = 0;
};

#endif // !_IDEVICE_H_