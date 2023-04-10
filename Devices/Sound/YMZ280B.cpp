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
#include "YMZ280B.h"
#include "ADPCM.h"
#include "YM.h"

/*
	Yamaha YMZ280B (PCMD8) 8-Channel PCM/ADPCM Decoder
	- 8 channels
	- 4-bit ADPCM, 8-bit PCM or 16-bit PCM sample format
	- 256-step level control
	- 16-level pan per channel

	This LSI comes in the following package:
	- 64-pin QFP (YMZ280B-F)

	A couple of notes:
	- Loop start address can not be changed when in 4-bit ADPCM mode and already looping (see manual page 10)
	- All other registers can be rewritten at any time (see manual page 10)

	Validation needed:
	- How is total level and pan actually handled ? Current implementation is probably wrong

	Things to do:
	- Implement a read interface
	- IRQ handling
	- Optional: a DSP interface ?

*/

YMZ280B::YMZ280B() :
	m_ClockSpeed(16934400), /* Use the internal clock if no external clock is connected */
	m_ClockDivider(192)
{
	/* Set memory size to 16MB */
	m_Memory.resize(0x01000000);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YMZ280B::GetDeviceName()
{
	return L"Yamaha YMZ280B";
}

void YMZ280B::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Clear utility registers */
	m_AddressLatch = 0;
	m_MemAddress = 0;
	m_KeyEnabled = 0;
	m_MemEnabled = 0;
	m_IrqEnabled = 0;
	m_LsiTest = 0;

	/* Clear channel registers  */
	memset(m_Channel, 0, sizeof(m_Channel));

	if (Type == ResetType::PowerOnDefaults)
	{
		/* Clear PCM memory */
		memset(m_Memory.data(), 0, m_Memory.size());
	}
}

void YMZ280B::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	/* No implementation needed */
}

bool YMZ280B::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
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

void YMZ280B::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YMZ280B::GetClockSpeed()
{
	return m_ClockSpeed;
}

void YMZ280B::Write(uint32_t Address, uint32_t Data)
{
	Data &= 0xFF; /* 8-bit data bus */
	
	if (Address & 1) /* Data write mode (A0 = H) */
	{
		WriteRegister(m_AddressLatch, Data);
	}
	else /* Address write mode (A0 = L) */
	{
		m_AddressLatch = Data;
	}
}

