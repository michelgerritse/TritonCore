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
#include "Devices/Sound/NullSound.h"

NullSound::NullSound(uint32_t Model, uint32_t Flags)
{

}

const wchar_t* NullSound::GetDeviceName()
{
	return L"NULL Sound Device";
}

void NullSound::Reset(ResetType Type)
{
	/* Do nothing */
}

uint32_t NullSound::GetOutputCount()
{
	return 0;
}

uint32_t NullSound::GetSampleRate(uint32_t ID)
{
	return 0;
}

uint32_t NullSound::GetSampleFormat(uint32_t ID)
{
	return 0;
}

uint32_t NullSound::GetChannelMask(uint32_t ID)
{
	return 0;
}

const wchar_t* NullSound::GetOutputName(uint32_t ID)
{
	return nullptr;
}

void NullSound::SetClockSpeed(uint32_t ClockSpeed)
{
	/* Do nothing */
}

uint32_t NullSound::GetClockSpeed()
{
	return 0;
}

void NullSound::Write(uint32_t Address, uint32_t Data)
{
	/* Do nothing */
}

void NullSound::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	/* Do nothing */
}