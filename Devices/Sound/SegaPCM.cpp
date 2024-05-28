/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023 - 2024, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#include "SegaPCM.h"

/*
	SegaPCM (315-5218)
	- 16 PCM channels
	- 8-bit unsigned PCM data
	- Per channel (L and R) pan data
	- 100 pin package
	- 12-bit output (depending on the DAC, most boards seem to use 10-bit DACs)

	The TTL version is used in the following arcade hardware:
	- Sega Hang-On hardware

	The CMOS version is used in the following arcade hardware:
	- Sega OutRun hardware
	- Sega X Board
	- Sega Y Board

	The banking hardware can access 64 banks of 64KB, for a total
	ROM size of 4MB (22-bit). A maximum of 6-bits can be used
	per channel to set the current ROM bank

	There are 256 registers, 16 for each channel. The registers are
	split into 2 banks. Bit 7 is the bank select

	The following information is taken from the OutRun schematics:
	http://arcarc.xmission.com/PDF_Arcade_Manuals_and_Schematics/OutRun_Schematics_Cleaned.pdf

	4KB SRAM (2x Toshiba TMM2015 2KB SRAM)
	Only address pins A0-A6 are connected (A7-A10 are grounded)
	so effectively only 128 bytes per SRAM module are used.

	The banking hardware is a Texas Instruments LS137
	3-line to 8-line decoder/demultiplexer with address latches

	The DAC is a National Semiconductor DAC1022
	10-Bit Binary Multiplying D/A Converter

	The DAC ouputs to two National Semiconductor MF6-50
	6th Order Switched Capacitor Butterworth Lowpass Filter

	Credits:
	--------
	MAME: register bank layout:
	https://github.com/mamedev/mame

*/

/* Audio output enumeration */
enum AudioOut
{
	Default = 0
};

/* Static class member initialization */
const std::wstring SegaPCM::s_DeviceName = L"SegaPCM (315-5218)";

SegaPCM::SegaPCM(uint32_t ClockSpeed, uint32_t BankFlags) :
	m_ClockSpeed(ClockSpeed), //NOTE: Should be fixed to 16MHz
	m_ClockDivider(128), //NOTE: Should be set to 512 for a 31250Hz sample rate @ 16MHz clock
	m_BankShift(BankFlags & 0x0F),
	m_BankMask(0x70 | (BankFlags >> 16) & 0xFC)
{
	/* Calculate memory size */
	auto Size = (1 << TC::GetParity(m_BankMask)) << 16;

	/* Set memory size */
	m_Memory.resize(Size);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* SegaPCM::GetDeviceName()
{
	return s_DeviceName.c_str();
}

void SegaPCM::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Reset all channels */
	memset(m_Channel, 0, sizeof(m_Channel));

	if (Type == ResetType::PowerOnDefaults)
	{
		/* Clear PCM memory */
		memset(m_Memory.data(), 0, m_Memory.size());
	}
}

void SegaPCM::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
}

bool SegaPCM::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == AudioOut::Default)
	{
		Desc.SampleRate		= m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat	= AudioFormat::AUDIO_FMT_S16;
		Desc.Channels		= 2;
		Desc.ChannelMask	= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description	= L"";

		return true;
	}
	
	return false;
}

void SegaPCM::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t SegaPCM::GetClockSpeed()
{
	return m_ClockSpeed;
}

void SegaPCM::Write(uint32_t Address, uint32_t Data)
{	
	Data &= 0xFF; /* 8-bit data bus */
	
	auto Offset   = TC::GetBitField(Address, 3u, 4u);
	auto Register = TC::GetBitField(Address, 0u, 3u);
	auto Bank     = TC::GetBit(Address, 7u);
	auto& Channel = m_Channel[Offset];

	switch ((Bank << 3) | Register)
	{
	case 0x00: /* Current Address [7:0] (validation needed) */
		Channel.Addr.u8ll = Data;
		break;

	case 0x02: /* Panpot Left (7-bit) */
		Channel.PanL = Data & 0x7F;
		break;

	case 0x03: /* Panpot Right (7-bit) */
		Channel.PanR = Data & 0x7F;
		break;

	case 0x04: /* Loop Address [15:8] */
		Channel.LoopAddr.u8lh = Data;
		break;

	case 0x05: /* Loop Address [23:16] */
		Channel.LoopAddr.u8hl = Data;
		break;

	case 0x06: /* End Address [23:16] */
		Channel.EndAddr = Data + 1;
		break;

	case 0x07: /* Frequency Delta */
		Channel.Delta = Data;
		break;

	case 0x0C: /* Current Address [15:8] */
		Channel.Addr.u8lh = Data;
		break;

	case 0x0D: /* Current Address [23:16] */
		Channel.Addr.u8hl = Data;
		break;

	case 0x0E: /* Channel Control + Banking */
		Channel.On   = (Data & 0x01) ? 0 : ~0;
		Channel.Loop = (Data & 0x02) ? 0 : ~0;
		Channel.Bank = (Data & m_BankMask) << m_BankShift;
		break;

	case 0x01: /* Not used (validation needed) */
	case 0x08:
	case 0x09:
	case 0x0A:
	case 0x0B:
	case 0x0F:
		break;
	}
}

void SegaPCM::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int32_t OutL;
	int32_t OutR;
	int8_t PCM;

	while (Samples--)
	{
		OutL = 0;
		OutR = 0;

		for (auto& Channel : m_Channel)
		{
			/* Load PCM data, convert from 8-bit unsigned to signed */
			PCM = (m_Memory[Channel.Bank | (Channel.Addr.u32 >> 8)] ^ 0x80) & Channel.On;

			/* Increase address counter (16.8 fixed point) */
			Channel.Addr = (Channel.Addr.u32 + Channel.Delta) & 0x00FFFFFF;

			if (Channel.Addr.u8hl == Channel.EndAddr)
			{
				/* Load loop address */
				Channel.Addr = Channel.LoopAddr.u32 | (Channel.Addr.u32 & 0xFF); /* Do we need to keep the 16.8 fractional part ? */
				
				/* Disable sound generation (if not looping) */
				Channel.On &= Channel.Loop;
			}

			/* Apply panning and accumulate in output buffer */
			OutL += (PCM * Channel.PanL);
			OutR += (PCM * Channel.PanR);
		}

		/* Limiter (signed 16-bit) */
		//OutL = std::clamp(OutL, -32768, 32767);
		//OutR = std::clamp(OutR, -32768, 32767);

		//TODO: Implement National Semiconductor DAC1022 (or similar 10-bit DAC) 
		OutBuffer[AudioOut::Default]->WriteSampleS16(OutL);
		OutBuffer[AudioOut::Default]->WriteSampleS16(OutR);
	}
}

void SegaPCM::CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void SegaPCM::CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(MemoryID, Offset, Data, Size);
}