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
#include "YMF278B.h"
#include "YM.h"

/*
	Yamaha YMF278-B (OPL4)

	PCM part:
	- 24 PCM channels
	- 8-bit, 12-bit and 16-bit linear PCM data
	- 16-stage pan control
	- Envelope control (including pseudo-reverb and damping)
	- PM and AM LFO (a.k.a vibrato and tremolo)
	- Interface up to 4MB of ROM + SRAM (22-bit address bus + 8-bit data bus)

	Things to validate :
	- LFO validation
	- TL interpolation
	- Attenuation (Envelope has a -96dB - 0dB range, TL and PAN a -48dB - 0 range but something is not adding up)

	Things to do:
	- Implement FM
	- Implement pseudo-reverb
	- Implement damping
	- Output channel selection
	- Implement a ADSR::Off state, to speed up code execution for non-playing channels
*/

/* Mix control attenuation table */
static const uint32_t MixAttnTable[8] =
{
	/* See OPL4 datasheet page 21 (section Mix Control)
	+-----+-----------------------------------------------+
	| Mix |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
	+-----+-----------------------------------------------+
	| dB  |  0  | -3  | -6  | -9  | -12 | -15 | -18 | inf |
	+-----+-----------------------------------------------+
	*/

	0, 0x20, 0x40, 0x60, 0x80, 0xA0, 0xC0, 0x3FF
};

YMF278B::YMF278B() :
	m_ClockSpeed(33868800),
	m_ClockDivider(768)
{
	YM::AWM::BuildTables();

	/* Set memory size to 4MB */
	m_Memory.resize(0x400000);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YMF278B::GetDeviceName()
{
	return L"Yamaha YMF278B";
}

void YMF278B::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	m_AddressLatch = 0;

	/* Reset utility registers */
	m_MemoryAddress.u32 = 0;
	m_MemoryAccess = 0;
	m_MemoryType = 0;
	m_WaveTableHeader = 0;
	
	/* Reset OPL expansion registers  */
	m_New = 0;
	m_New2 = 0;

	/* Reset mix control */
	m_MixCtrlFML = MixAttnTable[3]; /* -9dB */
	m_MixCtrlFMR = MixAttnTable[3]; /* -9dB */
	m_MixCtrlPCML = MixAttnTable[0]; /* 0dB */
	m_MixCtrlPCMR = MixAttnTable[0]; /* 0dB */

	/* Reset counters */
	m_EnvelopeCounter = 0;
	m_InterpolCounter = 0;

	/* Clear channel registers  */
	for (auto& Channel : m_Channel)
	{
		memset(&Channel, 0, sizeof(CHANNEL));

		/* Default envelope state */
		Channel.EgPhase = ADSR::Release;
		Channel.EgLevel = 0x3FF;

		/* Default LFO period */
		Channel.LfoPeriod = YM::AWM::LfoPeriod[0];
	}

	if (Type == ResetType::PowerOnDefaults)
	{
		/* Clear PCM memory */
		memset(m_Memory.data(), 0, m_Memory.size());
	}
}

void YMF278B::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
}

bool YMF278B::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	switch (OutputNr)
	{
	case 0:
		Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"FM (DO0)";
		return true;
		
	case 1:
		Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"PCM (DO1)";
		return true;

	case 2:
		Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"MIX (DO2)";
		return true;

	default:
		return false;
	}
}

void YMF278B::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YMF278B::GetClockSpeed()
{
	return m_ClockSpeed;
}

void YMF278B::Write(uint32_t Address, uint32_t Data)
{
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;

	switch (Address & 0x07) /* 3-bit address bus (A0 - A2) */
	{
	case 0x00: /* Address write mode */
	case 0x02:
	case 0x04:
	case 0x06:
		m_AddressLatch = Data;
		break;

	case 0x01: /* FM array 0 data write mode */
		WriteFM0(m_AddressLatch, Data);
		break;

	case 0x03: /* FM array 1 data write mode */
		WriteFM1(m_AddressLatch, Data);
		break;

	case 0x05: /* PCM data write mode */
		if (m_New2) WritePCM(m_AddressLatch, Data);
		break;

	case 0x07: /* Invalid address */
		break;
	}
}

