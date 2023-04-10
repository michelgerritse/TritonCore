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
#include "YMW258F.h"
#include "YM.h"

/*	
	Yamaha YMW258-F (GEW8)

	This chip is also known as the Sega MultiPCM (315-5560)

	- 28 PCM channels
	- 8-bit and 12-bit linear PCM data (16-bit as well ?)
	- 16-stage pan control
	- Envelope control
	- PM and AM LFO (a.k.a vibrato and tremolo)
	- Interface up to 4MB of ROM + SRAM (22-bit address bus + 8-bit data bus)

	Things validate:
	- LFO validation
	- TL interpolation
	- Attenuation (Envelope has a -96dB - 0dB range, TL and PAN a -48dB - 0 range)

	Things to do:
	- Implement a ADSR::Off state, to speed up code execution for non-playing channels

	Daytona USA: Arcade Soundtrack (Vinyl Rip - Side A)
	https://www.youtube.com/watch?v=aSCtTxcBOk8

	Daytona USA: Arcade Soundtrack (Vinyl Rip - Side B)
	https://www.youtube.com/watch?v=o_2zA522WFE

	Sega banking information provided by Valley Bell:
	https://github.com/ValleyBell/libvgm
*/

static uint32_t PowTable[256];
static uint32_t TremoloTable[256][8];
static  int32_t VibratoTable[64][8];

void YMW258F::BuildTables()
{
	static bool Initialized = false;

	if (!Initialized)
	{
		/* Linear to dB table */
		for (auto i = 0; i < 256; i++)
		{
			PowTable[i] = (YM::ExpTable[i ^ 255] | 0x400) << 2;
		}

		/* Tremolo table (AM) */
		for (auto i = 0; i < 256; i++)
		{
			uint32_t Step = i; /* 256 steps */

			/* Create triangular shaped wave (0x00 .. 0x7F, 0x7F .. 0x00) */
			if (Step & 0x80) Step ^= 0xFF;

			//TODO: is this an inverted triangle (like OPN) ?
			//		eg. starting at maximum amplitude
			//		Als need to confirm on the step increment (which is 2 for OPN)

			for (auto j = 0; j < 8; j++)
			{
				TremoloTable[i][j] = (Step * YM::AWM::LfoAmDepth[j]) >> 7;
			}
		}

		/* Vibrato table (PM) */
		for (auto i = 0; i < 64; i++)
		{
			uint32_t Step = i; /* 64 steps (32 pos, 32 neg) */

			/* Create triangular shaped wave (0x0 .. 0xF, 0xF .. 0x0) */
			if (Step & 0x10) Step ^= 0x1F;

			for (auto j = 0; j < 8; j++)
			{
				if (Step & 0x20) /* Negative phase */
				{
					VibratoTable[i][j] = 0 - (((Step & 0x0F) * YM::AWM::LfoPmDepth[j]) >> 4);
				}
				else /* Positive phase */
				{
					VibratoTable[i][j] = ((Step & 0x0F) * YM::AWM::LfoPmDepth[j]) >> 4;
				}
			}
		}

		Initialized = true;
	}
}

YMW258F::YMW258F() :
	m_ClockDivider(224)
{
	BuildTables();

	/* Set memory size to 4MB */
	m_Memory.resize(0x400000);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YMW258F::GetDeviceName()
{
	return L"Yamaha YMW258F";
}

void YMW258F::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Reset latches */
	m_AddressLatch = 0;
	m_RegisterLatch = 0;

	/* Reset counters */
	m_EnvelopeCounter = 0;
	m_InterpolCounter = 0;

	/* Reset banking (Sega MultiPCM only) */
	m_Banking = 0;
	m_Bank0 = 0;
	m_Bank1 = 0;

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

void YMW258F::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	/* Sega MultiPCM bank switching */
	switch (Command)
	{
	case 0x10: /* 1 MB banking (Sega Model 1 + 2) */
		m_Banking = 1;
		m_Bank0 = ((Value << 20) | 0x000000) & 0x3FFFFF;
		m_Bank1 = ((Value << 20) | 0x080000) & 0x3FFFFF;
		break;

	case 0x11: /* 512 KB banking - low bank (Sega Multi 32) */
		m_Banking = 1;
		m_Bank0 = (Value << 19) & 0x3FFFFF;
		break;

	case 0x12:	/* 512 KB banking - high bank (Sega Multi 32) */
		m_Banking = 1;
		m_Bank1 = (Value << 19) & 0x3FFFFF;
		break;
	}
}

