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
#include "YM2612.h"
#include "YM.h"

#define VGM_WORKAROUND /* Workaround for VGM files */

/*
	Yamaha YM2612 (OPN2)

	- 6 FM Channels
	- 4 Operators per channel
	- 2 Timers (Timer A and B)
	- 1 Shared LFO unit for PM and AM
	- 3CH mode (individual frequency settings for the channel 3 operators)
	- CSM mode (Timer A generated key-on events)
	- Channel 6 can be used for 9-bit PCM playback
	- SSG-EG envelope modes
	- Built-in 9-bit DAC

	A couple of notes:
	- The FM prescaler is fixed to 6
	- There is an overflow bug in the EG counter, causing it to never reach 0
	- The DAC has a discontinuity error aka. "Ladder Effect"
*/

/* Status / Control / IRQ register bits */
#define FLAG_TIMERA		0x01	/* Timer A overflow			*/
#define FLAG_TIMERB		0x02	/* Timer B overflow			*/
#define FLAG_BUSY		0x80	/* Register loading			*/

/* Audio output enumeration */
enum AudioOut
{
	OPN = 0
};

/* Slot naming */
enum SlotName
{
	S1 = 0, S2, S3, S4
};

/* Channel naming */
enum ChannelName
{
	CH1 = 0, CH2, CH3, CH4, CH5, CH6
};

/* Name to Slot ID */
#define O(c, s) { (c << 2) + s}

/* Envelope phases */
enum ADSR: uint32_t
{
	Attack = 0,
	Decay,
	Sustain,
	Release
};

static int16_t DacDiscontinuity[512];

YM2612::YM2612(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed)
{
	static bool Initialized = false;

	/* Build DAC discontinuity table */
	if (!Initialized)
	{
		Initialized = true;

		for (uint16_t i = 0; i < 512; i++)
		{
			/*
			TODO: This needs validation... will need real voltage measurements
			https://docs.google.com/document/d/1ST9GbFfPnIjLT5loytFCm3pB0kWQ1Oe34DCBBV8saY8/pub
			*/

			int16_t DacOut = i & 0xFF;

			if (i & 256) /* Negative output */
			{
				DacOut -= 256;
				DacOut -= 3;
			}
			else /* Positive output */
			{
				/* Do we need to apply an offset on the positive side ? */
				DacOut += 0;
			}

			DacDiscontinuity[i] = DacOut << 5; /* Signed 9 to 14-bit */
		}
	}
	
	YM::OPN::BuildTables();

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YM2612::GetDeviceName()
{
	return L"Yamaha YM2612";
}

void YM2612::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Reset latches */
	m_AddressLatch = 0;
	m_PortLatch = 0;

	/* Reset OPN unit */
	memset(&m_OPN, 0, sizeof(m_OPN));

	/* Default general register state */
	m_OPN.LFO.Period = YM::OPN::LfoPeriod[0];

	/* Default operator register state */
	for (auto& Slot : m_OPN.Slot)
	{
		Slot.Multi = 1; /* x0.5 */
		Slot.EgPhase = ADSR::Release;
		Slot.EgLevel = 0x3FF;
	}

	/* Default channel register state */
	for (auto& Channel : m_OPN.Channel)
	{
		/* All channels are ON by default for OPN compatibility */
		Channel.MaskL = ~0;
		Channel.MaskR = ~0;
	}
}

void YM2612::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	if (Command & 0x100) /* Port 1 */
	{
		Write(0x02, Command & 0xFF);
		Write(0x03, Value);
	}
	else /* Port 0 */
	{
		Write(0x00, Command & 0xFF);
		Write(0x01, Value);
	}
}

bool YM2612::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == AudioOut::OPN)
	{
		Desc.SampleRate = m_ClockSpeed / (6 * 24);
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"FM";
		return true;
	}

	return false;
}

void YM2612::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM2612::GetClockSpeed()
{
	return m_ClockSpeed;
}

uint32_t YM2612::Read(uint32_t Address)
{
	switch (Address & 0x03) /* 2-bit address bus (A0 - A1) */
	{
	case 0x00: /* Read status 0 */
		return m_OPN.Status;

	case 0x01: /* Invalid */
	case 0x02:
	case 0x03:
		return 0;
	}

	return 0;
}