void YMF278B::WriteFM0(uint8_t Register, uint8_t Data)
{

}

void YMF278B::WriteFM1(uint8_t Register, uint8_t Data)
{
	switch (Register)
	{
	case 0x00: /* LSI Test */
	case 0x01:
		break;

	case 0x02: /* Not used */
	case 0x03:
		break;

	case 0x04: /* 4-Operator mode setting */
		//TODO
		break;

	case 0x05: /* Expansion register */
		/* OPL3 mode enable flag */
		m_New = Data & 0x01;

		/* OPL4 mode enable flag */
		if (m_New) m_New2 = (Data >> 1) & 0x01;
		break;

	default:
		break;
	}
}

void YMF278B::WritePCM(uint8_t Register, uint8_t Data)
{
	auto& Channel = m_Channel[(Register - 8) % 24];

	switch (Register)
	{
	case 0x00: /* LSI Test */
	case 0x01:
		break;

	case 0x02: /* Wave table header / Memory type and access mode */
		m_MemoryAccess = (Data >> 0) & 0x01;
		m_MemoryType = (Data >> 1) & 0x01;
		m_WaveTableHeader = ((Data >> 2) & 0x07) << 19; /* 8 banks x 512KB */

		/* Note: Device ID is read only */
		break;

	case 0x03: /* Memory address (H) */
		m_MemoryAddress.u8hl = Data;
		break;

	case 0x04: /* Memory address (M) */
		m_MemoryAddress.u8lh = Data;
		break;

	case 0x05: /* Memory address (L) */
		m_MemoryAddress.u8ll = Data;
		break;

	case 0x06: /* Memory data */
		if (m_MemoryAccess)
		{
			m_Memory[m_MemoryAddress.u32] = Data;
			m_MemoryAddress.u32 = (m_MemoryAddress.u32 + 1) & 0x3FFFFF;
		}
		break;

	/* Wave table number [7:0] */
	case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0E: case 0x0F:
	case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E: case 0x1F:
		Channel.WaveNr.u8l = Data;

		LoadWaveTable(Channel);
		break;

	/* Frequency [6:0] / Wave table number [8] */
	case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
	case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2E: case 0x2F:
	case 0x30: case 0x31: case 0x32: case 0x33: case 0x34: case 0x35: case 0x36: case 0x37:
		Channel.FNum = (Channel.FNum & 0x380) | Data >> 1;
		Channel.WaveNr.u8h = Data & 0x1;
		break;

	/* Octave / Pseudo-Reverb / Frequency [9:7] */
	case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3E: case 0x3F:
	case 0x40: case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x46: case 0x47:
	case 0x48: case 0x49: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4E: case 0x4F:
		Channel.FNum = (Channel.FNum & 0x07F) | ((Data & 0x07) << 7);
		Channel.FNum9 = Channel.FNum >> 9;
		Channel.Octave = ((Data >> 4) ^ 8) - 8; /* Sign extend [range = +7 : -8] */
		//TODO: Channel.Reverb = (Data >> 3) & 0x01;
		break;

	/* Total level / Level direct */
	case 0x50: case 0x51: case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57:
	case 0x58: case 0x59: case 0x5A: case 0x5B: case 0x5C: case 0x5D: case 0x5E: case 0x5F:
	case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
		Channel.TargetTL = (Data & 0xFE) >> 1;

		if (Data & 0x01) /* Level direct */
		{
			Channel.TL = Channel.TargetTL;
		}
		break;

	/* Key on / Damp / LFO reset / Output selection / Panpot */
	case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E: case 0x6F:
	case 0x70: case 0x71: case 0x72: case 0x73: case 0x74: case 0x75: case 0x76: case 0x77:
	case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F:
		Channel.KeyPending = Data >> 7;
		Channel.PanAttnL = YM::AWM::PanAttnL[Data & 0x0F];
		Channel.PanAttnR = YM::AWM::PanAttnR[Data & 0x0F];
		Channel.LfoReset = (Data >> 5) & 0x01;		
		//TODO: Damp, output selection
		break;

	/* LFO / Vibrato (PM) */
	case 0x80: case 0x81: case 0x82: case 0x83: case 0x84: case 0x85: case 0x86: case 0x87:
	case 0x88: case 0x89: case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8E: case 0x8F:
	case 0x90: case 0x91: case 0x92: case 0x93: case 0x94: case 0x95: case 0x96: case 0x97:
		Channel.LfoPeriod = YM::AWM::LfoPeriod[(Data >> 3) & 0x07];
		Channel.PmDepth = Data & 0x07;
		break;

	/* Attack rate / Decay rate */
	case 0x98: case 0x99: case 0x9A: case 0x9B: case 0x9C: case 0x9D: case 0x9E: case 0x9F:
	case 0xA0: case 0xA1: case 0xA2: case 0xA3: case 0xA4: case 0xA5: case 0xA6: case 0xA7:
	case 0xA8: case 0xA9: case 0xAA: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
		Channel.Rate[ADSR::Attack] = Data >> 4;
		Channel.Rate[ADSR::Decay] = Data & 0x0F;
		break;

	/* Decay level / Sustain rate */
	case 0xB0: case 0xB1: case 0xB2: case 0xB3: case 0xB4: case 0xB5: case 0xB6: case 0xB7:
	case 0xB8: case 0xB9: case 0xBA: case 0xBB: case 0xBC: case 0xBD: case 0xBE: case 0xBF:
	case 0xC0: case 0xC1: case 0xC2: case 0xC3: case 0xC4: case 0xC5: case 0xC6: case 0xC7:
		Channel.Rate[ADSR::Sustain] = Data & 0x0F;

		/* If all DL bits are set, DL is -93dB. See OPL4 manual page 20 */
		Channel.DL = (Data & 0xF0) << 1;
		if (Channel.DL == 0x1E0) Channel.DL = 0x3E0;
		break;

	/* Release rate / Rate correction */
	case 0xC8: case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF:
	case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4: case 0xD5: case 0xD6: case 0xD7:
	case 0xD8: case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
		Channel.RC = Data >> 4;
		Channel.Rate[ADSR::Release] = Data & 0x0F;
		break;

	/* Tremolo (AM) */
	case 0xE0: case 0xE1: case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7:
	case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEE: case 0xEF:
	case 0xF0: case 0xF1: case 0xF2: case 0xF3: case 0xF4: case 0xF5: case 0xF6: case 0xF7:
		Channel.AmDepth = Data & 0x07;
		break;

	case 0xF8: /* Mix control (FM) */
		m_MixCtrlFML = MixAttnTable[(Data >> 0) & 0x07];
		m_MixCtrlFMR = MixAttnTable[(Data >> 3) & 0x07];
		break;

	case 0xF9: /* Mix control (PCM) */
		m_MixCtrlPCML = MixAttnTable[(Data >> 0) & 0x07];
		m_MixCtrlPCMR = MixAttnTable[(Data >> 3) & 0x07];
		break;

	default: /* Unused (0x07, 0xFA - 0xFF) */
		//__debugbreak();
		break;
	}
}