bool YMW258F::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
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

void YMW258F::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YMW258F::GetClockSpeed()
{
	return m_ClockSpeed;
}

void YMW258F::Write(uint32_t Address, uint32_t Data)
{
	/* 4-bit address bus (A0 - A3) */
	Address &= 0x0F;
	
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;
	
	switch (Address)
	{
	case 0x00: /* PCM channel data write */
		WriteChannel(m_AddressLatch, m_RegisterLatch, Data);
		break;

	case 0x01: /* PCM channel latch */
		/* Addresses are mirrored (Virtua Racing uses this) */
		m_AddressLatch = Data & 0x1F;
		break;

	case 0x02: /* PCM register latch */
		/* Virtua Racing writes to 0x09 and 0x0A, which we will ignore for now */
		m_RegisterLatch = Data;
		break;

	case 0x03: /* Unknown */
		/* Virtua Racing writes 0x00 */
		break;

	case 0x09: /* Unknown */
		/* Stadium Cross and Title Fight write 0x03 */
		break;

	default:
		/* 
		Some datasheets (eg. Yamaha PSR-510) indicate the YMW258-F is used to control an external DSP
		There are also read/write lines (MRD and MWR) which would indicate
		sample reading / writing from an external CPU.
		
		All this would require the extra address lines that are available,
		since out of the 16 addresses only 3 are used
		(There are rumors this chip could do FM as well)
		*/
		__debugbreak();
		break;
	}
}

void YMW258F::WriteChannel(uint32_t Address, uint32_t Register, uint32_t Data)
{
	/*	Address to channel mapping:
		0x00: 00	0x08: 07	0x10: 14	0x18: 21
		0x01: 01	0x09: 08	0x11: 15	0x19: 22
		0x02: 02	0x0A: 09	0x12: 16	0x1A: 23
		0x03: 03	0x0B: 10	0x13: 17	0x1B: 24
		0x04: 04	0x0C: 11	0x14: 18	0x1C: 25
		0x05: 05	0x0D: 12	0x15: 19	0x1D: 26
		0x06: 06	0x0E: 13	0x16: 20	0x1E: 27
		0x07: --	0x0F: --	0x17: --	0x1F: --
	*/

	/* Invalid channel selected */
	if ((Address & 0x07) == 0x07) return;

	/* Invalid register selected ? */
	if (Register > 7)
	{
		//__debugbreak();
		return;
	}

	/* Map address to channel */
	auto& Channel = m_Channel[Address - (Address >> 3)];

	switch (Register)
	{
	case 0x00: /* Pan */
		Channel.PanAttnL = YM::AWM::PanAttnL[Data >> 4];
		Channel.PanAttnR = YM::AWM::PanAttnR[Data >> 4];

		assert((Data & 0x0F) == 0); /* Test for undocumented bits */
		break;

	case 0x01: /* Wave table number [7:0] */
		Channel.WaveNr.u8l = Data;

		/* Load wave header */
		ReadWaveHeader(Channel);

		break;

	case 0x02: /* Frequency [5:0] / Wave table number [8] */
		Channel.FNum = (Channel.FNum & 0x3C0) | Data >> 2;
		Channel.WaveNr.u8h = Data & 0x1;

		assert((Data & 0x02) == 0); /* Test for undocumented bits */
		break;

	case 0x03: /* Octave / Frequency [9:6] */
		Channel.FNum = (Channel.FNum & 0x03F) | ((Data & 0x0F) << 6);
		Channel.FNum9 = Channel.FNum >> 9;
		Channel.Octave = ((Data >> 4) ^ 8) - 8; /* Sign extend [range = +7 : -8] */
		
		assert(Channel.Octave != -8);
		break;

	case 0x04: /* Channel control */
		Channel.KeyPending = Data >> 7;

		assert((Data & 0x7F) == 0); /* Test for undocumented bits */
		break;

	case 0x05: /* Total level / Level direct */
		/* The OPL4 manual page 18 states the weighting for each TL bit as:
		   24dB - 12 dB - 6dB - 3dB - 1.5dB - 0.75dB - 0.375dB
		*/

		Channel.TargetTL = (Data & 0xFE) >> 1;

		if (Data & 0x01) /* Level direct */
		{
			Channel.TL = Channel.TargetTL;
		}
		break;

	case 0x06: /* LFO Frequency / Vibrato (PM) */
		Channel.LfoPeriod = YM::AWM::LfoPeriod[(Data >> 3) & 0x07];
		Channel.PmDepth = Data & 0x07;

		assert((Data & 0xC0) == 0); /* Test for undocumented bits */
		break;

	case 0x07: /* Tremolo (AM) */
		Channel.AmDepth = Data & 0x07;

		assert((Data & 0xF8) == 0); /* Test for undocumented bits */
		break;
	}
}

