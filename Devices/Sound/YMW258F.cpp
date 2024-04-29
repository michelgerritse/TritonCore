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
#include "YMW258F.h"

/*
	Yamaha YMW258-F (GEW8)

	This chip is also known as the Sega MultiPCM (315-5560)
	- 28 PCM channels
	- 8-bit and 12-bit linear PCM data
	- 16-stage pan control
	- Envelope control
	- PM and AM LFO (a.k.a vibrato and tremolo)
	- Interface up to 4MB of ROM or SRAM (22-bit address bus + 8-bit data bus)
	- 18-bit accumulator
	
	Things to validate:
	- Channel output resolution (assuming 16-bit)
	- LFO (vibrato and tremolo)
	- TL interpolation

	Sega banking information provided by Valley Bell:
	https://github.com/ValleyBell/libvgm
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
const std::wstring YMW258F::s_DeviceName = L"Yamaha YMW258-F";

YMW258F::YMW258F(uint32_t ClockSpeed, bool HasLDSP, size_t MemorySizeLDSP) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(224),
	m_LDSP(HasLDSP ? std::make_unique<YM3413>(MemorySizeLDSP) : nullptr)
{
	YM::GEW8::BuildTables();

	/* Set memory size to 4MB */
	m_Memory.resize(0x400000);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YMW258F::GetDeviceName()
{
	return s_DeviceName.c_str();
}

void YMW258F::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	m_Timer = 0;
	m_ChannelLatch = 0;
	m_RegisterLatch = 0;
	m_MemoryAddress = 0;

	/* Reset DSP command */
	m_DspCommand.u32 = 0;
	m_DspCommandCnt = 0;

	/* Reset banking (Sega MultiPCM only) */
	m_Banking = 0;
	m_Bank0 = 0;
	m_Bank1 = 0;

	/* Clear channel registers  */
	for (auto& Channel : m_Channel)
	{
		memset(&Channel, 0, sizeof(YM::GEW8::channel_t));

		/* Default envelope state */
		Channel.EgPhase = ADSR::Release;
		Channel.EgLevel = YM::GEW8::MaxAttenuation;

		/* Default LFO period */
		Channel.LfoPeriod = YM::GEW8::LfoPeriod[0];
	}

	if (Type == ResetType::PowerOnDefaults)
	{
		/* Clear PCM memory */
		memset(m_Memory.data(), 0, m_Memory.size());
	}

	/* Reset LDSP */
	if (m_LDSP != nullptr) m_LDSP->InitialClear();
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
	if (OutputNr == AudioOut::Default)
	{
		Desc.SampleRate		= m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat	= AudioFormat::AUDIO_FMT_S16;
		Desc.Channels		= 2;
		Desc.ChannelMask	= SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description	= L"Digital Out";

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
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;
	
	switch (Address & 0x0F) /* 4-bit address bus (A0 - A3) */
	{
	case 0x00: /* PCM data write */
		WritePcmData(m_ChannelLatch, m_RegisterLatch, Data);
		break;

	case 0x01: /* PCM channel latch */
		m_ChannelLatch = Data;
		break;

	case 0x02: /* PCM register latch */
		m_RegisterLatch = Data;
		break;

	case 0x03: /* Memory address (H) (Guess) */
		/* Virtua Racing writes 0x00 */
		m_MemoryAddress.u8hl = Data & 0x3F;
		break;

	case 0x04: /* Memory address (M) (Guess) */
		m_MemoryAddress.u8lh = Data;
		break;

	case 0x05: /* Memory address (L) (Guess) */
		m_MemoryAddress.u8ll = Data;
		break;

	case 0x06: /* Memory data (Guess) */
		m_Memory[m_MemoryAddress.u32] = Data;

		/* Address auto increment */
		m_MemoryAddress.u32 = (m_MemoryAddress.u32 + 1) & 0x3FFFFF;
		break;

	case 0x07: /* Unknown */
		__debugbreak();
		break;

	case 0x08: /* Unknown */
		__debugbreak();
		break;

	case 0x09: /* Global / Master Level (Guess)  */
		/* Stadium Cross and Title Fight write 0x03 */
		/* TG-100 startup code writes 0x03 */
		break;

	case 0x0A: /* Unknown */
		__debugbreak();
		break;

	case 0x0B: /* Unknown */
		__debugbreak();
		break;

	case 0x0C: /* Unknown */
		__debugbreak();
		break;

	case 0x0D: /* LDSP command data */
		switch (m_DspCommandCnt)
		{
		case 0:
			m_DspCommand.u8ll = Data;
			break;

		case 1:
			m_DspCommand.u8lh = Data;
			break;

		case 2:
			m_DspCommand.u8hl = Data;
			break;

		case 3:
			m_DspCommand.u8hh = Data;

			if (m_LDSP != nullptr) m_LDSP->SendCommandData(m_DspCommand.u32);
			break;
		}

		m_DspCommandCnt = (m_DspCommandCnt + 1) & 3;
		break;

	case 0x0E: /* LDSP reset (Guess) */
		if (m_LDSP != nullptr) m_LDSP->InitialClear();
		break;

	case 0x0F: /* Unknown */
		__debugbreak();
		break;
	}
}

