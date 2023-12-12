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
#include "YM2203.h"

#define VGM_WORKAROUND /* Workaround for VGM files */

/*
	Yamaha YM2203 (OPN)

	- 3 FM Channels
	- 4 Operators per channel
	- 2 Timers (Timer A and B)
	- 3CH mode (individual frequency settings for the channel 3 operators)
	- CSM mode (Timer A generated key-on events)
	- SSG-EG envelope modes
	- SSG unit

	Needs fixing:
	- Currently using a simplified OPN generation method. No 12-stage pipeline yet

	Not implemented:
	 - Interrupts
	 - SSG IO ports
	 - OPN / SSG prescalers. Code is there but disabled due to my underlying sound engine not supporting sample rates above 200kHz
*/

/* Status register bits */
#define FLAG_TIMERA		0x01	/* Timer A overflow			*/
#define FLAG_TIMERB		0x02	/* Timer B overflow			*/
#define FLAG_BUSY		0x80	/* Register loading			*/

/* Audio output enumeration */
enum AudioOut
{
	SSGA = 0,
	SSGB,
	SSGC,
	OPN
};

/* Slot naming */
enum SlotName
{
	S1 = 0, S2, S3, S4
};

/* Channel naming */
enum ChannelName
{
	CH1 = 0, CH2, CH3
};

/* Name to Slot ID */
#define O(c, s) { (c << 2) + s}

/* Envelope phases */
enum ADSR : uint32_t
{
	Attack = 0,
	Decay,
	Sustain,
	Release
};

YM2203::YM2203(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed)
{
	YM::OPN::BuildTables();

	Reset(ResetType::PowerOnDefaults);
	if (ClockSpeed <= 1500000)
	{
		m_PreScalerOPN = 2;
		m_PreScalerSSG = 1;
	}
	else if (ClockSpeed <= 2000000)
	{
		m_PreScalerOPN = 3;
		m_PreScalerSSG = 2;
	}
	else
	{
		m_PreScalerOPN = 6;
		m_PreScalerSSG = 4;
	}
}

const wchar_t* YM2203::GetDeviceName()
{
	return L"Yamaha YM2203";
}

void YM2203::Reset(ResetType Type)
{
	m_CyclesToDoSSG = 0;
	m_CyclesToDoOPN = 0;

	/* Reset prescalers */
	//m_PreScalerOPN = 6;
	//m_PreScalerSSG = 4;

	/* Reset latches */
	m_AddressLatch = 0;

	/* Reset SSG unit */
	memset(&m_SSG, 0, sizeof(m_SSG));

	/* Default SSG noise state */
	m_SSG.Noise.LFSR = 1 << (17 - 1);

	/* Default SSG envelope state */
	m_SSG.Envelope.Counter = 0;
	m_SSG.Envelope.Period.u32 = 0;
	m_SSG.Envelope.Amplitude = AY::Amplitude32[31];
	m_SSG.Envelope.Step = 31;
	m_SSG.Envelope.StepDec = 1;
	m_SSG.Envelope.Hld = 1;
	m_SSG.Envelope.Alt = 31;
	m_SSG.Envelope.Inv = 0;

	/* Reset OPN unit */
	memset(&m_OPN, 0, sizeof(m_OPN));

	/* Default operator register state */
	for (auto& Slot : m_OPN.Slot)
	{
		Slot.Multi = 1; /* x0.5 */
		Slot.EgPhase = ADSR::Release;
		Slot.EgLevel = 0x3FF;
	}
}

void YM2203::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	Write(0x00, Command);
	Write(0x01, Value);
}

