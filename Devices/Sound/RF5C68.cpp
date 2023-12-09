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
#include "RF5C68.h"

/*
	The RF5C164 is identical to the RF5C68, with the following exceptions:
	- It allows for higher clock speeds (12.5MHz vs 10MHz)
	- You can read the current wave address for each channel from address 0x10 - 0x1F
	- It can interface with 128K MROM (RAMAX is used) to give 17.10 fixed point (instead of 16.11)
	- 16-bit vs 10-bit output to DAC

	Known models:
	- RF5C68
	- RF5C164

	Apparently there also is this model:
	- RF5C105 (only mentioned in the MegaCD manual, no other datasheet/information available)
*/

RF5C68::RF5C68(uint32_t Model, bool UseRAMAX) :
	m_ClockDivider(384),
	m_Shift(11),
	m_Model(Model)
{
	uint32_t Size = 64 * 1024; /* Default to 64KB */

	/* Initialize model specific variables */
	switch (m_Model)
	{
		case MODEL_RF5C68:
		default:
			m_OutputMask = ~0x3F; /* 10-bit DAC output */
			break;

		case MODEL_RF5C164:
			m_OutputMask = ~0x00; /* 16-bit DAC output */

			if (UseRAMAX) /* 17.10 address format */
			{
				Size = 128 * 1024;
				m_Shift = 10;
			}
			break;
	}

	m_Memory.resize(Size);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* RF5C68::GetDeviceName()
{
	switch (m_Model)
	{
		case MODEL_RF5C68:
		default:
			return L"Ricoh RF5C68";

		case MODEL_RF5C164:
			return L"Ricoh RF5C164";
	}
}

void RF5C68::Reset(ResetType Type)
{
	m_CyclesToDo = 0;
	
	m_Sounding = 0;
	m_ChannelBank = 0;
	m_WaveBank = 0;
	m_ChannelCtrl = 0xFF;

	/* Reset all channels */
	memset(m_Channel, 0, sizeof(m_Channel));

	if (Type == ResetType::PowerOnDefaults)
	{
		/* Clear PCM memory */
		memset(m_Memory.data(), 0, m_Memory.size());
	}
}

void RF5C68::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	/* No implementation needed */
}

bool RF5C68::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == 0)
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

void RF5C68::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t RF5C68::GetClockSpeed()
{
	return m_ClockSpeed;
}

void RF5C68::Write(uint32_t Address, uint32_t Data)
{
	/*
		0x0009 - 0x0FFF is unused address space (according to the RF5C68 manual)
		Validation is needed to check if any region is mirrored
	*/

	/* 8KB Address space (13-bit) */
	Address &= 0x1FFF;
	Data &= 0xFF;

	if (Address & 0x1000) /* Waveform data (0x1000-0x1FFF) */
	{
		m_Memory[m_WaveBank | (Address & 0x0FFF)] = Data;
		return;
	}
	else /* Register data (0x0000-0x0FFF) */
	{
		auto& Channel = m_Channel[m_ChannelBank];

		switch (Address)
		{
		case 0x00: /* Envelope Register */
			Channel.ENV = Data;

			/* Pre-calculate */
			Channel.PREMUL_L = Channel.ENV * Channel.PAN_L;
			Channel.PREMUL_R = Channel.ENV * Channel.PAN_R;
			break;

		case 0x01: /* Pan Register */
			Channel.PAN_R = Data >> 4;
			Channel.PAN_L = Data & 0x0F;

			/* Pre-calculate */
			Channel.PREMUL_L = Channel.ENV * Channel.PAN_L;
			Channel.PREMUL_R = Channel.ENV * Channel.PAN_R;
			break;

		case 0x02: /* Frequency Delta (LSB) */
			Channel.FD = (Channel.FD & 0xFF00) | Data;
			break;

		case 0x03: /* Frequency Delta (MSB) */
			Channel.FD = (Data << 8) | (Channel.FD & 0x00FF);
			break;

		case 0x04: /* Loop Start (LSB) */
			Channel.LS = (Channel.LS & 0xFF00) | Data;
			break;

		case 0x05: /* Loop Start (MSB) */
			Channel.LS = (Data << 8) | (Channel.LS & 0x00FF);
			break;

		case 0x06: /* Start Address (MSB) */
			Channel.ST = Data << 8; /* LSB is always set to 0x00 */
			break;

		case 0x07: /* Control Register */
			m_Sounding = Data >> 7;

			if (Data & 0x40) /* MOD is 'H' */
			{
				m_ChannelBank = Data & 0x07;
			}
			else /* MOD is 'L' */
			{
				m_WaveBank = (Data & 0x0F) << 12;
			}
			break;

		case 0x08: /* Channel On/Off Register */
			m_ChannelCtrl = ~Data; /* 0 = ON, 1 = OFF */

			for (auto i = 0; i < 8; i++)
			{
				uint32_t ON = (m_ChannelCtrl >> i) & 0x01;

				if ((ON ^ m_Channel[i].ON) & ON)
				{
					/* Restarting channel, load start address */
					m_Channel[i].ADDR = m_Channel[i].ST << m_Shift;
				}

				m_Channel[i].ON = ON;
			}
			break;

		default: /* Illegal Access */
			/* The FM Towns startup music writes here */
			//__debugbreak();
			break;
		}
	}
}