void YMW258F::WritePcmData(uint8_t ChannelNr, uint8_t Register, uint8_t Data)
{
	/*	Channel mapping:
		0x00: 00	0x08: 07	0x10: 14	0x18: 21
		0x01: 01	0x09: 08	0x11: 15	0x19: 22
		0x02: 02	0x0A: 09	0x12: 16	0x1A: 23
		0x03: 03	0x0B: 10	0x13: 17	0x1B: 24
		0x04: 04	0x0C: 11	0x14: 18	0x1C: 25
		0x05: 05	0x0D: 12	0x15: 19	0x1D: 26
		0x06: 06	0x0E: 13	0x16: 20	0x1E: 27
		0x07: --	0x0F: --	0x17: --	0x1F: --
	*/

	/* Channel validation */
	if ((ChannelNr & 0x07) == 0x07) return;

	/* Channel mirroring (Virtua Racing uses this) */
	ChannelNr &= 0x1F;

	auto& Channel = m_Channel[ChannelNr - (ChannelNr >> 3)];

	switch (Register)
	{
	case 0x00: /* Pan / DSP send level */
		Channel.PanAttnL = YM::GEW8::PanAttnL[Data >> 4];
		Channel.PanAttnR = YM::GEW8::PanAttnR[Data >> 4];
		Channel.DspSendLvl = Data & 0x0F; /* Validation needed */

		assert(Channel.DspSendLvl <= 8);
		break;

	case 0x01: /* Wave table number [7:0] */
		Channel.WaveNr.u8l = Data;	
		
		LoadWaveTable(Channel);

		/* Reset address counter after loading a wave bank */
		Channel.ReadAddr = 0;
		break;

	case 0x02: /* Frequency [5:0] / Wave table number [8] */
		Channel.FNum = (Channel.FNum & 0x3C0) | (Data >> 2);
		Channel.WaveNr.u8h = Data & 0x1;
		break;

	case 0x03: /* Octave / Frequency [9:6] */
		Channel.FNum = (Channel.FNum & 0x03F) | ((Data & 0x0F) << 6);
		Channel.FNum9 = Channel.FNum >> 9;
		Channel.Octave = ((Data >> 4) ^ 8) - 8; /* Sign extend [range = +7 : -8] */
		
		assert(Channel.Octave != -8);
		break;

	case 0x04: /* Channel control */
		Channel.KeyLatch	= (Data >> 7) & 0x01;
		Channel.LfoHold		= (Data >> 6) & 0x01;
		Channel.EgUnknown	= (Data >> 3) & 0x01;

		assert((Data & 0x37) == 0); /* Test for undocumented bits */
		break;

	case 0x05: /* Total level / Level direct */
		/* The OPL4 manual page 18 states the weighting for each TL bit as:
		   24dB - 12dB - 6dB - 3dB - 1.5dB - 0.75dB - 0.375dB

		   If all TL bits are set, TL is -95.625dB (validation needed)
		   This "fixes" the out fading tracks in Virtua Racing
		*/
		Channel.TargetTL = Data >> 1;
		Channel.TargetTL |= (Channel.TargetTL + 1) & 0x80;

		/* Level direct */
		if (Data & 0x01) Channel.TotalLevel = Channel.TargetTL;
		break;

	case 0x06: /* LFO Frequency / Vibrato (PM) depth */
		Channel.LfoPeriod = YM::GEW8::LfoPeriod[(Data >> 3) & 0x07];
		Channel.PmDepth = Data & 0x07;
		break;

	case 0x07: /* Attack rate / Tremolo (AM) depth */
		Channel.EgRate[ADSR::Attack] = Data >> 4;
		Channel.AmDepth = Data & 0x07;
		break;

	case 0x08: /* Decay level / Decay rate */
		__debugbreak();

		Channel.DecayLvl = Data >> 4; /* Validation needed */
		Channel.EgRate[ADSR::Decay] = Data & 0x0F; /* Validation needed */

		/* If all DL bits are set, DL is -93dB. See OPL4 manual page 20 */
		Channel.DecayLvl |= (Channel.DecayLvl + 1) & 0x10;
		break;

	case 0x09: /* Rate correction / Release rate */
		/* Virtua Racing writes 0x0F for all channels */
		/* TG100 demo writes 0xFF for all channels */
		Channel.EgRateCorrect = Data >> 4; /* Validation needed */
		Channel.EgRate[ADSR::Release] = Data & 0x0F;
		break;

	case 0x0A: /* Sustain rate */
		/* Virtua Racing writes 0x07 for some channels */
		/* TG100 demo writes 0x01 and 0x02 for some channels */
		Channel.EgRate[ADSR::Sustain] = Data & 0x0F; /* Validation needed */
		
		assert((Data & 0xF0) == 0); /* Test for undocumented bits */
		break;
	
	default:
		__debugbreak();
		break;
	}
}

