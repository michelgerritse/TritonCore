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

/* NULL sound device */
class NullSound : public ISoundDevice
{
public:
	NullSound() {};
	~NullSound() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName()
	{
		return L"NULL Sound Device";
	}
	
	void Reset(ResetType Type)
	{
	};

	/* ISoundDevice methods */
	bool EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
	{
		return false;
	}
	
	void SetClockSpeed(uint32_t ClockSpeed)
	{
	}
	
	uint32_t GetClockSpeed()
	{
		return 0;
	}
	
	void Write(uint32_t Address, uint32_t Data)
	{
	}
	
	void Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
	{
	}
};

#endif // !_NULLSOUND_H_