void YM2612::Write(uint32_t Address, uint32_t Data)
{
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;

	/* 2-bit address bus (A0 - A1) */
	Address &= 0x03;

	switch (Address)
	{
	case 0x00: /* Address write mode */
	case 0x02:
		m_AddressLatch = Data;
		m_PortLatch = Address >> 1;
		break;

	case 0x01: /* Data write mode */
	case 0x03:
		if (m_AddressLatch < 0x30) /* Write mode data (0x20 - 0x2F) */
		{
			if (m_PortLatch == 0) /* Only valid for port 0 */
			{
				WriteMode(m_AddressLatch, Data);
			}
		}
		else /* Write FM data (0x30 - 0xB6) */
		{
			WriteFM(m_AddressLatch, m_PortLatch, Data);
		}
		break;
	}
}

void YM2612::WriteMode(uint8_t Address, uint8_t Data)
{
	switch (Address) /* 0x20 - 0x2F */
	{
	case 0x20: /* Not used */
		break;

	case 0x21: /* LSI Test */
		/* Not implemented */
		break;

	case 0x22: /* LFO Control */
		m_OPN.LFO.Enable = (Data & 0x08) ? ~0 : 0; /* Note: implemented as a mask */
		m_OPN.LFO.Period = YM::OPN::LfoPeriod[Data & 0x07];
		break;

	case 0x23: /* Not used */
		break;

	case 0x24: /* Timer A [9:2] */
		m_OPN.TimerA.Period &= 0x03;
		m_OPN.TimerA.Period |= (Data << 2);
		break;

	case 0x25: /* Timer A [1:0] */
		m_OPN.TimerA.Period &= 0x3FC;
		m_OPN.TimerA.Period |= (Data & 0x03);
		break;

	case 0x26: /* Timer B */
		m_OPN.TimerB.Period = Data;
		break;

	case 0x27: /* 3CH mode / Timer control */
	{
		/* Timer A and B start / stop */
		auto StartA = (Data >> 0) & 0x01;
		auto StartB = (Data >> 1) & 0x01;

		if (m_OPN.TimerA.Load ^ StartA)
		{
			m_OPN.TimerA.Load = StartA;
			m_OPN.TimerA.Counter = 1024 - m_OPN.TimerA.Period;
		}

		if (m_OPN.TimerB.Load ^ StartB)
		{
			m_OPN.TimerB.Load = StartB;
			m_OPN.TimerB.Counter = (256 - m_OPN.TimerB.Period) << 4; /* Note: period x16 to sync with Timer A */
		}

		/* Timer A/B enable */
		m_OPN.TimerA.Enable = (Data >> 2) & 0x01;
		m_OPN.TimerB.Enable = (Data >> 3) & 0x01;

		/* Timer A/B overflow flag reset */
		if (Data & 0x10) ClearStatusFlags(FLAG_TIMERA);
		if (Data & 0x20) ClearStatusFlags(FLAG_TIMERB);

		/* 3CH / CSM mode */
		m_OPN.Mode3CH = ((Data & 0xC0) != 0x00) ? 1 : 0;
		m_OPN.ModeCSM = ((Data & 0xC0) == 0x80) ? 1 : 0;
		break;
	}

	case 0x28: /* Key On/Off */
	{
		if ((Data & 0x03) == 0x03) break; /* Invalid channel */

		uint32_t ChannelId = ((Data & 0x03) + ((Data & 0x04) ? 3 : 0)) << 2;

		m_OPN.Slot[ChannelId + S1].KeyLatch = (Data >> 4) & 0x01;
		m_OPN.Slot[ChannelId + S2].KeyLatch = (Data >> 5) & 0x01;
		m_OPN.Slot[ChannelId + S3].KeyLatch = (Data >> 6) & 0x01;
		m_OPN.Slot[ChannelId + S4].KeyLatch = (Data >> 7) & 0x01;

#ifdef VGM_WORKAROUND
		/*	Note: This is a work-around as key events should not be procesed here.
			Some VGM files write consecutive key-on / key-off data without a render update in between.
			This causes latched key data, which has not yet been processed, to be overwritten
		*/
		ProcessKeyEvent(ChannelId + S1);
		ProcessKeyEvent(ChannelId + S2);
		ProcessKeyEvent(ChannelId + S3);
		ProcessKeyEvent(ChannelId + S4);
#endif // VGM_WORKAROUND
		break;
	}

	case 0x29: /* Not used */
		break;

	case 0x2A: /* DAC data */
		m_OPN.DacData &= 0x01;
		m_OPN.DacData |= (Data << 1);
		break;

	case 0x2B: /* DAC select */
		m_OPN.DacSelect = Data >> 7;
		break;

	case 0x2C: /* LSI Test 2 */
		m_OPN.DacData &= ~0x01;
		m_OPN.DacData |= ((Data >> 3) & 0x01);
		break;

	case 0x2D: /* Not used */
		break;

	case 0x2E: /* Not used */
		break;

	case 0x2F: /* Not used */
		break;
	}
}