void RF5C68::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int32_t OutL;
	int32_t OutR;
	uint8_t PCM;

	if (!m_Sounding || !m_ChannelCtrl) /* IC is not sounding / all channels are OFF */
	{
		/* Clear sample buffer (samples x 16-bit x 2 channels) */
		for (uint32_t i = 0; i < Samples; i++)
		{
			OutBuffer[0]->WriteSampleS16(0);
			OutBuffer[0]->WriteSampleS16(0);
		}

		return;
	}

	while (Samples != 0)
	{
		OutL = 0;
		OutR = 0;

		for (CHANNEL& Channel : m_Channel)
		{
			if (Channel.ON)
			{
				/* Read wave data from current address */
				PCM = m_Memory[Channel.ADDR >> m_Shift];

				if (PCM == 0xFF) /* Loop stop data */
				{
					/* Set to loop start address */
					Channel.ADDR = Channel.LS << m_Shift;

					/* Re-read wave data */
					PCM = m_Memory[Channel.ADDR >> m_Shift];

					if (PCM == 0xFF) continue; /* Looped to loop stop data, move to next channel */
				}

				/* Advance address counter (limit to 27-bits) */
				Channel.ADDR = (Channel.ADDR + Channel.FD) & 0x07FFFFFF;

				/* Apply panning + envelope and add/sub to output buffer

					7-bit PCM x 8-bit ENV x 4-bit PAN = 19-bit
					The most significant 14-bits are accumulated
				*/
				if (PCM & 0x80)
				{
					PCM &= 0x7F;
					OutL += (PCM * Channel.PREMUL_L) >> 5;
					OutR += (PCM * Channel.PREMUL_R) >> 5;
				}
				else
				{
					OutL -= (PCM * Channel.PREMUL_L) >> 5;
					OutR -= (PCM * Channel.PREMUL_R) >> 5;
				}
			}
		}

		/* Limiter (signed 16-bit) */
		OutL = std::clamp(OutL, -32768, 32767);
		OutR = std::clamp(OutR, -32768, 32767);

		/* 10-bit / 16-bit DAC output (interleaved) */
		OutBuffer[0]->WriteSampleS16(OutL & m_OutputMask);
		OutBuffer[0]->WriteSampleS16(OutR & m_OutputMask);

		Samples--;
	}
}

void RF5C68::CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void RF5C68::CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size + m_WaveBank) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset + m_WaveBank, Data, Size);
}