void YMW258F::LoadWaveTable(YM::GEW8::channel_t& Channel)
{
	/* Read wave table header. Each header is 12-bytes */
	size_t Offset = Channel.WaveNr.u16 * 12;

	/* Wave format (2-bit) */
	Channel.Format = m_Memory[Offset] >> 6;

	/* Start address (22-bit) */
	Channel.StartAddr = ((m_Memory[Offset] << 16) | (m_Memory[Offset + 1] << 8) | m_Memory[Offset + 2]) & 0x3FFFFF;

	/* Loop address (16-bit) */
	Channel.LoopAddr = (m_Memory[Offset + 3] << 8) | m_Memory[Offset + 4];

	/* End address (16-bit) */
	Channel.EndAddr = 0x10000 - ((m_Memory[Offset + 5] << 8) | m_Memory[Offset + 6]);

	/* LFO (3-bit) + Vibrato (3-bit) */
	Channel.LfoPeriod = YM::GEW8::LfoPeriod[(m_Memory[Offset + 7] >> 3) & 0x07];
	Channel.PmDepth = m_Memory[Offset + 7] & 0x07;

	/* Attack rate (4-bit) + Decay rate (4-bit)  */
	Channel.EgRate[ADSR::Attack] = m_Memory[Offset + 8] >> 4;
	Channel.EgRate[ADSR::Decay] = m_Memory[Offset + 8] & 0x0F;

	/* Decay level (4-bit) + Sustain rate (4-bit) */
	Channel.DecayLvl = m_Memory[Offset + 9] >> 4;
	Channel.EgRate[ADSR::Sustain] = m_Memory[Offset + 9] & 0x0F;
	
	/* If all DL bits are set, DL is -93dB. See OPL4 manual page 20 */
	Channel.DecayLvl |= (Channel.DecayLvl + 1) & 0x10;

	/* Rate correction (4-bit) + Release rate (4-bit) */
	Channel.EgRateCorrect = m_Memory[Offset + 10] >> 4;
	Channel.EgRate[ADSR::Release] = m_Memory[Offset + 10] & 0x0F;

	/* Tremolo (3-bit) */
	Channel.AmDepth = m_Memory[Offset + 11] & 0x07;

	/* Appply banking (Sega MultiPCM only) */
	if (m_Banking)
	{
		if (Channel.StartAddr & 0x100000)
		{
			if (Channel.StartAddr & 0x080000)
			{
				Channel.StartAddr = (Channel.StartAddr & 0x7FFFF) | m_Bank1;
			}
			else
			{
				Channel.StartAddr = (Channel.StartAddr & 0x7FFFF) | m_Bank0;
			}
		}
	}
}

