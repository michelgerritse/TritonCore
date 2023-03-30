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
#include "SegaPCM.h"

/*
	SegaPCM (315 - 5218)
	- 16 PCM channels
	- 8-bit unsigned PCM data
	- Per channel (L and R) pan data
	- 100 pin package
	- 12-bit output (depending on the DAC, most boards seem to use 10-bit DACs)

	The TTL version is used in the following arcade hardware:
	- Sega Hang-On hardware
	- Sega Space Harrier hardware

	The CMOS version is used in the following arcade hardware:
	- Sega OutRun hardware
	- Sega X Board
	- Sega Y Board

	The banking hardware can access 64 banks of 64KB, for a total
	ROM size of 4MB (22-bit). A maximum of 6-bits can be used
	per channel to set the current ROM bank

	There are 256 registers, 16 for each channel. The registers are
	split into 2 banks. Bit 7 is the bank select

	The register address map is as follows:
		0x00 - 0x07: Channel 1 - register bank 1
		0x08 - 0x0F: Channel 2 - register bank 1
		.
		.
		0x70 - 0x77: Channel 15 - register bank 1
		0x78 - 0x7F: Channel 16 - register bank 1

		0x80 - 0x87: Channel 1 - register bank 2
		0x88 - 0x8F: Channel 2 - register bank 2
		.
		.
		0xF0 - 0xF7: Channel 15 - register bank 2
		0xF8 - 0xFF: Channel 16 - register bank 2

	Register bank 1 layout:
		0x00: Unknown
		0x01: Unknown
		0x02: Panpot left (7-bit)
		0x03: Panpot right (7-bit)
		0x04: Loop address LSB
		0x05: Loop address MSB
		0x06: End address MSB
		0x07: Frequency delta (8-bit)

	Register bank 2 layout:
		0x00: Unknown
		0x01: Unknown
		0x02: Unknown
		0x03: Unknown
		0x04: Current address LSB
		0x05: Current address MSB
		0x06: Channel control and banking
		0x07: Unknown

	In reality, the SegaPCM doesn't have internal registers but
	it uses 2 external SRAM chips. 1 for each register bank.

	Note: The following information is taken from the OutRun
	schematics. It might differ for other boards
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

	Things that need validation:
	----------------------------
	- When is the fractional part of the 16.8 sample address reset ? I assume on address (MSB and LSB) write
	- Where is the factional part stored ? In SRAM or internal to the chip ? The former would indicate
	  it being able to be accessed from the register banks.
	- What do the unknown registers do ? Some get written a lot. Envelope data ? Other features ?

	Credits:
	--------
	MAME: register bank layout:
	https://github.com/mamedev/mame

*/

SegaPCM::SegaPCM(uint32_t Flags) :
	m_ClockDivider(128),
	m_Shift(8), /* 16.8 fixed point address format */
	m_BankShift(0),
	m_BankMask(0),
	m_OutputMask(~((1 << (16 - 10)) - 1)) /* 10 or 12 bit DAC ?*/
{
	/* Setup memory banking parameters */
	if (Flags)
	{
		m_BankShift = Flags & 0x0F;
		m_BankMask = (0x70 | ((Flags >> 16) & 0xFC));
	}

	/* Calculate memory size */
	uint32_t Size = (1 << TritonCore::GetParity(m_BankMask)) << 16;

	/* Set memory size */
	m_Memory.resize(Size);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* SegaPCM::GetDeviceName()
{
	return L"SegaPCM (315-5218)";
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

bool SegaPCM::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == 0)
	{
		Desc.SampleRate	= m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"";

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
	auto Offset = (Address & 0x7F) >> 3;
	auto Register = Address & 0x07;
	auto& Channel = m_Channel[Offset];

	Data &= 0xFF;

	if ((Address & 0x80) == 0) /* Register bank 1 (0x00 - 0x7F) */
	{
		switch (Register)
		{
		case 0x00: /* Unknown */
			//Note: This get written a lot by multiple games
			break;

		case 0x01: /* Unknown */
			break;

		case 0x02: /* Panpot Left (7-bit) */
			Channel.PANL = Data & 0x7F;
			break;

		case 0x03: /* Panpot Right (7-bit) */
			Channel.PANR = Data & 0x7F;
			break;

		case 0x04: /* Loop Address (LSB) */
			Channel.LS = (Channel.LS & 0xFF00) | Data;
			break;

		case 0x05: /* Loop Address (MSB) */
			Channel.LS = (Data << 8) | (Channel.LS & 0x00FF);
			break;

		case 0x06: /* End Address (MSB) */
			Channel.END = ((Data + 1) & 0xFF) << 8; /* END address is inclusive */
			break;

		case 0x07: /* Frequency Delta */
			Channel.FD = Data;
			break;
		}
	}
	else /* Register bank 2 (0x80 - 0xFF) */
	{
		switch (Register)
		{
		case 0x00: /* Unknown */
			break;

		case 0x01: /* Unknown */
			break;

		case 0x02: /* Unknown */
			break;

		case 0x03: /* Unknown */
			break;

		case 0x04: /* Current Address (LSB) */
			Channel.ADDR = (Channel.ADDR & 0xFF0000) | (Data << 8);
			break;

		case 0x05: /* Current Address (MSB) */
			Channel.ADDR = (Channel.ADDR & 0x00FF00) | (Data << 16);
			break;

		case 0x06: /* Channel Control + Banking */
			Channel.ON = (~Data & 0x01);
			Channel.LOOP = (~Data & 0x02);
			Channel.BANK = (Data & m_BankMask) << m_BankShift;
			break;

		case 0x07: /* Unknown */
			break;
		}
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

	while (Samples != 0)
	{
		OutL = 0;
		OutR = 0;

		for (CHANNEL& Channel : m_Channel)
		{
			if (Channel.ON)
			{
				/* Lookup PCM data */
				PCM = m_Memory[Channel.BANK | (Channel.ADDR >> m_Shift)];

				/* Convert from 8-bit unsigned to signed */
				PCM = PCM - 0x80;

				/* Increase address counter (16.8 fixed point) */
				Channel.ADDR = (Channel.ADDR + Channel.FD) & 0x00FFFFFF;

				/* Check for end address  */
				if ((Channel.ADDR >> m_Shift) == Channel.END)
				{
					if (Channel.LOOP) /* Loop Enabled */
					{
						/* Load loop address */
						Channel.ADDR = Channel.LS << m_Shift;
					}
					else /* Loop Disabled */
					{
						/* Stop Channel */
						Channel.ON = 0;
					}
				}

				/* Apply panning and accumulate in output buffer */
				OutL += PCM * Channel.PANL;
				OutR += PCM * Channel.PANR;
			}
		}

		/* Limiter (signed 16-bit) */
		OutL = std::clamp(OutL, -32768, 32767);
		OutR = std::clamp(OutR, -32768, 32767);

		/* 10 / 12 bit DAC output (interleaved) */
		OutBuffer[0]->WriteSampleS16(OutL & m_OutputMask);
		OutBuffer[0]->WriteSampleS16(OutR & m_OutputMask);

		Samples--;
	}
}

void SegaPCM::CopyToMemory(size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void SegaPCM::CopyToMemoryIndirect(size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(Offset, Data, Size);
}