void YMW258F::ReadWaveHeader(CHANNEL& Channel)
{
	/* Read wave table header. Each header is 12-bytes */
	uint32_t Offset = Channel.WaveNr.u16 * 12;

	/* Byte 0: Wave format + start address [21:16] */
	Channel.Format = m_Memory[Offset] >> 6;
	Channel.Start.u8hl = m_Memory[Offset] & 0x3F;

	assert((m_Memory[Offset] & 0xC0) == 0); /* Test for undocumented bits */

	/* Byte 1: Start address [15:8] */
	Channel.Start.u8lh = m_Memory[Offset + 1];

	/* Byte 2: Start address [7:0] */
	Channel.Start.u8ll = m_Memory[Offset + 2];

	/* Byte 3: Loop address [15:8] */
	Channel.Loop.u8h = m_Memory[Offset + 3];

	/* Byte 4: Loop address [7:0] */
	Channel.Loop.u8l = m_Memory[Offset + 4];

	/* Byte 5: End address [15:8] */
	Channel.End.u8h = m_Memory[Offset + 5];

	/* Byte 6: End address [7:0] */
	Channel.End.u8l = m_Memory[Offset + 6];

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

	/* Appply banking (Sega MultiPCM only) */
	if (m_Banking)
	{
		if (Channel.Start.u32 & 0x100000)
		{
			if (Channel.Start.u32 & 0x080000)
			{
				Channel.Start.u32 = (Channel.Start.u32 & 0x7FFFF) | m_Bank1;
			}
			else
			{
				Channel.Start.u32 = (Channel.Start.u32 & 0x7FFFF) | m_Bank0;
			}
		}
	}
}

void YMW258F::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
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
			UpdatePhaseGenerator(Channel);
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

int16_t YMW258F::ReadSample(CHANNEL& Channel)
{
	uint32_t Offset;
	int16_t Sample = 0;

	switch (Channel.Format)
	{
	case 0: /* 8-bit PCM */
		Offset = Channel.Start.u32 + Channel.Addr.u16h;
		Sample = m_Memory[Offset] << 8;
		break;

	case 1: /* 12-bit PCM */
		Offset = Channel.Start.u32 + ((Channel.Addr.u16h * 2) / 3);

		if (Channel.Addr.u16h & 0x01)
		{
			Sample = (m_Memory[Offset + 2] << 8) | (m_Memory[Offset + 1] & 0xF0);
		}
		else
		{
			Sample = (m_Memory[Offset + 0] << 8) | (m_Memory[Offset + 1] << 4);
		}
		break;

	case 2: /* 16-bit PCM */
		Offset = Channel.Start.u32 + (Channel.Addr.u16h * 2);

		Sample = (m_Memory[Offset + 0] << 8) | m_Memory[Offset + 1];
		break;

	case 3: /* Invalid format */
		break;
	}

	return Sample;
}