bool YM2203::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	switch (OutputNr)
	{
	case AudioOut::SSGA: /* SSG - Channel A */
		Desc.SampleRate = m_ClockSpeed / (8 * m_PreScalerSSG);
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"Channel A";
		return true;

	case AudioOut::SSGB: /* SSG - Channel B */
		Desc.SampleRate = m_ClockSpeed / (8 * m_PreScalerSSG);
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"Channel B";
		return true;

	case AudioOut::SSGC: /* SSG - Channel C */
		Desc.SampleRate = m_ClockSpeed / (8 * m_PreScalerSSG);
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"Channel C";
		return true;

	case AudioOut::OPN: /* FM */
		Desc.SampleRate = m_ClockSpeed / (12 * m_PreScalerOPN);
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"FM";
		return true;
	}

	return false;
}

void YM2203::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM2203::GetClockSpeed()
{
	return m_ClockSpeed;
}

uint32_t YM2203::Read(int32_t Address)
{
	if ((Address & 0x01) == 0) /* Read status */
	{
		return m_OPN.Status;
	}
	else /* Read SSG registers */
	{
		if (m_AddressLatch < 0x10)
		{
			return m_SSG.Register[m_AddressLatch];
		}
	}

	return 0;
}

void YM2203::Write(uint32_t Address, uint32_t Data)
{
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;

	if ((Address & 0x01) == 0) /* Address write mode */
	{
		m_AddressLatch = Data;
	}
	else /* Data write mode */
	{
		switch (m_AddressLatch & 0xF0)
		{
		case 0x00: /* Write SSG data (0x00 - 0x0F) */
			WriteSSG(m_AddressLatch, Data);
			break;

		case 0x10: /* Not used (0x10 - 0x1F) */
			break;

		case 0x20: /* Write OPN mode data (0x20 - 0x2F) */
			WriteMode(m_AddressLatch, Data);
			break;

		default: /* Write OPN FM data (0x30 - 0xB6) */
			WriteFM(m_AddressLatch, Data);
			break;
		}
	}
}

void YM2203::WriteSSG(uint8_t Address, uint8_t Data)
{
	Address &= 0x0F;
	m_SSG.Register[Address] = Data;

	Data &= AY::Mask[Address]; /* Mask unused bits after storing */

	switch (Address)
	{
	case 0x00: /* Channel A Tone Period (Fine Tune) */
		m_SSG.Tone[0].Period.u8ll = Data;
		break;

	case 0x01: /* Channel A Tone Period (Coarse Tune) */
		m_SSG.Tone[0].Period.u8lh = Data;
		break;

	case 0x02: /* Channel B Tone Period (Fine Tune) */
		m_SSG.Tone[1].Period.u8ll = Data;
		break;

	case 0x03: /* Channel B Tone Period (Coarse Tune) */
		m_SSG.Tone[1].Period.u8lh = Data;
		break;

	case 0x04: /* Channel C Tone Period (Fine Tune) */
		m_SSG.Tone[2].Period.u8ll = Data;
		break;

	case 0x05: /* Channel C Tone Period (Coarse Tune) */
		m_SSG.Tone[2].Period.u8lh = Data;
		break;

	case 0x06: /* Noise Period */
		m_SSG.Noise.Period = Data;
		break;

	case 0x07: /* Mixer Control - I/O Enable */
		m_SSG.Tone[0].ToneDisable = (Data >> 0) & 1;
		m_SSG.Tone[1].ToneDisable = (Data >> 1) & 1;
		m_SSG.Tone[2].ToneDisable = (Data >> 2) & 1;

		m_SSG.Tone[0].NoiseDisable = (Data >> 3) & 1;
		m_SSG.Tone[1].NoiseDisable = (Data >> 4) & 1;
		m_SSG.Tone[2].NoiseDisable = (Data >> 5) & 1;
		break;

	case 0x08: /* Channel A Amplitude Control */
		m_SSG.Tone[0].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_SSG.Tone[0].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x09: /* Channel B Amplitude Control */
		m_SSG.Tone[1].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_SSG.Tone[1].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0A: /* Channel C Amplitude Control */
		m_SSG.Tone[2].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_SSG.Tone[2].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0B: /* Envelope Period (Fine Tune) */
		m_SSG.Envelope.Period.u8ll = Data;
		break;

	case 0x0C: /* Envelope Period (Coarse Tune) */
		m_SSG.Envelope.Period.u8lh = Data;
		break;

	case 0x0D: /* Envelope Shape / Cycle Control */
		m_SSG.Envelope.Counter = 0;
		m_SSG.Envelope.Step = 31;
		m_SSG.Envelope.StepDec = 1;

		/* If attacking, apply output inversion */
		m_SSG.Envelope.Inv = (Data & 0x04) ? 31 : 0;

		if (Data & 0x08) /* Continuous cycles */
		{
			m_SSG.Envelope.Hld = Data & 0x01;

			if (m_SSG.Envelope.Hld)
				m_SSG.Envelope.Alt = (Data & 0x02) ? 0 : 31;
			else
				m_SSG.Envelope.Alt = (Data & 0x02) ? 31 : 0;
		}
		else /* Single cycle */
		{
			m_SSG.Envelope.Hld = 1;
			m_SSG.Envelope.Alt = m_SSG.Envelope.Inv ^ 31;
		}

		/* Set initial ouput volume */
		m_SSG.Envelope.Amplitude = AY::Amplitude32[m_SSG.Envelope.Step ^ m_SSG.Envelope.Inv];
		break;

	case 0x0E: /* I/O Port A Data Store */
		/* Not implemented */
		break;

	case 0x0F: /* I/O Port B Data Store */
		/* Not implemented */
		break;
	}
}