void YMW258F::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int32_t AccmL, AccmR, DspAccmL, DspAccmR;
	int16_t DspSampleL, DspSampleR;

	while (Samples-- != 0)
	{
		AccmL = AccmR = DspAccmL = DspAccmR = 0;
		DspSampleL = DspSampleR = 0;

		/* Update global timer */
		m_Timer++;
		
		for (auto& Channel : m_Channel)
		{
			UpdateLFO(Channel);
			UpdateEnvelopeGenerator(Channel);
			UpdateAddressGenerator(Channel);
			UpdateMultiplier(Channel);

			AccmL += Channel.OutputL;
			AccmR += Channel.OutputR;

			/* Validation needed: is DSP send level correctly applied ?? */
			DspAccmL += Channel.OutputL >> (16 - (Channel.DspSendLvl << 1));
			DspAccmR += Channel.OutputR >> (16 - (Channel.DspSendLvl << 1));
		}

		if (m_LDSP != nullptr)
		{
			/* Limit DSP accumulator (signed 18 to 16-bit) */
			DspSampleL = std::clamp(DspAccmL, -131072, 131071) >> 2;
			DspSampleR = std::clamp(DspAccmR, -131072, 131071) >> 2;

			/* Round trip DSP samples */
			m_LDSP->ProcessChannel0(&DspSampleL, &DspSampleR);
		}
		
		/* Limiter (signed 18-bit) */
		AccmL = std::clamp(AccmL + (DspSampleL << 2), -131072, 131071);
		AccmR = std::clamp(AccmR + (DspSampleR << 2), -131072, 131071);

		/* Note: The accumulator is 18-bit, we only output the MSB 16-bits */
		OutBuffer[AudioOut::Default]->WriteSampleS16(AccmL >> 2);
		OutBuffer[AudioOut::Default]->WriteSampleS16(AccmR >> 2);

		/* DSP Test code */
		//OutBuffer[AudioOut::Default]->WriteSampleS16(DspSampleL);
		//OutBuffer[AudioOut::Default]->WriteSampleS16(DspSampleR);
	}
}

void YMW258F::UpdateLFO(YM::GEW8::channel_t& Channel)
{
	if (Channel.LfoHold == 0)
	{
		if (++Channel.LfoCounter >= Channel.LfoPeriod)
		{
			/* Reset counter */
			Channel.LfoCounter = 0;

			/* Increase step counter (8-bit) */
			Channel.LfoStep++;
		}
	}
}