void YMZ280B::WriteRegister(uint32_t Address, uint32_t Data)
{
	if ((Address & 0x80) == 0) /* Function Registers (0x00 - 0x7F) */
	{
		auto& Channel = m_Channel[(Address >> 2) & 0x07];

		switch (Address & 0x63)
		{
		case 0x00: /* Playback Pitch (FN0 - FN7) */
			Channel.Pitch.u8l = Data;
			break;

		case 0x01: /* Key on, Mode, Loop, FN8 */
			Channel.Pitch.u8h = Data & 0x01;
			Channel.Mode = (Data >> 5) & 0x03;
			Channel.Loop = (Data >> 4) & 0x01;

			if (m_KeyEnabled) ProcessKeyOnOff(Channel, (Data >> 7) & 0x01);
			break;

		case 0x02: /* Total level */
			Channel.TotalLevel = Data;
			break;

		case 0x03: /* Panpot */
			Channel.PanAttnL = YM::PCMD8::PanAttnL[Data & 0x0F];
			Channel.PanAttnR = YM::PCMD8::PanAttnR[Data & 0x0F];
			break;

		case 0x20: /* Start address (H) */
			Channel.Start.u8hl = Data;
			break;

		case 0x21: /* Loop start address (H) */
			/* Don't allow loop start addres changes when in ADPCM loop mode */
			if (Channel.KeyOn && Channel.Loop && (Channel.Mode == 1)) break;
			
			Channel.LoopStart.u8hl = Data;
			break;

		case 0x22: /* Loop end address (H) */
			Channel.LoopEnd.u8hl = Data;
			break;

		case 0x23: /* End address (H) */
			Channel.End.u8hl = Data;
			break;

		case 0x40: /* Start address (M) */
			Channel.Start.u8lh = Data;
			break;

		case 0x41: /* Loop start address (M) */
			/* Don't allow loop start addres changes when in ADPCM loop mode */
			if (Channel.KeyOn && Channel.Loop && (Channel.Mode == 1)) break;

			Channel.LoopStart.u8lh = Data;
			break;

		case 0x42: /* Loop end address (M) */
			Channel.LoopEnd.u8lh = Data;
			break;

		case 0x43: /* End address (M) */
			Channel.End.u8lh = Data;
			break;

		case 0x60: /* Start address (L) */
			Channel.Start.u8ll = Data;
			break;

		case 0x61: /* Loop start address (L) */
			/* Don't allow loop start addres changes when in ADPCM loop mode */
			if (Channel.KeyOn && Channel.Loop && (Channel.Mode == 1)) break;

			Channel.LoopStart.u8ll = Data;
			break;

		case 0x62: /* Loop end address (L) */
			Channel.LoopEnd.u8ll = Data;
			break;

		case 0x63: /* End address (L) */
			Channel.End.u8ll = Data;
			break;
		}
	}
	else /* Utility Registers (0x80 - 0xFF) */
	{
		switch (Address)
		{
		case 0x80: /* DSP voice enable/selection */
			/* Not implemented */
			break;

		case 0x81: /* DSP enable */
			/* Not implemented */
			break;

		case 0x82: /* DSP data */
			/* Not implemented */
			break;

		case 0x84: /* RAM address (H) */
			m_MemAddress.u8hl = Data;
			break;

		case 0x85: /* RAM address (M) */
			m_MemAddress.u8lh = Data;
			break;

		case 0x86: /* RAM address (L) */
			m_MemAddress.u8ll = Data;
			break;

		case 0x87: /* RAM data */
			if (m_MemEnabled)
			{
				m_Memory[m_MemAddress.u32] = Data;

				/* Auto increment RAM address */
				m_MemAddress.u32 = (m_MemAddress.u32 + 1) & 0x00FFFFFF;
			}
			break;

		case 0xE0: /* IRQ enable/mask */
			/* Not implemented */
			break;

		case 0xFF: /* Global Key On / Off, Memory enable, IRQ enable, LSI Test */
			m_KeyEnabled = (Data >> 7) & 0x01;
			m_MemEnabled = (Data >> 6) & 0x01;
			m_IrqEnabled = (Data >> 4) & 0x01;
			m_LsiTest = Data & 0x03;

			/* Forcibly key off all channels */
			if (m_KeyEnabled == 0)
			{
				for (auto i = 0; i < 8; i++)
				{
					ProcessKeyOnOff(m_Channel[i], 0);
				}
			}
			break;

		default:
			//__debugbreak();
			break;

		}
	}
}

void YMZ280B::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int32_t OutL;
	int32_t OutR;
	int16_t Sample;

	while (Samples != 0)
	{
		OutL = 0;
		OutR = 0;

		for (CHANNEL& Channel : m_Channel)
		{
			if (Channel.KeyOn)
			{
				/* Update "pitch" generator */
				Channel.PitchCnt += (Channel.Pitch.u16 + 1);

				/* Check for "pitch" overflow */
				if (Channel.PitchCnt & 0xFE00)
				{
					/* Remove integer part, keep fractional part */
					Channel.PitchCnt &= 0x01FF;

					/* Save previous sample */
					Channel.SampleT0 = Channel.SampleT1;

					/* Load new sample, update address counter */
					switch (Channel.Mode)
					{
						case 0x00: /* Invalid format */
							/* Do we need to key off the channel ? */
							/* Should it be possible to get here ? */
							Channel.SampleT1 = 0;
							break;

						case 0x01: /* 4-bit ADPCM format */
							Channel.SampleT1 = UpdateSample4(Channel);
							break;

						case 0x02: /* 8-bit PCM format */
							Channel.SampleT1 = UpdateSample8(Channel);
							break;

						case 0x03: /* 16-bit PCM format */
							Channel.SampleT1 = UpdateSample16(Channel);
							break;
					}
				}

				/* Linear sample interpolation */
				Sample = (((0x200 - Channel.PitchCnt) * Channel.SampleT0) + (Channel.PitchCnt * Channel.SampleT1)) >> 9;

				/* NOTE: Below code handles level and pan control. Current implementation works, but is probably wrong */
				
				int32_t AttnL;
				int32_t AttnR;
				int16_t OutputL;
				int16_t OutputR;

				/* Apply pan */
				AttnL = Channel.TotalLevel - Channel.PanAttnL;
				AttnR = Channel.TotalLevel - Channel.PanAttnR;

				/* Limit */
				if (AttnL < 0) AttnL = 0;
				if (AttnR < 0) AttnR = 0;

				/* Multiply with interpolated sample */
				OutputL = (Sample * AttnL) / 255;
				OutputR = (Sample * AttnR) / 255;

				OutL += OutputL;
				OutR += OutputR;
			}
		}

		/* Limiter (signed 16-bit) */
		OutL = std::clamp(OutL, -32768, 32767);
		OutR = std::clamp(OutR, -32768, 32767);

		/* 16-bit DAC output (interleaved) */
		OutBuffer[0]->WriteSampleS16(OutL);
		OutBuffer[0]->WriteSampleS16(OutR);

		Samples--;
	}
}