void YM2612::WriteFM(uint8_t Address, uint8_t Port, uint8_t Data)
{
	/* Slot address mapping: S1 - S3 - S2 - S4 */
	static const int32_t SlotMap[2][16] =
	{
		{
			/* Port 0: Channel 1 - 3 */
			O(CH1,S1), O(CH2,S1), O(CH3,S1), -1, O(CH1,S3), O(CH2,S3), O(CH3,S3), -1,
			O(CH1,S2), O(CH2,S2), O(CH3,S2), -1, O(CH1,S4), O(CH2,S4), O(CH3,S4), -1
		},
		{
			/* Port 1: Channel 4 - 6 */
			O(CH4,S1), O(CH5,S1), O(CH6,S1), -1, O(CH4,S3), O(CH5,S3), O(CH6,S3), -1,
			O(CH4,S2), O(CH5,S2), O(CH6,S2), -1, O(CH4,S4), O(CH5,S4), O(CH6,S4), -1
		}
	};

	int32_t SlotId = SlotMap[Port][Address & 0x0F];
	if (SlotId == -1) return;

	if (Address < 0xA0) /* Slot register map (0x30 - 0x9F) */
	{
		auto& Slot = m_OPN.Slot[SlotId];

		switch (Address & 0xF0)
		{
		case 0x30: /* Detune / Multiply */
			Slot.Detune = (Data >> 4) & 0x07;
			Slot.Multi = (Data & 0x0F) << 1;
			if (Slot.Multi == 0) Slot.Multi = 1;
			break;

		case 0x40: /* Total Level */
			Slot.TotalLevel = (Data & 0x7F) << 3;
			break;

		case 0x50: /* Key Scale / Attack Rate */
			Slot.KeyScale = (Data >> 6);
			Slot.EgRate[ADSR::Attack] = Data & 0x1F;
			break;

		case 0x60: /* Decay Rate / AM On */
			Slot.AmOn = (Data & 0x80) ? ~0 : 0; /* Note: AM On/Off is implemented as a mask */
			Slot.EgRate[ADSR::Decay] = Data & 0x1F;
			break;

		case 0x70: /* Sustain Rate */
			Slot.EgRate[ADSR::Sustain] = Data & 0x1F;
			break;

		case 0x80: /* Sustain Level / Release Rate */
			/* If all SL bits are set, SL is 93dB. See YM2608 manual page 28 */
			Slot.SustainLvl = (Data >> 4) & 0x0F;
			Slot.SustainLvl |= (Slot.SustainLvl + 1) & 0x10;
			Slot.SustainLvl <<= 5;

			/* Map RR from 4 to 5 bits, with LSB always set to 1 */
			Slot.EgRate[ADSR::Release] = ((Data & 0x0F) << 1) | 0x01;
			break;

		case 0x90: /* SSG-EG Envelope Control */
			Slot.SsgEnable = (Data >> 3) & 0x01;
			Slot.SsgEgInv = (Data >> 2) & 0x01;
			Slot.SsgEgAlt = (Data >> 1) & 0x01;
			Slot.SsgEgHld = (Data >> 0) & 0x01;
			break;
		}
	}
	else /* Channel register map (0xA0 - 0xB6) */
	{
		auto& Chan = m_OPN.Channel[SlotId >> 2];

		switch (Address & 0xFC)
		{
		case 0xA0: /* F-Num 1 */
			Chan.FNum = m_OPN.FnumLatch | Data;
			Chan.Block = m_OPN.BlockLatch;
			Chan.KeyCode = (Chan.Block << 2) | YM::OPN::Note[Chan.FNum >> 7];
			break;

		case 0xA4: /* F-Num 2 / Block Latch */
			m_OPN.FnumLatch = (Data & 0x07) << 8;
			m_OPN.BlockLatch = (Data >> 3) & 0x07;
			break;
		case 0xA8: /* 3 Ch-3 F-Num  */
			if (Port == 0)
			{
				/* Slot order for 3CH mode */
				if (Address == 0xA9)
				{
					m_OPN.Fnum3CH[S1] = m_OPN.FnumLatch3CH | Data;
					m_OPN.Block3CH[S1] = m_OPN.BlockLatch3CH;
					m_OPN.KeyCode3CH[S1] = (m_OPN.Block3CH[S1] << 2) | YM::OPN::Note[m_OPN.Fnum3CH[S1] >> 7];
				}
				else if (Address == 0xA8)
				{
					m_OPN.Fnum3CH[S3] = m_OPN.FnumLatch3CH | Data;
					m_OPN.Block3CH[S3] = m_OPN.BlockLatch3CH;
					m_OPN.KeyCode3CH[S3] = (m_OPN.Block3CH[S3] << 2) | YM::OPN::Note[m_OPN.Fnum3CH[S3] >> 7];
				}
				else /* 0xAA */
				{
					m_OPN.Fnum3CH[S2] = m_OPN.FnumLatch3CH | Data;
					m_OPN.Block3CH[S2] = m_OPN.BlockLatch3CH;
					m_OPN.KeyCode3CH[S2] = (m_OPN.Block3CH[S2] << 2) | YM::OPN::Note[m_OPN.Fnum3CH[S2] >> 7];
				}
			}
			break;

		case 0xAC: /* 3 Ch-3 F-Num / Block Latch */
			if (Port == 0)
			{
				m_OPN.FnumLatch3CH = (Data & 0x07) << 8;
				m_OPN.BlockLatch3CH = (Data >> 3) & 0x07;
			}
			break;

		case 0xB0: /* Feedback / Connection */
			Chan.FB = (Data >> 3) & 0x07;
			Chan.Algo = Data & 0x07;
			break;

		case 0xB4: /* PMS / AMS / Panning */
			Chan.MaskL = (Data & 0x80) ? ~0 : 0;
			Chan.MaskR = (Data & 0x40) ? ~0 : 0;
			Chan.AMS = (Data >> 4) & 0x03;
			Chan.PMS = Data & 0x07;
			break;
		}
	}
}