void YM2203::WriteMode(uint8_t Address, uint8_t Data)
{
	switch (Address) /* 0x20 - 0x2F */
	{
	case 0x20: /* Not used */
		break;

	case 0x21: /* LSI Test */
		/* Not implemented */
		break;

	case 0x22: /* Not used */
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

		uint32_t ChannelId = (Data & 0x03) << 2;

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

	case 0x2A: /* Not used */
		break;

	case 0x2B: /* Not used */
		break;

	case 0x2C: /* Not used */
		break;

	case 0x2D: /* Prescaler selection (/6) */
		/*m_PreScalerOPN = 6;
		m_PreScalerSSG = 4;*/
		break;

	case 0x2E: /* Prescaler selection (/3) */
		/*if (m_PreScalerOPN == 6)
		{
			m_PreScalerOPN = 3;
			m_PreScalerSSG = 2;
		}*/
		break;

	case 0x2F: /* Prescaler selection (/2) */
		/*m_PreScalerOPN = 2;
		m_PreScalerSSG = 1;*/
		break;
	}
}

void YM2203::WriteFM(uint8_t Address, uint8_t Data)
{
	/* Slot address mapping: S1 - S3 - S2 - S4 */
	static const int32_t SlotMap[16] =
	{
		O(CH1,S1), O(CH2,S1), O(CH3,S1), -1, O(CH1,S3), O(CH2,S3), O(CH3,S3), -1,
		O(CH1,S2), O(CH2,S2), O(CH3,S2), -1, O(CH1,S4), O(CH2,S4), O(CH3,S4), -1
	};

	int32_t SlotId = SlotMap[Address & 0x0F];
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
			m_OPN.FnumLatch  = (Data & 0x07) << 8;
			m_OPN.BlockLatch = (Data >> 3) & 0x07;
			break;

		case 0xA8: /* 3 Ch-3 F-Num  */
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
			break;

		case 0xAC: /* 3 Ch-3 F-Num / Block Latch */
			m_OPN.FnumLatch3CH  = (Data & 0x07) << 8;
			m_OPN.BlockLatch3CH = (Data >> 3) & 0x07;
			break;

		case 0xB0: /* Feedback / Connection */
			Chan.FB = (Data >> 3) & 0x07;
			Chan.Algo = Data & 0x07;
			break;
		}
	}
}

