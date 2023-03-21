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
#ifndef _NULLSOUND_H_
#define _NULLSOUND_H_

#include "Interfaces/ISoundDevice.h"
#include "Interfaces/IAudioBuffer.h"

/* NULL sound generator */
class NullSound : public ISoundDevice
{
public:
	NullSound(uint32_t Model = 0, uint32_t Flags = 0);
	~NullSound() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName();
	void			Reset(ResetType Type);

	/* ISoundDevice methods */
	uint32_t		GetOutputCount();
	uint32_t		GetSampleRate(uint32_t ID);
	uint32_t		GetSampleFormat(uint32_t ID);
	uint32_t		GetChannelMask(uint32_t ID);
	const wchar_t*	GetOutputName(uint32_t ID);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);
};

#endif // !_NULLSOUND_H_