void YM2612::SetStatusFlags(uint8_t Flags)
{
	m_OPN.Status |= Flags;

	if (Flags & (FLAG_TIMERA | FLAG_TIMERB))
	{
		/* TODO: Set interrupt line */
	}
}

void YM2612::ClearStatusFlags(uint8_t Flags)
{
	m_OPN.Status &= ~Flags;
}

void YM2612::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	static const uint32_t SlotOrder[] =
	{
		 O(CH1,S1), O(CH2,S1), O(CH3,S1), O(CH4,S1), O(CH5,S1), O(CH6,S1),
		 O(CH1,S3), O(CH2,S3), O(CH3,S3), O(CH4,S3), O(CH5,S3), O(CH6,S3),
		 O(CH1,S2), O(CH2,S2), O(CH3,S2), O(CH4,S2), O(CH5,S2), O(CH6,S2),
		 O(CH1,S4), O(CH2,S4), O(CH3,S4), O(CH4,S4), O(CH5,S4), O(CH6,S4)
	};

	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / (24 * 6);
	m_CyclesToDo = TotalCycles % (24 * 6);

	while (Samples-- != 0)
	{
		ClearAccumulator();

		/* Update Timer A, Timer B and LFO */
		UpdateTimers();
		UpdateLFO();

		/* Update envelope clock */
		m_OPN.EgClock = (m_OPN.EgClock + 1) % 3;

		/* Update envelope counter */
		m_OPN.EgCounter += (m_OPN.EgClock >> 1);
		m_OPN.EgCounter += (m_OPN.EgCounter >> 12); /* Overflow bug in the OPN unit */
		m_OPN.EgCounter &= 0xFFF;

		/* Update slots (operators) */
		for (auto& Slot : SlotOrder)
		{
			PrepareSlot(Slot);
			UpdatePhaseGenerator(Slot);
			UpdateEnvelopeGenerator(Slot);
			UpdateOperatorUnit(Slot);
		}

		UpdateAccumulator(CH1);
		UpdateAccumulator(CH2);
		UpdateAccumulator(CH3);
		UpdateAccumulator(CH4);
		UpdateAccumulator(CH5);
		UpdateAccumulator(CH6);

		/* Limiter (signed 16-bit) */
		int16_t Mol = std::clamp(m_OPN.OutL, -32768, 32767);
		int16_t Mor = std::clamp(m_OPN.OutR, -32768, 32767);

		/* 16-bit output */
		OutBuffer[AudioOut::OPN]->WriteSampleS16(Mol);
		OutBuffer[AudioOut::OPN]->WriteSampleS16(Mor);
	}
}