void YM2203::SetStatusFlags(uint8_t Flags)
{
	m_OPN.Status |= Flags;

	if (Flags & (FLAG_TIMERA | FLAG_TIMERB))
	{
		/* TODO: Set interrupt line */
	}
}

void YM2203::ClearStatusFlags(uint8_t Flags)
{
	m_OPN.Status &= ~Flags;
}

void YM2203::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	UpdateSSG(ClockCycles, OutBuffer);
	UpdateOPN(ClockCycles, OutBuffer);
}

void YM2203::UpdateSSG(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDoSSG;
	uint32_t Samples = TotalCycles / (8 * m_PreScalerSSG);
	m_CyclesToDoSSG = TotalCycles % (8 * m_PreScalerSSG);

	int16_t Out;
	uint32_t Mask;

	while (Samples-- != 0)
	{
		/* Update envelope generator */
		if ((m_SSG.Envelope.Counter += 2) >= m_SSG.Envelope.Period.u32) //FIXME: should be += 1
		{
			/* Reset counter */
			m_SSG.Envelope.Counter = 0;

			/* Count down step counter (31 -> 0) */
			m_SSG.Envelope.Step -= m_SSG.Envelope.StepDec;

			if (m_SSG.Envelope.Step & 32) /* Envelope cycle completed */
			{
				/* Restart cycle */
				m_SSG.Envelope.Step = 31;

				/* Stop counting (if needed) */
				m_SSG.Envelope.StepDec = m_SSG.Envelope.Hld ^ 1;

				/* Toggle output inversion */
				m_SSG.Envelope.Inv ^= m_SSG.Envelope.Alt;
			}

			/* Apply output inversion and lookup amplitude */
			m_SSG.Envelope.Amplitude = AY::Amplitude32[m_SSG.Envelope.Step ^ m_SSG.Envelope.Inv];
		}

		/* Update noise generator */
		if (m_SSG.Noise.Prescaler ^= 1)
		{
			if ((m_SSG.Noise.Counter += 2) >= m_SSG.Noise.Period) //FIXME: should be += 1
			{
				/* Reset counter */
				m_SSG.Noise.Counter = 0;

				/* Update output flag */
				m_SSG.Noise.Output = m_SSG.Noise.LFSR & 1;

				/* Tap bits 3 and 0 (XOR feedback) */
				uint32_t Seed = ((m_SSG.Noise.LFSR >> 3) ^ (m_SSG.Noise.LFSR >> 0)) & 1;

				/* Shift LFSR and apply seed (17-bit wide) */
				m_SSG.Noise.LFSR = (m_SSG.Noise.LFSR >> 1) | (Seed << 16);
			}
		}

		/* Update, mix and output tone generators */
		for (auto i = 0; i < 3; i++)
		{
			auto& Tone = m_SSG.Tone[i];

			if ((Tone.Counter += 2) >= Tone.Period.u32) //FIXME: should be += 1
			{
				/* Reset counter */
				Tone.Counter = 0;

				/* Toggle output flag */
				Tone.Output ^= 1;
			}

			/* Mix tone and noise (implemented as a mask) */
			Mask = ~(((Tone.Output | Tone.ToneDisable) & (m_SSG.Noise.Output | Tone.NoiseDisable)) - 1);

			/* Amplitude control */
			Out = Tone.AmpCtrl ? m_SSG.Envelope.Amplitude : Tone.Amplitude;

			/* 16-bit output */
			OutBuffer[AudioOut::SSGA + i]->WriteSampleS16((Out & Mask) >> 1);
		}
	}
}