void YMF278B::LoadWaveTable(CHANNEL& Channel)
{
	uint32_t Offset;
	uint32_t WaveNr = Channel.WaveNr.u16;

	/* Read wave table header. Each header is 12-bytes */
	if (m_WaveTableHeader && (WaveNr >= 384))
	{
		WaveNr -= 384;
		Offset = (WaveNr * 12) + m_WaveTableHeader;
	}
	else
	{
		Offset = WaveNr * 12;
	}
	
	/* Byte 0: Wave format + start address [21:16] */
	Channel.Format = m_Memory[Offset] >> 6;
	Channel.Start.u8hl = m_Memory[Offset] & 0x3F;

	/* Byte 1: Start address [15:8] */
	Channel.Start.u8lh = m_Memory[Offset + 1];

	/* Byte 2: Start address [7:0] */
	Channel.Start.u8ll = m_Memory[Offset + 2];

	/* Byte 3: Loop address [15:8] */
	Channel.Loop.u8h = m_Memory[Offset + 3];

	/* Byte 4: Loop address [7:0] */
	Channel.Loop.u8l = m_Memory[Offset + 4];

	/* Byte 5 + 6: End address [15:0] */
	Channel.End = 0x10000 - ((m_Memory[Offset + 5] << 8) | m_Memory[Offset + 6]);

	/* Byte 7: LFO + VIB */
	Channel.LfoPeriod = YM::AWM::LfoPeriod[(m_Memory[Offset + 7] >> 3) & 0x07];
	Channel.PmDepth = m_Memory[Offset + 7] & 0x07;

	/* Byte 8: Attack rate + decay rate  */
	Channel.Rate[ADSR::Attack] = m_Memory[Offset + 8] >> 4;
	Channel.Rate[ADSR::Decay] = m_Memory[Offset + 8] & 0x0F;

	/* Byte 9: Decay level + sustain rate */
	Channel.Rate[ADSR::Sustain] = m_Memory[Offset + 9] & 0x0F;

	/* If all DL bits are set, DL is -93dB. See OPL4 manual page 20 */
	Channel.DL = (m_Memory[Offset + 9] & 0xF0) << 1;
	if (Channel.DL == 0x1E0) Channel.DL = 0x3E0;

	/* Byte 10: Rate correction + release rate */
	Channel.RC = m_Memory[Offset + 10] >> 4;
	Channel.Rate[ADSR::Release] = m_Memory[Offset + 10] & 0x0F;

	/* Byte 11: AM */
	Channel.AmDepth = m_Memory[Offset + 11] & 0x07;
}