void YM2612::PrepareSlot(uint32_t SlotId)
{
	uint32_t ChannelId = SlotId >> 2;
	auto& Chan = m_OPN.Channel[ChannelId];
	auto& Slot = m_OPN.Slot[SlotId];

	/* Copy some values for later processing */
	Slot.FNum = Chan.FNum;
	Slot.Block = Chan.Block;
	Slot.KeyCode = Chan.KeyCode;

	if (m_OPN.Mode3CH)
	{
		auto i = SlotId & 3;

		/* Get Block/FNum for channel 3: S1-S2-S3 */
		if ((ChannelId == CH3) && (i != S4))
		{
			Slot.FNum = m_OPN.Fnum3CH[i];
			Slot.Block = m_OPN.Block3CH[i];
			Slot.KeyCode = m_OPN.KeyCode3CH[i];
		}
	}
}

void YM2612::UpdatePhaseGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPN.Channel[SlotId >> 2];
	auto& Slot = m_OPN.Slot[SlotId];

	uint32_t FNum = Slot.FNum << 1; /* 11 to 12-bit */

	/* LFO frequency modulation (12-bit result) */
	FNum = (FNum + YM::OPN::LfoPmTable[FNum >> 5][m_OPN.LFO.Step >> 2][Chan.PMS]) & 0xFFF;

	/* Block shift (17-bit result) */
	uint32_t Inc = (FNum << Slot.Block) >> 2;

	/* Detune (17-bit result, might overflow) */
	Inc = (Inc + YM::OPN::Detune[Slot.KeyCode][Slot.Detune]) & 0x1FFFF;

	/* Multiply (20-bit result) */
	Inc = (Inc * Slot.Multi) >> 1;

	/* Update phase counter (20-bit) */
	Slot.PgPhase = (Slot.PgPhase + Inc) & 0xFFFFF;
}