void YMW258F::UpdateAddressGenerator(YM::GEW8::channel_t& Channel)
{
	if (Channel.PgReset)
	{
		/* Reset address counter */
		Channel.ReadAddr = 0;

		/* Reset sample interpolation */
		Channel.SampleT0 = 0;
		Channel.SampleT1 = 0;
	}
	
	/* Vibrato lookup */
	int32_t Vibrato = YM::GEW8::VibratoTable[Channel.LfoStep >> 2][Channel.PmDepth]; /* 64 steps */

	/* Calculate address increment */
	uint32_t Inc = ((1024 + Channel.FNum + Vibrato) << (8 + Channel.Octave)) >> 3;

	/* Update address counter (16.16) */
	uint16_t OldAddr = Channel.ReadAddr.u16h;
	Channel.ReadAddr.u32 += Inc;

	/* Check for delta overflow */
	if (Channel.ReadAddr.u16h != OldAddr)
	{
		size_t Offset;
		
		/* Load new sample */
		Channel.SampleT0 = Channel.SampleT1;

		switch (Channel.Format)
		{
		case 0:
		case 2: /* 8-bit PCM */
			Offset = Channel.StartAddr + OldAddr;
			Channel.SampleT1 = m_Memory[Offset] << 8;
			break;

		case 1:
		case 3: /* 12-bit PCM */
			Offset = Channel.StartAddr + ((OldAddr / 2) * 3);

			if (OldAddr & 0x01) /* 2nd sample */
			{
				Channel.SampleT1 = (m_Memory[Offset + 2] << 8) | ((m_Memory[Offset + 1] & 0x0F) << 4);
			}
			else /* 1st sample */
			{
				Channel.SampleT1 = (m_Memory[Offset + 0] << 8) | (m_Memory[Offset + 1] & 0xF0);
			}
			break;
		}

		/* Check if we reached or exceeded the end address */
		if (Channel.ReadAddr.u16h >= Channel.EndAddr)
		{
			Channel.ReadAddr.u16h -= (Channel.EndAddr - Channel.LoopAddr);
		}
	}

	/* Linear sample interpolation */
	uint32_t T0 = 0x10000 - Channel.ReadAddr.u16l;
	uint32_t T1 = Channel.ReadAddr.u16l;

	Channel.Sample = ((T0 * Channel.SampleT0) + (T1 * Channel.SampleT1)) >> 16;
}