void YMF278B::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int32_t OutL;
	int32_t OutR;

	while (Samples != 0)
	{
		OutL = 0;
		OutR = 0;

		for (auto& Channel : m_Channel)
		{
			UpdateLFO(Channel);
			UpdateAddressGenerator(Channel);
			UpdateInterpolator(Channel);
			UpdateEnvelopeGenerator(Channel);
			UpdateMultiplier(Channel);

			OutL += Channel.OutputL;
			OutR += Channel.OutputR;
		}

		/* Global counter increments */
		m_EnvelopeCounter++;
		m_InterpolCounter++;

		/* Limiter (signed 16-bit) */
		OutL = std::clamp(OutL, -32768, 32767);
		OutR = std::clamp(OutR, -32768, 32767);

		/* 16-bit DAC output (interleaved) */
		OutBuffer[0]->WriteSampleS16(OutL);
		OutBuffer[0]->WriteSampleS16(OutR);

		Samples--;
	}
}

int16_t YMF278B::ReadSample(CHANNEL& Channel)
{
	uint32_t Offset;
	int16_t Sample = 0;

	switch (Channel.Format)
	{
	case 0: /* 8-bit PCM */
		Offset = Channel.Start.u32 + Channel.SampleCount;
		Sample = m_Memory[Offset] << 8;
		break;

	case 1: /* 12-bit PCM */
		Offset = Channel.Start.u32 + ((Channel.SampleCount * 3) / 2);

		if (Channel.SampleCount & 0x01)
		{
			Sample = (m_Memory[Offset + 2] << 8) | ((m_Memory[Offset + 1] & 0x0F) << 4);
		}
		else
		{
			Sample = (m_Memory[Offset + 0] << 8) | (m_Memory[Offset + 1] & 0xF0);
		}
		break;

	case 2: /* 16-bit PCM */
		Offset = Channel.Start.u32 + (Channel.SampleCount * 2);

		Sample = (m_Memory[Offset + 0] << 8) | m_Memory[Offset + 1];
		break;

	case 3: /* Invalid format */
		break;
	}

	return Sample;
}