void YM2612::UpdateEnvelopeGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPN.Channel[SlotId >> 2];
	auto& Slot = m_OPN.Slot[SlotId];

	/*-------------------------------------*/
	/* Step 0: Key On / Off event handling */
	/*-------------------------------------*/
	ProcessKeyEvent(SlotId);

	/*-----------------------------*/
	/* Step 1: SSG-EG update cycle */
	/*-----------------------------*/
	if ((Slot.EgLevel >> 9) & Slot.SsgEnable)
	{
		if (Slot.KeyOn) /* Attack, decay or sustain phase */
		{
			if (Slot.SsgEgHld) /* Hold mode */
			{
				/* Set output inversion to the hold state */
				Slot.SsgEgInvOut = Slot.SsgEgInv ^ Slot.SsgEgAlt;
			}
			else /* Repeating mode */
			{
				StartEnvelope(SlotId);

				/* Flip output inversion flag (if alternating) */
				Slot.SsgEgInvOut ^= Slot.SsgEgAlt;

				/* Restart the phase counter when we are repeating normally (not alternating) */
				//if (Slot.SsgEgAlt == 0) Slot.PgPhase = 0;
				Slot.PgPhase &= ~(Slot.SsgEgAlt - 1);
			}
		}
		else /* Release phase */
		{
			/* Force the EG attenuation to maximum when we hit 0x200 during release */
			Slot.EgLevel = 0x3FF;
		}
	}

	/*-------------------------------*/
	/* Step 2: Envelope update cycle */
	/*-------------------------------*/
	if (m_OPN.EgClock == 2)
	{
		/* When attacking, move to the decay phase when attenuation level is minimal */
		if ((Slot.EgPhase | Slot.EgLevel) == 0)
		{
			Slot.EgPhase = ADSR::Decay;
		}

		/* If we reached the sustain level, move to the sustain phase */
		if ((Slot.EgPhase == ADSR::Decay) && (Slot.EgLevel >= Slot.SustainLvl))
		{
			Slot.EgPhase = ADSR::Sustain;
		}

		/* Get key scaled rate */
		uint32_t Rate = CalculateRate(Slot.EgRate[Slot.EgPhase], Slot.KeyCode, Slot.KeyScale);

		/* Get EG counter resolution */
		uint32_t Shift = YM::OPN::EgShift[Rate];
		uint32_t Mask = (1 << Shift) - 1;

		if ((m_OPN.EgCounter & Mask) == 0) /* Counter overflowed */
		{
			uint16_t Level = Slot.EgLevel;

			/* Get update cycle (8 cycles in total) */
			uint32_t Cycle = (m_OPN.EgCounter >> Shift) & 0x07;

			/* Lookup attenuation adjustment */
			uint32_t AttnInc = YM::OPN::EgLevelAdjust[Rate][Cycle];

			if (Slot.EgPhase == ADSR::Attack) /* Exponential attack */
			{
				if (Rate < 62)
				{
					Level += ((~Level * AttnInc) >> 4);
				}
			}
			else /* Linear decay */
			{
				/* When SSG-EG is active, don't update once we hit 0x200 */
				if (((Level >> 9) & Slot.SsgEnable) == 0)
				{
					Level += AttnInc << (Slot.SsgEnable << 1);

					/* Limit to maximum attenuation */
					if (Level > 0x3FF) Level = 0x3FF;
				}
			}

			Slot.EgLevel = Level;
		}
	}

	/*-------------------------------------*/
	/* Step 3: Envelope output calculation */
	/*-------------------------------------*/
	uint32_t Attn = Slot.EgLevel;

	/* Apply SGG-EG output inversion */
	if (Slot.SsgEgInvOut) Attn = (0x200 - Attn) & 0x3FF;

	/* Apply total level */
	Attn += Slot.TotalLevel;

	/* Apply AM LFO */
	Attn += (YM::OPN::LfoAmTable[m_OPN.LFO.Step][Chan.AMS] & Slot.AmOn);

	/* Limit (10-bit = 4.6 fixed point) */
	if (Attn > 0x3FF) Attn = 0x3FF;

	/* Convert from 4.6 to 4.8 fixed point */
	Slot.EgOutput = Attn << 2;
}

void YM2612::UpdateOperatorUnit(uint32_t SlotId)
{
	auto& Slot = m_OPN.Slot[SlotId];

	/* Phase modulation (10-bit) */
	uint32_t Phase = (Slot.PgPhase >> 10) + GetModulation(SlotId);

	/* Attenuation (4.8 + 4.8 = 5.8 fixed point) */
	uint32_t Level = YM::SineTable[Phase & 0x1FF] + Slot.EgOutput;

	/* dB to linear conversion (13-bit) */
	int16_t Output = YM::ExpTable[Level & 0xFF] >> (Level >> 8);

	/* Negate output (14-bit) */
	if (Phase & 0x200) Output = -Output;

	/* The last 2 generated samples are stored */
	Slot.Output[1] = Slot.Output[0];
	Slot.Output[0] = Output;
}

void YM2612::ClearAccumulator()
{
	m_OPN.OutL = 0;
	m_OPN.OutR = 0;
}