void YM2203::UpdateOPN(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	static const uint32_t SlotOrder[] =
	{
		 O(CH1,S1), O(CH2,S1), O(CH3,S1), O(CH1,S3), O(CH2,S3), O(CH3,S3),
		 O(CH1,S2), O(CH2,S2), O(CH3,S2), O(CH1,S4), O(CH2,S4), O(CH3,S4)
	};
	
	uint32_t TotalCycles = ClockCycles + m_CyclesToDoOPN;
	uint32_t Samples = TotalCycles / (12 * m_PreScalerOPN);
	m_CyclesToDoOPN = TotalCycles % (12 * m_PreScalerOPN);

	while (Samples-- != 0)
	{
		ClearAccumulator();

		/* Update Timer A and Timer B*/
		UpdateTimers();

		/* Update envelope clock */
		m_OPN.EgClock = (m_OPN.EgClock + 1) % 3;

		/* Update envelope counter */
		m_OPN.EgCounter = (m_OPN.EgCounter + (m_OPN.EgClock >> 1)) & 0xFFF;

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

		/* 16-bit output */
		OutBuffer[AudioOut::OPN]->WriteSampleS16(m_OPN.Out);
	}
}

void YM2203::PrepareSlot(uint32_t SlotId)
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

void YM2203::UpdatePhaseGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPN.Channel[SlotId >> 2];
	auto& Slot = m_OPN.Slot[SlotId];

	/* Block shift (17-bit result) */
	uint32_t Inc = (Slot.FNum << Slot.Block) >> 1;

	/* Detune (17-bit result, might overflow) */
	Inc = (Inc + YM::OPN::Detune[Slot.KeyCode][Slot.Detune]) & 0x1FFFF;

	/* Multiply (20-bit result) */
	Inc = (Inc * Slot.Multi) >> 1;

	/* Update phase counter (20-bit) */
	Slot.PgPhase = (Slot.PgPhase + Inc) & 0xFFFFF;
}

void YM2203::UpdateEnvelopeGenerator(uint32_t SlotId)
{
	uint32_t ChannelId = SlotId >> 2;
	auto& Chan = m_OPN.Channel[ChannelId];
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
	if (m_OPN.EgClock == ChannelId)
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

	/* Limit (10-bit = 4.6 fixed point) */
	if (Attn > 0x3FF) Attn = 0x3FF;

	/* Convert from 4.6 to 4.8 fixed point */
	Slot.EgOutput = Attn << 2;
}

void YM2203::UpdateOperatorUnit(uint32_t SlotId)
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

void YM2203::ClearAccumulator()
{
	m_OPN.Out = 0;
}

void YM2203::UpdateAccumulator(uint32_t ChannelId)
{
	int16_t Output = 0;
	uint32_t SlotId = ChannelId << 2;

	/* Accumulate output */
	switch (m_OPN.Channel[ChannelId].Algo)
	{
	case 0:
	case 1:
	case 2:
	case 3: /* S4 */
		Output = m_OPN.Slot[SlotId + S4].Output[0];
		break;

	case 4: /* S2 + S4 */
		Output = m_OPN.Slot[SlotId + S2].Output[0] + m_OPN.Slot[SlotId + S4].Output[0];
		break;

	case 5:
	case 6: /* S2 + S3 + S4 */
		Output = m_OPN.Slot[SlotId + S2].Output[0] + m_OPN.Slot[SlotId + S3].Output[0] + m_OPN.Slot[SlotId + S4].Output[0];
		break;

	case 7: /* S1 + S2 + S3 + S4 */
		Output = m_OPN.Slot[SlotId + S1].Output[0] + m_OPN.Slot[SlotId + S2].Output[0] + m_OPN.Slot[SlotId + S3].Output[0] + m_OPN.Slot[SlotId + S4].Output[0];
		break;
	}

	/* Limit (14-bit) and mix channel output */
	m_OPN.Out += std::clamp<int16_t>(Output, -8192, 8191);
}

int16_t YM2203::GetModulation(uint32_t Cycle)
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

uint8_t YM2203::CalculateRate(uint8_t Rate, uint8_t KeyCode, uint8_t KeyScale)
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

void YM2203::ProcessKeyEvent(uint32_t SlotId)
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

void YM2203::StartEnvelope(uint32_t SlotId)
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

void YM2203::UpdateTimers()
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