void YMZ280B::ProcessKeyOnOff(CHANNEL& Channel, uint32_t NewState)
{
	/* Check for state changes */
	if (Channel.KeyOn != NewState)
	{
		if (NewState) /* Key On */
		{
			/* Reset address counter */
			Channel.Addr = Channel.Start.u32;

			/* Reset "pitch" counter */
			Channel.PitchCnt = 0;

			/* Reset sample interpolation */
			Channel.SampleT0 = 0;
			Channel.SampleT1 = 0;

			/* Reset ADPCM decoder */
			Channel.Signal = 0;
			Channel.Step = 127;
			Channel.LoopSignal = 0;
			Channel.LoopStep = 127;
			Channel.NibbleShift = 4;
		}

		Channel.KeyOn = NewState;
	}
}

int16_t YMZ280B::UpdateSample4(CHANNEL& Channel)
{
	if (Channel.Loop)
	{
		/* Check if we reached loop start address */
		if (Channel.Addr == Channel.LoopStart.u32)
		{
			if (Channel.NibbleShift) /* Make sure we're on the 1st nibble */
			{
				/* Save decoder state */
				Channel.LoopSignal = Channel.Signal;
				Channel.LoopStep = Channel.Step;
			}
		}

		/* Check if we reached loop end address */
		if (Channel.Addr >= Channel.LoopEnd.u32)
		{
			/* Reload loop start address */
			Channel.Addr = Channel.LoopStart.u32;

			/* Load decoder state */
			Channel.Signal = Channel.LoopSignal;
			Channel.Step = Channel.LoopStep;
		}
	}
	else
	{
		/* Check if we reached end address */
		if (Channel.Addr >= Channel.End.u32)
		{
			Channel.KeyOn = 0;
			return 0;
		}
	}
		
	/* Load nibble from memory */
	uint8_t Nibble = (m_Memory[Channel.Addr] >> Channel.NibbleShift) & 0x0F;

	/* Alternate between 1st and 2nd nibble */
	Channel.NibbleShift ^= 4;

	/* Increase address counter */
	Channel.Addr += (Channel.NibbleShift >> 2);

	/* Decode ADPCM nibble */
	YM::ADPCMZ::Decode(Nibble, &Channel.Step, &Channel.Signal);

	return Channel.Signal;
}

int16_t YMZ280B::UpdateSample8(CHANNEL& Channel)
{
	if (Channel.Loop)
	{
		/* Check if we reached loop end address */
		if (Channel.Addr >= Channel.LoopEnd.u32)
		{
			/* Reload loop start address */
			Channel.Addr = Channel.LoopStart.u32;
		}
	}
	else
	{
		/* Check if we reached end address */
		if (Channel.Addr >= Channel.End.u32)
		{
			Channel.KeyOn = 0;
			return 0;
		}
	}

	/* Load 8-bit sample from memory */
	int16_t Sample = m_Memory[Channel.Addr] << 8;

	/* Increase address counter */
	Channel.Addr += 1;

	return Sample;
}

int16_t YMZ280B::UpdateSample16(CHANNEL& Channel)
{
	if (Channel.Loop)
	{
		/* Check if we reached loop end address */
		if (Channel.Addr >= Channel.LoopEnd.u32)
		{
			/* Reload loop start address */
			Channel.Addr = Channel.LoopStart.u32;
		}
	}
	else
	{
		/* Check if we reached end address */
		if (Channel.Addr >= Channel.End.u32)
		{
			Channel.KeyOn = 0;
			return 0;
		}
	}

	/* Load 16-bit sample from memory */
	int16_t Sample = (m_Memory[Channel.Addr + 0] << 8) | m_Memory[Channel.Addr + 1];

	/* Increase address counter */
	Channel.Addr += 2;

	return Sample;
}

void YMZ280B::CopyToMemory(size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void YMZ280B::CopyToMemoryIndirect(size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(Offset, Data, Size);
}