void YM2612::UpdateAccumulator(uint32_t ChannelId)
{
	int16_t Output = 0;
	uint32_t SlotId = ChannelId << 2;

	if ((ChannelId == CH6) && m_OPN.DacSelect)
	{
		Output = m_OPN.DacData - 0x100;
	}
	else
	{
		/* Accumulate output */
		switch (m_OPN.Channel[ChannelId].Algo)
		{
		case 0:
		case 1:
		case 2:
		case 3: /* S4 */
			Output += m_OPN.Slot[SlotId + S4].Output[0] >> 5;
			break;

		case 4: /* S2 + S4 */
			Output += m_OPN.Slot[SlotId + S2].Output[0] >> 5;
			Output += m_OPN.Slot[SlotId + S4].Output[0] >> 5;
			break;

		case 5:
		case 6: /* S2 + S3 + S4 */
			Output += m_OPN.Slot[SlotId + S2].Output[0] >> 5;
			Output += m_OPN.Slot[SlotId + S3].Output[0] >> 5;
			Output += m_OPN.Slot[SlotId + S4].Output[0] >> 5;
			break;

		case 7: /* S1 + S2 + S3 + S4 */
			Output += m_OPN.Slot[SlotId + S1].Output[0] >> 5;
			Output += m_OPN.Slot[SlotId + S2].Output[0] >> 5;
			Output += m_OPN.Slot[SlotId + S3].Output[0] >> 5;
			Output += m_OPN.Slot[SlotId + S4].Output[0] >> 5;
			break;
		}
	}

	/* Limit (signed 9-bit) */
	Output = std::clamp<int16_t>(Output, -256, 255);

	/* Generate DAC output */
	Output = DacDiscontinuity[Output & 0x1FF];

	/* Mix channel output */
	m_OPN.OutL += Output & m_OPN.Channel[ChannelId].MaskL;
	m_OPN.OutR += Output & m_OPN.Channel[ChannelId].MaskR;
}

int16_t YM2612::GetModulation(uint32_t Cycle)
{
	auto& Chan = m_OPN.Channel[Cycle >> 2];

	uint32_t SlotId = Cycle & 0x03;
	uint32_t ChanId = Cycle & ~0x03;

	switch (((Chan.Algo << 2) | SlotId) & 0x1F)
	{
	case 0x00: /* Algo: 0 - S1 */
	case 0x04: /* Algo: 1 - S1 */
	case 0x08: /* Algo: 2 - S1 */
	case 0x0C: /* Algo: 3 - S1 */
	case 0x10: /* Algo: 4 - S1 */
	case 0x14: /* Algo: 5 - S1 */
	case 0x18: /* Algo: 6 - S1 */
	case 0x1C: /* Algo: 7 - S1 */
		if (Chan.FB) /* Slot 1 self-feedback modulation (10-bit) */
			return (m_OPN.Slot[Cycle].Output[0] + m_OPN.Slot[Cycle].Output[1]) >> (10 - Chan.FB);
		else
			return 0;

	case 0x01: /* Algo: 0 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x02: /* Algo: 0 - S3 */
		return m_OPN.Slot[ChanId + S2].Output[0] >> 1;

	case 0x03: /* Algo: 0 - S4 */
		return m_OPN.Slot[ChanId + S3].Output[0] >> 1;

	case 0x05: /* Algo: 1 - S2 */
		return 0;

	case 0x06: /* Algo: 1 - S3 */
		return (m_OPN.Slot[ChanId + S1].Output[1] + m_OPN.Slot[ChanId + S2].Output[0]) >> 1;

	case 0x07: /* Algo: 1 - S4 */
		return m_OPN.Slot[ChanId + S3].Output[0] >> 1;

	case 0x09: /* Algo: 2 - S2 */
		return 0;

	case 0x0A: /* Algo: 2 - S3 */
		return m_OPN.Slot[ChanId + S2].Output[0] >> 1;

	case 0x0B: /* Algo: 2 - S4 */
		return (m_OPN.Slot[ChanId + S1].Output[0] + m_OPN.Slot[ChanId + S3].Output[0]) >> 1;

	case 0x0D: /* Algo: 3 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x0E: /* Algo: 3 - S3 */
		return 0;

	case 0x0F: /* Algo: 3 - S4 */
		return (m_OPN.Slot[ChanId + S2].Output[1] + m_OPN.Slot[ChanId + S3].Output[0]) >> 1;

	case 0x11: /* Algo: 4 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x12: /* Algo: 4 - S3 */
		return 0;

	case 0x13: /* Algo: 4 - S4 */
		return m_OPN.Slot[ChanId + S3].Output[0] >> 1;

	case 0x15: /* Algo: 5 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x16: /* Algo: 5 - S3 */
		return m_OPN.Slot[ChanId + S1].Output[1] >> 1;

	case 0x17: /* Algo: 5 - S4 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x19: /* Algo: 6 - S2 */
		return m_OPN.Slot[ChanId + S1].Output[0] >> 1;

	case 0x1A: /* Algo: 6 - S3 */
	case 0x1B: /* Algo: 6 - S4 */
		return 0;

	case 0x1D: /* Algo: 7 - S2 */
	case 0x1E: /* Algo: 7 - S3 */
	case 0x1F: /* Algo: 7 - S4 */
		return 0;
	}

	return 0;
}

