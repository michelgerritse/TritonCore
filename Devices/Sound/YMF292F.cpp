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
#include "YMF292F.h"

/*
	Yamaha YMF292-F (Saturn Custom Sound Processor)
*/

/* Audio output enumeration */
enum AudioOut
{
	Default = 0
};

/* Envelope phases */
enum ADSR : uint32_t
{
	Attack = 0,
	Decay,
	Sustain,
	Release
};

/* Static class member initialization */
const std::wstring YMF292F::s_DeviceName = L"Yamaha YMF292-F";

YMF292F::YMF292F(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(512)
{
	/* Set memory size to 512KB */
	m_Memory.resize(0x80000);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YMF292F::GetDeviceName()
{
	return s_DeviceName.c_str();
}

void YMF292F::Reset(ResetType Type)
{
	m_CyclesToDo = 0;
}

void YMF292F::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
}

bool YMF292F::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == AudioOut::Default)
	{
		Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"";

		return true;
	}

	return false;
}

void YMF292F::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YMF292F::GetClockSpeed()
{
	return m_ClockSpeed;
}

void YMF292F::Write(uint32_t Address, uint32_t Data)
{
	/* VGM only interface */
}

void YMF292F::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	while (Samples-- != 0)
	{
		OutBuffer[AudioOut::Default]->WriteSampleS16(0);
		OutBuffer[AudioOut::Default]->WriteSampleS16(0);
	}
}

void YMF292F::CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void YMF292F::CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(MemoryID, Offset, Data, Size);
}