void YMF278B::UpdateLFO(CHANNEL& Channel)
{
	if (!Channel.LfoReset) /* LFO active */
	{
		if (++Channel.LfoCounter >= Channel.LfoPeriod)
		{
			/* Reset counter */
			Channel.LfoCounter = 0;

			/* Increase step counter (8-bit) */
			Channel.LfoStep++;
		}
	}
	else /* LFO deactive */
	{
		Channel.LfoCounter = 0;
		Channel.LfoStep = 0;
	}
}

void YMF278B::UpdateAddressGenerator(CHANNEL& Channel)
{
	/* Vibrato lookup */
	int32_t Vibrato = YM::AWM::VibratoTable[Channel.LfoStep >> 2][Channel.PmDepth]; /* 64 steps */

	/* Calculate address increment */
	uint32_t Inc = ((1024 + Channel.FNum + Vibrato) << (8 + Channel.Octave)) >> 3;

	/* Update address counter (16.16) */
	Channel.SampleDelta += Inc;

	/* Check for delta overflow */
	if (Channel.SampleDelta >> 16)
	{
		/* Update sample counter */
		Channel.SampleCount += (Channel.SampleDelta >> 16);
		Channel.SampleDelta &= 0xFFFF;

		/* Check for end address */
		if (Channel.SampleCount >= Channel.End)
		{
			/* Loop */
			Channel.SampleCount = Channel.Loop.u16;
		}

		/* Load new sample */
		Channel.SampleT0 = Channel.SampleT1;
		Channel.SampleT1 = ReadSample(Channel);
	}
}

void YMF278B::UpdateEnvelopeGenerator(CHANNEL& Channel)
{
	/* Process pending Key On/ Off events */
	if (Channel.KeyOn != Channel.KeyPending) ProcessKeyOnOff(Channel);

	int32_t Level = Channel.EgLevel;

	/* Get adjusted / key scaled rate */
	uint32_t Rate = CalculateRate(Channel, Channel.Rate[Channel.EgPhase]);

	/* Get EG counter resolution */
	uint32_t Shift = YM::OPL::EgShift[Rate];
	uint32_t Mask = (1 << Shift) - 1;

	if ((m_EnvelopeCounter & Mask) == 0) /* Counter overflowed */
	{
		/* Get update cycle (8 cycles in total) */
		uint32_t Cycle = (m_EnvelopeCounter >> Shift) & 0x07;

		/* Lookup attenuation adjustment */
		uint32_t AttnInc = YM::OPL::EgLevelAdjust[Rate][Cycle];

		if (Channel.EgPhase == ADSR::Attack) /* Exponential decrease (0x3FF -> 0) */
		{
			/* A rate of 63 has a special meaning: it forces the ADSR phase to instantly move from
			   attack to decay. This is only evaluated during key-on events. This implies that the attack phase
			   can be stalled if the rate is changed to 63 while already (slowly) attacking */

			if (Rate < 63)
			{
				Level += (~Level * AttnInc) >> 4;

				if (Level <= 0)
				{
					/* We've reached minimum level, move to decay phase
					   or move to sustain phase when decay level is 0 */

					Level = 0;
					Channel.EgPhase = Channel.DL ? ADSR::Decay : ADSR::Sustain;
				}
			}
		}
		else /* Linear increase (0 -> 0x3FF) */
		{
			Level += AttnInc;

			/* Limit to maximum attenuation */
			if (Level > 0x3FF) Level = 0x3FF;

			if (Channel.EgPhase == ADSR::Decay)
			{
				/* We reached the decay level, move to the sustain phase */
				if ((uint32_t)Level >= Channel.DL)
				{
					Level = Channel.DL;
					Channel.EgPhase = ADSR::Sustain;
				}
			}
		}

		Channel.EgLevel = Level;
	}
}