uint8_t YM2612::CalculateRate(uint8_t Rate, uint8_t KeyCode, uint8_t KeyScale)
{
	uint8_t ScaledRate = 0;

	/* YM2608 manual page 30 */
	if (Rate != 0)
	{
		/* Calculate key scale value */
		uint8_t KSV = KeyCode >> (3 - KeyScale);

		/* Calculate key scaled rate */
		ScaledRate = (Rate << 1) + KSV;

		/* Limit to a max. of 63 */
		if (ScaledRate > 63) ScaledRate = 63;
	}

	return ScaledRate;
}

void YM2612::ProcessKeyEvent(uint32_t SlotId)
{
	auto& Slot = m_OPN.Slot[SlotId];

	/* Get latched key on/off state */
	uint32_t NewState = (Slot.KeyLatch | Slot.CsmKeyLatch);

	/* Clear CSM key on flag */
	Slot.CsmKeyLatch = 0;

	if (Slot.KeyOn ^ NewState)
	{
		if (NewState) /* Key On */
		{
			/* Start envelope */
			StartEnvelope(SlotId);

			/* Reset phase counter */
			Slot.PgPhase = 0;

			/* Set SSG-EG inverted ouput flag to the initial state when we are in any SSG-EG inverted mode */
			Slot.SsgEgInvOut = Slot.SsgEnable & Slot.SsgEgInv;
		}
		else /* Key Off */
		{
			/* Move envelope to release phase */
			Slot.EgPhase = ADSR::Release;

			if (Slot.SsgEgInvOut)
			{
				/* Allow the release phase to continue normally */
				Slot.EgLevel = (0x200 - Slot.EgLevel) & 0x3FF;

				/* Clear the SSG-EG inverted output flag */
				Slot.SsgEgInvOut = 0;
			}
		}

		Slot.KeyOn = NewState;
	}
}

void YM2612::StartEnvelope(uint32_t SlotId)
{
	auto& Slot = m_OPN.Slot[SlotId];

	/* Move envelope to attack phase */
	Slot.EgPhase = ADSR::Attack;

	/* Instant attack */
	if (CalculateRate(Slot.EgRate[ADSR::Attack], Slot.KeyCode, Slot.KeyScale) >= 62)
	{
		/* Instant minimum attenuation */
		Slot.EgLevel = 0;
	}
}

void YM2612::UpdateLFO()
{
	if (++m_OPN.LFO.Counter >= m_OPN.LFO.Period)
	{
		m_OPN.LFO.Counter = 0;
		m_OPN.LFO.Step = (m_OPN.LFO.Step + 1) & 0x7F;
	}

	m_OPN.LFO.Step &= m_OPN.LFO.Enable;
}

void YM2612::UpdateTimers()
{
	if (m_OPN.TimerA.Load)
	{
		if (--m_OPN.TimerA.Counter == 0)
		{
			m_OPN.TimerA.Counter = 1024 - m_OPN.TimerA.Period;

			/* Overflow flag enabled */
			if (m_OPN.TimerA.Enable) SetStatusFlags(FLAG_TIMERA);

			/* CSM Key On */
			if (m_OPN.ModeCSM)
			{
				/* CSM Key-On all channel 3 slots */
				m_OPN.Slot[8 + S1].CsmKeyLatch = 1;
				m_OPN.Slot[8 + S2].CsmKeyLatch = 1;
				m_OPN.Slot[8 + S3].CsmKeyLatch = 1;
				m_OPN.Slot[8 + S4].CsmKeyLatch = 1;
			}
		}
	}

	if (m_OPN.TimerB.Load)
	{
		if (--m_OPN.TimerB.Counter == 0)
		{
			m_OPN.TimerB.Counter = (256 - m_OPN.TimerB.Period) << 4;

			/* Overflow flag enabled */
			if (m_OPN.TimerB.Enable) SetStatusFlags(FLAG_TIMERB);
		}
	}
}