void YMW258F::UpdateLFO(CHANNEL& Channel)
{
	if (++Channel.LfoCounter >= Channel.LfoPeriod)
	{
		/* Reset counter */
		Channel.LfoCounter = 0;

		/* Increase step counter (8-bit) */
		Channel.LfoStep++;
	}
}

void YMW258F::UpdatePhaseGenerator(CHANNEL& Channel)
{
	/* Vibrato lookup */
	int32_t Vibrato = VibratoTable[Channel.LfoStep >> 2][Channel.PmDepth]; /* 64 steps */

	/* Calculate phase increment */
	uint32_t Inc = ((1024 + Channel.FNum + Vibrato) << (8 + Channel.Octave)) >> 3;

	/* Update address counter (16.16) */
	Channel.Addr.u32 += Inc;

	/* Check for end address (ignore fractional part) */
	if ((Channel.Addr.u16h + Channel.End.u16) > 0xFFFF)
	{
		/* Loop */
		Channel.Addr.u16h += (Channel.End.u16 + Channel.Loop.u16);
	}
}

void YMW258F::UpdateEnvelopeGenerator(CHANNEL& Channel)
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
				Level += (~Level << AttnInc) >> 5;

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
				if ((uint32_t) Level >= Channel.DL) Channel.EgPhase = ADSR::Sustain;
			}
		}

		Channel.EgLevel = Level;
	}
}

void YMW258F::UpdateMultiplier(CHANNEL& Channel)
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
	Attenuation += TremoloTable[Channel.LfoStep][Channel.AmDepth];

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

	/* linear to dB conversion */
	uint32_t VolumeL = PowTable[AttnL & 0xFF] >> (AttnL >> 8);
	uint32_t VolumeR = PowTable[AttnR & 0xFF] >> (AttnR >> 8);

	/* Multiply with interpolated sample */
	Channel.OutputL = (Sample * VolumeL) >> 15;
	Channel.OutputR = (Sample * VolumeR) >> 15;
}

void YMW258F::UpdateInterpolator(CHANNEL& Channel)
{
	uint32_t T0 = 0x10000 - Channel.Addr.u16l;
	uint32_t T1 = Channel.Addr.u16l;

	if (Channel.OldAddr ^ Channel.Addr.u16h) /* We moved to a new sample address */
	{
		Channel.SampleT0 = Channel.SampleT1;
		Channel.SampleT1 = ReadSample(Channel);
		Channel.OldAddr = Channel.Addr.u16h;
	}

	//FIXME: Virtua Cop - 07 Underground Weapon Storage + 14 Game Over are having interpolation issues

	/* Linear sample interpolation */
	Channel.Sample = ((T0 * Channel.SampleT0) + (T1 * Channel.SampleT1)) >> 16;
}

void YMW258F::ProcessKeyOnOff(CHANNEL& Channel)
{
	if (Channel.KeyPending) /* Key On */
	{
		/* Reset address counter */
		Channel.Addr.u32 = 0;

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
		Channel.OldAddr = 0xFFFF;
	}
	else /* Key Off */
	{
		/* Move envelope to release phase */
		Channel.EgPhase = ADSR::Release;
	}

	Channel.KeyOn = Channel.KeyPending;
}

uint8_t YMW258F::CalculateRate(CHANNEL& Channel, uint8_t Rate)
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

void YMW258F::CopyToMemory(size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void YMW258F::CopyToMemoryIndirect(size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(Offset, Data, Size);
}