void YMF278B::UpdateMultiplier(CHANNEL& Channel)
{
	/* Level interpolation

	( (78.2 * 44100) / 1000) / 128 (TL steps) = ~27 samples
	((156.4 * 44100) / 1000) / 128 (TL steps) = ~54 samples

	This seems wrong IMHO but those are the numbers mentioned in the OPL4 manual.

	For now I use a global counter which counts up once every sample
	(might as well use the envelope counter as it counts up on the same clock) */
	if (Channel.TargetTL != Channel.TL)
	{
		if (Channel.TL < Channel.TargetTL) /* Maximum to minimum volume (156.4 msec) */
		{
			if ((m_InterpolCounter % 54) == 0) Channel.TL += 1;
		}
		else /* Minimum to maximum volume (78.2 msec) */
		{
			if ((m_InterpolCounter % 27) == 0) Channel.TL -= 1;
		}
	}

	/* Get envelope generator output (10-bit = 4.6 fixed point) */
	uint32_t Attenuation = Channel.EgLevel;

	int16_t Sample = Channel.Sample;

	/* Apply AM LFO (tremolo) */
	Attenuation += YM::AWM::TremoloTable[Channel.LfoStep][Channel.AmDepth];

	/* Apply total level */
	Attenuation += Channel.TL << 2;

	/* Apply pan */
	uint32_t AttnL = Attenuation + Channel.PanAttnL;
	uint32_t AttnR = Attenuation + Channel.PanAttnR;

	/* Limit */
	if (AttnL > 0x3FF) AttnL = 0x3FF;
	if (AttnR > 0x3FF) AttnR = 0x3FF;

	/* Convert from 4.6 to 4.8 fixed point */
	AttnL <<= 2;
	AttnR <<= 2;

	/* dB to linear conversion (13-bit) */
	uint32_t VolumeL = YM::ExpTable[AttnL & 0xFF] >> (AttnL >> 8);
	uint32_t VolumeR = YM::ExpTable[AttnR & 0xFF] >> (AttnR >> 8);

	/* Multiply with interpolated sample */
	Channel.OutputL = (Sample * VolumeL) >> 15;
	Channel.OutputR = (Sample * VolumeR) >> 15;
}

void YMF278B::UpdateInterpolator(CHANNEL& Channel)
{
	uint32_t T0 = 0x10000 - Channel.SampleDelta;
	uint32_t T1 = Channel.SampleDelta;

	/* Linear sample interpolation */
	Channel.Sample = ((T0 * Channel.SampleT0) + (T1 * Channel.SampleT1)) >> 16;
}

void YMF278B::ProcessKeyOnOff(CHANNEL& Channel)
{
	if (Channel.KeyPending) /* Key On */
	{
		/* Reset sample counter */
		Channel.SampleCount = 0;
		Channel.SampleDelta = 0;

		/* Move envelope to attack phase */
		Channel.EgPhase = ADSR::Attack;

		/* Instant attack */
		if (CalculateRate(Channel, Channel.Rate[ADSR::Attack]) == 63)
		{
			/* Instant minimum attenuation */
			Channel.EgLevel = 0;

			/* Move to decay or sustain phase */
			Channel.EgPhase = Channel.DL ? ADSR::Decay : ADSR::Sustain;
		}

		/* Reset sample interpolation */
		Channel.SampleT0 = 0;
		Channel.SampleT1 = 0;
	}
	else /* Key Off */
	{
		/* Move envelope to release phase */
		Channel.EgPhase = ADSR::Release;
	}

	Channel.KeyOn = Channel.KeyPending;
}

uint8_t YMF278B::CalculateRate(CHANNEL& Channel, uint8_t Rate)
{
	/* How to calculate the actual rate (OPL4 manual page 23):

	   RATE = (OCT + rate correction) x 2 + F9 + RD

	   OCT	= Octave (-7 to +7)
	   F9	= Fnum bit 9 (0 or 1)
	   RD	= Rate x 4
	*/

	if (Rate == 0) return 0;
	if (Rate == 15) return 63;

	Rate <<= 2; /* RD = Rate x 4 */

	if (Channel.RC != 0xF)
	{
		Rate += (2 * std::clamp<int32_t>(Channel.Octave + Channel.RC, 0, 15)) + Channel.FNum9;

		/* Limit to a max. rate of 63 */
		if (Rate > 63) Rate = 63;
	}

	return Rate;
}

void YMF278B::CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void YMF278B::CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(MemoryID, Offset, Data, Size);
}