void YMW258F::UpdateEnvelopeGenerator(YM::GEW8::channel_t& Channel)
{	
	/*-------------------------------------*/
	/* Step 1: Key On / Off event handling */
	/*-------------------------------------*/
	uint32_t EnvelopeStart = 0;

	switch ((Channel.KeyLatch << 1) | Channel.KeyState)
	{
	case 0x00:
	case 0x03: /* No key event changes */
		Channel.PgReset = 0;
		break;

	case 0x01: /* Key off event */
		Channel.EgPhase = ADSR::Release;
		Channel.PgReset = 0;
		Channel.KeyState = 0;
		break;

	case 0x02: /* Key on event */
		Channel.EgPhase = ADSR::Attack;
		Channel.PgReset = 1;
		Channel.KeyState = 1;
		EnvelopeStart = 1;
		break;
	}

	/*-------------------------------*/
	/* Step 2: Envelope update cycle */
	/*-------------------------------*/
	uint32_t Rate = Channel.EgRate[Channel.EgPhase];
	
	if (Rate != 0)
	{
		/* How to calculate the actual rate (OPL4 manual page 23):
			RATE = (OCT + RC) x 2 + F9 + RD
			OCT  = Octave (-7 to +7)
			RC   = Rate Correction (0 to 14)
			F9   = Fnum bit 9 (0 or 1)
			RD   = Rate x 4	

			if Rate = 0 : RATE = 0
			if Rate = 15: RATE = 63
		*/
		uint32_t ActualRate = 63;

		if (Rate != 15)
		{
			ActualRate = Rate << 2;
			
			if (Channel.EgRateCorrect != 15)
			{				
				int32_t Correction = (Channel.Octave + Channel.EgRateCorrect) * 2 + Channel.FNum9;
				Correction = std::clamp(Correction, 0, 15);
				ActualRate = std::min(ActualRate + Correction, 63u);
			}
		}

		/* Get timer resolution */
		uint32_t Shift = YM::GEW8::EgShift[ActualRate];
		uint32_t Mask = (1 << Shift) - 1;

		if ((m_Timer & Mask) == 0) /* Timer expired */
		{
			uint16_t Level = Channel.EgLevel;

			/* Get update cycle (8 cycles in total) */
			uint32_t Cycle = (m_Timer >> Shift) & 0x07;

			/* Lookup attenuation adjustment */
			uint32_t AttnInc = YM::GEW8::EgLevelAdjust[ActualRate][Cycle];

			switch (Channel.EgPhase)
			{
			case ADSR::Attack:
				if (ActualRate == 63)
				{
					/* Instant attack */
					if (EnvelopeStart) Level = 0;
				}
				else
				{
					if (Level != 0) Level += ((~Level * AttnInc) >> 4);
				}

				if (Level == 0) Channel.EgPhase = (Channel.DecayLvl != 0) ? ADSR::Decay : ADSR::Sustain;
				break;

			case ADSR::Decay:
				Level += AttnInc;
				if ((Level >> 5) == Channel.DecayLvl) Channel.EgPhase = ADSR::Sustain;
				break;

			case ADSR::Sustain:
			case ADSR::Release:
				Level += AttnInc;
				if (Level >= YM::GEW8::MaxEgLevel) Level = YM::GEW8::MaxAttenuation;
				break;
			}

			Channel.EgLevel = Level;
		}
	}

	/*-------------------------------------*/
	/* Step 3: Envelope output calculation */
	/*-------------------------------------*/
	
	/* Total Level interpolation:

	(( 78.2 * 44100) / 1000) / 127 (TL steps) = ~27 samples
	((156.4 * 44100) / 1000) / 127 (TL steps) = ~54 samples

	This seems wrong IMHO but those are the numbers mentioned in the OPL4 manual.
	*/
	if (Channel.TargetTL != Channel.TotalLevel)
	{
		if (Channel.TotalLevel < Channel.TargetTL) /* Maximum to minimum volume (156.4 msec) */
		{
			if ((m_Timer % 54) == 0) Channel.TotalLevel += 1;
		}
		else /* Minimum to maximum volume (78.2 msec) */
		{
			if ((m_Timer % 27) == 0) Channel.TotalLevel -= 1;
		}
	}

	uint32_t Attn = Channel.EgLevel + (Channel.TotalLevel << 2);

	/* Apply LFO-AM (tremolo) */
	Attn += YM::GEW8::TremoloTable[Channel.LfoStep][Channel.AmDepth];

	/* Apply pan, limit and shift from 4.6 to 4.8 */
	Channel.EgOutputL = std::min(Attn + Channel.PanAttnL, YM::GEW8::MaxAttenuation) << 2;
	Channel.EgOutputR = std::min(Attn + Channel.PanAttnR, YM::GEW8::MaxAttenuation) << 2;
}

void YMW258F::UpdateMultiplier(YM::GEW8::channel_t& Channel)
{
	uint32_t AttnL = Channel.EgOutputL;
	uint32_t AttnR = Channel.EgOutputR;

	/* dB to linear conversion (13-bit) */
	uint32_t VolumeL = YM::GEW8::ExpTable[AttnL & 0xFF] >> (AttnL >> 8);
	uint32_t VolumeR = YM::GEW8::ExpTable[AttnR & 0xFF] >> (AttnR >> 8);

	/* Multiply with interpolated sample (16-bit) */
	Channel.OutputL = (Channel.Sample * VolumeL) >> 13;
	Channel.OutputR = (Channel.Sample * VolumeR) >> 13;
}

void YMW258F::CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void YMW258F::CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(MemoryID, Offset, Data, Size);
}