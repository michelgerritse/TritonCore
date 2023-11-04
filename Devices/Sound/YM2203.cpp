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
#include "YM.h"

/*
	Yamaha YM2203 (OPN)
*/

#define VGM_WORKAROUND /* Workaround for VGM files */

/* Timer A overflow flag */
#define TIMER_A_OVERFLOW 0x01

/* Timer B overflow flag */
#define TIMER_B_OVERFLOW 0x02

/* Busy status flag */
#define BUSY_STATUS_FLAG 0x80

/* Slot naming */
enum SLOT_NAME
{
	S1 = 0, S2, S3, S4
};

/* Channel naming */
enum CHAN_NAME
{
	CH1 = 0, CH2, CH3
};

static const uint32_t SlotOrder[12] =
{
	 0 + S1,  4 + S1,  8 + S1, /* Channel 1 - 3: Slot 1 */
	 0 + S3,  4 + S3,  8 + S3, /* Channel 1 - 3: Slot 3 */
	 0 + S2,  4 + S2,  8 + S2, /* Channel 1 - 3: Slot 2 */
	 0 + S4,  4 + S4,  8 + S4  /* Channel 1 - 3: Slot 4 */
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
	m_CyclesToDoOPN = 0;
	m_CyclesToDoSSG = 0;

	/* Reset prescalers */
	//m_PreScalerOPN = 6;
	//m_PreScalerSSG = 4;

	/* Reset latches */
	m_AddressLatch = 0;
	m_FNumLatch = 0;
	m_3ChFNumLatch = 0;

	/* Reset status register */
	m_Status = 0;

	/* Reset envelope generator */
	m_EgCounter = 0;
	m_EgClock = 0;

	/* Reset timers */
	m_TimerA = 0;
	m_TimerB = 0;
	m_TimerAEnable = 0;
	m_TimerBEnable = 0;
	m_TimerALoad = 0;
	m_TimerBLoad = 0;
	m_TimerACount = 0;
	m_TimerBCount = 0;

	/* Reset 3CH / CSM mode */
	m_3ChMode = 0;
	m_CsmMode = 0;

	/* Clear 3CH registers */
	for (auto i = 0; i < 3; i++)
	{
		m_3ChFNum[i] = 0;
		m_3ChBlock[i] = 0;
		m_3ChKeyCode[i] = 0;
	}

	/* Clear slot registers */
	for (auto& Slot : m_Slot)
	{
		memset(&Slot, 0, sizeof(SLOT));

		/* Default register values */
		Slot.Multi = 1; /* x0.5 */

		/* Default envelope state */
		Slot.EgPhase = ADSR::Release;
		Slot.EgLevel = 0x3FF;
	}

	/* Clear channel registers  */
	for (auto& Channel : m_Channel)
	{
		memset(&Channel, 0, sizeof(CHANNEL));
	}

	/* Clear register array (initial state is 0 for all registers) */
	m_Register.fill(0);

	/* Reset SSG tone generators */
	for (auto& Tone : m_Tone)
	{
		memset(&Tone, 0, sizeof(AY::tone_t));
	}

	/* Reset SSG noise generator */
	memset(&m_Noise, 0, sizeof(AY::noise_t));
	m_Noise.LFSR = 1 << (17 - 1);

	/* Reset SSG envelope generator */
	m_Envelope.Counter = 0;
	m_Envelope.Period.u32 = 0;
	m_Envelope.Amplitude = AY::Amplitude32[31];
	m_Envelope.Step = 31;
	m_Envelope.StepDec = 1;
	m_Envelope.Hld = 1;
	m_Envelope.Alt = 31;
	m_Envelope.Inv = 0;
}

void YM2203::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
}

bool YM2203::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	switch (OutputNr)
	{
		case 0: /* SSG - Channel A */
			Desc.SampleRate = m_ClockSpeed / (8 * m_PreScalerSSG);
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"Channel A";
			break;

		case 1: /* SSG - Channel B */
			Desc.SampleRate = m_ClockSpeed / (8 * m_PreScalerSSG);
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"Channel B";
			break;

		case 2: /* SSG - Channel C */
			Desc.SampleRate = m_ClockSpeed / (8 * m_PreScalerSSG);
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"Channel C";
			break;

		case 3: /* FM */
			Desc.SampleRate = m_ClockSpeed / (12 * m_PreScalerOPN);
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"FM";
			break;

		default:
			return false;
	}

	return true;
}

void YM2203::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM2203::GetClockSpeed()
{
	return m_ClockSpeed;
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
		if (m_AddressLatch < 0x10) /* Write SSG data (0x00 - 0x10) */
		{
			WriteSSG(m_AddressLatch, Data);
		}
		else if (m_AddressLatch < 0x30) /* Write mode data (0x20 - 0x2F) */
		{
			WriteMode(m_AddressLatch, Data);
		}
		else /* Write FM data (0x30 - 0xB2) */
		{
			WriteFM(m_AddressLatch, Data);
		}
	}
}

void YM2203::WriteSSG(uint8_t Register, uint8_t Data)
{
	Register &= 0x0F;
	m_Register[Register] = Data;

	Data &= AY::Mask[Register]; /* Mask unused bits after storing */

	switch (Register)
	{
	case 0x00: /* Channel A Tone Period (Fine Tune) */
		m_Tone[0].Period.u8ll = Data;
		break;

	case 0x01: /* Channel A Tone Period (Coarse Tune) */
		m_Tone[0].Period.u8lh = Data;
		break;

	case 0x02: /* Channel B Tone Period (Fine Tune) */
		m_Tone[1].Period.u8ll = Data;
		break;

	case 0x03: /* Channel B Tone Period (Coarse Tune) */
		m_Tone[1].Period.u8lh = Data;
		break;

	case 0x04: /* Channel C Tone Period (Fine Tune) */
		m_Tone[2].Period.u8ll = Data;
		break;

	case 0x05: /* Channel C Tone Period (Coarse Tune) */
		m_Tone[2].Period.u8lh = Data;
		break;

	case 0x06: /* Noise Period */
		m_Noise.Period = Data;
		break;

	case 0x07: /* Mixer Control - I/O Enable */
		m_Tone[0].ToneDisable = (Data >> 0) & 1;
		m_Tone[1].ToneDisable = (Data >> 1) & 1;
		m_Tone[2].ToneDisable = (Data >> 2) & 1;

		m_Tone[0].NoiseDisable = (Data >> 3) & 1;
		m_Tone[1].NoiseDisable = (Data >> 4) & 1;
		m_Tone[2].NoiseDisable = (Data >> 5) & 1;
		break;

	case 0x08: /* Channel A Amplitude Control */
		m_Tone[0].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_Tone[0].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x09: /* Channel B Amplitude Control */
		m_Tone[1].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_Tone[1].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0A: /* Channel C Amplitude Control */
		m_Tone[2].Amplitude = AY::Amplitude32[AY::MapLvl4to5[Data & 0x0F]];
		m_Tone[2].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0B: /* Envelope Period (Fine Tune) */
		m_Envelope.Period.u8ll = Data;
		break;

	case 0x0C: /* Envelope Period (Coarse Tune) */
		m_Envelope.Period.u8lh = Data;
		break;

	case 0x0D: /* Envelope Shape / Cycle Control */
		m_Envelope.Counter = 0;
		m_Envelope.Step = 31;
		m_Envelope.StepDec = 1;

		/* If attacking, apply output inversion */
		m_Envelope.Inv = (Data & 0x04) ? 31 : 0;

		if (Data & 0x08) /* Continuous cycles */
		{
			m_Envelope.Hld = Data & 0x01;

			if (m_Envelope.Hld)
				m_Envelope.Alt = (Data & 0x02) ? 0 : 31;
			else
				m_Envelope.Alt = (Data & 0x02) ? 31 : 0;
		}
		else /* Single cycle */
		{
			m_Envelope.Hld = 1;
			m_Envelope.Alt = m_Envelope.Inv ^ 31;
		}

		/* Set initial ouput volume */
		m_Envelope.Amplitude = AY::Amplitude32[m_Envelope.Step ^ m_Envelope.Inv];
		break;

	case 0x0E: /* I/O Port A Data Store */
		/* Not implemented */
		break;

	case 0x0F: /* I/O Port B Data Store */
		/* Not implemented */
		break;
	}
}

void YM2203::WriteMode(uint8_t Register, uint8_t Data)
{
	switch (Register) /* 0x20 - 0x2F */
	{
	case 0x21: /* LSI Test */
		/* Not implemented */
		break;

	case 0x24: /* Timer A [9:2] */
		m_TimerA = (Data << 2) | (m_TimerA & 0x03);
		break;

	case 0x25: /* Timer A [1:0] */
		m_TimerA = (m_TimerA & 0x3FC) | (Data & 0x03);
		break;

	case 0x26: /* Timer B */
		m_TimerB = Data;
		break;

	case 0x27: /* 3CH mode / Timer control */
		/* Timer A start / stop */
		if (m_TimerALoad ^ (Data & 0x01))
		{
			m_TimerALoad = Data & 0x01;
			if (m_TimerALoad) m_TimerACount = 1024 - m_TimerA;
		}

		/* Timer B start / stop */
		if (m_TimerBLoad ^ (Data & 0x02))
		{
			m_TimerBLoad = Data & 0x02;
			if (m_TimerBLoad) m_TimerBCount = (256 - m_TimerB) << 4; /* Note: period x 16 to allign with Timer A */
		}

		/* Timer A/B enable */
		m_TimerAEnable = (Data >> 2) & 0x01;
		m_TimerBEnable = (Data >> 3) & 0x01;

		/* Timer A/B overflow flag reset */
		if (Data & 0x10) m_Status &= ~TIMER_A_OVERFLOW;
		if (Data & 0x20) m_Status &= ~TIMER_B_OVERFLOW;

		/* 3CH / CSM mode */
		m_3ChMode = ((Data & 0xC0) != 0x00) ? 1 : 0;
		m_CsmMode = ((Data & 0xC0) == 0x80) ? 1 : 0;

		assert(m_CsmMode == 0);
		break;

	case 0x28: /* Key On/Off */
	{
		if ((Data & 0x03) == 0x03) break; /* Invalid channel */

		uint32_t ChannelId = (Data & 0x03) << 2;

		m_Slot[ChannelId + S1].KeyLatch = (Data & 0x10) >> 4;
		m_Slot[ChannelId + S2].KeyLatch = (Data & 0x20) >> 5;
		m_Slot[ChannelId + S3].KeyLatch = (Data & 0x40) >> 6;
		m_Slot[ChannelId + S4].KeyLatch = (Data & 0x80) >> 7;

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

	case 0x2D: /* Prescaler selection */
		/*m_PreScalerOPN = 6;
		m_PreScalerSSG = 4;*/
		break;

	case 0x2E: /* Prescaler selection */
		/*if (m_PreScalerOPN == 6)
		{
			m_PreScalerOPN = 3;
			m_PreScalerSSG = 2;
		}*/
		break;

	case 0x2F: /* Prescaler selection */
		/*m_PreScalerOPN = 2;
		m_PreScalerSSG = 1;*/
		break;
	}
}

void YM2203::WriteFM(uint8_t Register, uint8_t Data)
{
	/* Slot address mapping: S1 - S3 - S2 - S4 */
	static const int32_t SlotMap[16] =
	{
		0, 4, 8, -1, 2, 6, 10, -1, 1, 5, 9, -1, 3, 7, 11, -1
	};

	int32_t SlotId = SlotMap[Register & 0x0F];
	if (SlotId == -1) return;

	if (Register < 0xA0) /* Slot register map (0x30 - 0x9F) */
	{
		auto& Slot = m_Slot[SlotId];

		switch (Register & 0xF0)
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
			Slot.SsgEnable= (Data >> 3) & 0x01;
			Slot.SsgEgInv = (Data >> 2) & 0x01;
			Slot.SsgEgAlt = (Data >> 1) & 0x01;
			Slot.SsgEgHld = (Data >> 0) & 0x01;

			assert(Slot.SsgEnable == 0);
			break;
		}
	}
	else /* Channel register map (0xA0 - 0xB2) */
	{
		auto& Chan = m_Channel[SlotId >> 2];

		switch (Register & 0xFC)
		{
		case 0xA0: /* F-Num 1 */
			Chan.FNum = ((m_FNumLatch & 0x07) << 8) | Data;
			Chan.Block = m_FNumLatch >> 3;
			Chan.KeyCode = (Chan.Block << 2) | YM::OPN::Note[Chan.FNum >> 7];
			break;

		case 0xA4: /* F-Num 2 / Block Latch */
			m_FNumLatch = Data & 0x3F;
			break;

		case 0xA8: /* 3 Ch-3 F-Num  */
			/* Slot order for 3CH mode */
			if (Register == 0xA9)
			{
				m_3ChFNum[S1] = ((m_3ChFNumLatch & 0x07) << 8) | Data;
				m_3ChBlock[S1] = m_3ChFNumLatch >> 3;
				m_3ChKeyCode[S1] = (m_3ChBlock[S1] << 2) | YM::OPN::Note[m_3ChFNum[S1] >> 7];
			}
			else if (Register == 0xA8)
			{
				m_3ChFNum[S3] = ((m_3ChFNumLatch & 0x07) << 8) | Data;
				m_3ChBlock[S3] = m_3ChFNumLatch >> 3;
				m_3ChKeyCode[S3] = (m_3ChBlock[S3] << 2) | YM::OPN::Note[m_3ChFNum[S3] >> 7];
			}
			else /* 0xAA */
			{
				m_3ChFNum[S2] = ((m_3ChFNumLatch & 0x07) << 8) | Data;
				m_3ChBlock[S2] = m_3ChFNumLatch >> 3;
				m_3ChKeyCode[S2] = (m_3ChBlock[S2] << 2) | YM::OPN::Note[m_3ChFNum[S2] >> 7];
			}
			break;

		case 0xAC: /* 3 Ch-3 F-Num / Block Latch */
			m_3ChFNumLatch = Data & 0x3F;
			break;

		case 0xB0: /* Feedback / Connection */
			Chan.FB = (Data >> 3) & 0x07;
			Chan.Algo = Data & 0x07;
			break;
		}
	}
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
		if ((m_Envelope.Counter += 2) >= m_Envelope.Period.u32) //FIXME: should be += 1
		{
			/* Reset counter */
			m_Envelope.Counter = 0;

			/* Count down step counter (31 -> 0) */
			m_Envelope.Step -= m_Envelope.StepDec;

			if (m_Envelope.Step & 32) /* Envelope cycle completed */
			{
				/* Restart cycle */
				m_Envelope.Step = 31;

				/* Stop counting (if needed) */
				m_Envelope.StepDec = m_Envelope.Hld ^ 1;

				/* Toggle output inversion */
				m_Envelope.Inv ^= m_Envelope.Alt;
			}

			/* Apply output inversion and lookup amplitude */
			m_Envelope.Amplitude = AY::Amplitude32[m_Envelope.Step ^ m_Envelope.Inv];
		}

		/* Update noise generator */
		if (m_Noise.Prescaler ^= 1)
		{
			if ((m_Noise.Counter += 2) >= m_Noise.Period) //FIXME: should be += 1
			{
				/* Reset counter */
				m_Noise.Counter = 0;

				/* Update output flag */
				m_Noise.Output = m_Noise.LFSR & 1;

				/* Tap bits 3 and 0 (XOR feedback) */
				uint32_t Seed = ((m_Noise.LFSR >> 3) ^ (m_Noise.LFSR >> 0)) & 1;

				/* Shift LFSR and apply seed (17-bit wide) */
				m_Noise.LFSR = (m_Noise.LFSR >> 1) | (Seed << 16);
			}
		}

		/* Update, mix and output tone generators */
		for (auto i = 0; i < 3; i++)
		{
			auto& Tone = m_Tone[i];

			if ((Tone.Counter += 2) >= Tone.Period.u32) //FIXME: should be += 1
			{
				/* Reset counter */
				Tone.Counter = 0;

				/* Toggle output flag */
				Tone.Output ^= 1;
			}

			/* Mix tone and noise (implemented as a mask) */
			Mask = ~(((Tone.Output | Tone.ToneDisable) & (m_Noise.Output | Tone.NoiseDisable)) - 1);

			/* Amplitude control */
			Out = Tone.AmpCtrl ? m_Envelope.Amplitude : Tone.Amplitude;

			/* 16-bit output */
			OutBuffer[i]->WriteSampleS16(Out & Mask);
		}
	}
}

void YM2203::UpdateOPN(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDoOPN;
	uint32_t Samples = TotalCycles / (12 * m_PreScalerOPN);
	m_CyclesToDoOPN = TotalCycles % (12 * m_PreScalerOPN);

	int16_t Out;

	while (Samples-- != 0)
	{
		Out = 0;

		/* Update Timer A and Timer B */
		UpdateTimers();

		/* Update Envelope Generator clock */
		m_EgClock = (m_EgClock + 1) % 3;
		m_EgCounter += (m_EgClock >> 1);
		m_EgCounter &= 0xFFF;

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

		Out += m_Channel[CH1].Output;
		Out += m_Channel[CH2].Output;
		Out += m_Channel[CH3].Output;

		/* 16-bit DAC output */
		OutBuffer[3]->WriteSampleS16(Out);
	}
}

void YM2203::PrepareSlot(uint32_t SlotId)
{
	uint32_t ChannelId = SlotId >> 2;
	auto& Chan = m_Channel[ChannelId];
	auto& Slot = m_Slot[SlotId];

	/* Copy some values for later processing */
	Slot.FNum = Chan.FNum;
	Slot.Block = Chan.Block;
	Slot.KeyCode = Chan.KeyCode;

	if (m_3ChMode)
	{
		auto i = SlotId & 3;

		/* Get Block/FNum for channel 3: S1-S2-S3 */
		if ((ChannelId == CH3) && (i != S4))
		{
			Slot.FNum = m_3ChFNum[i];
			Slot.Block = m_3ChBlock[i];
			Slot.KeyCode = m_3ChKeyCode[i];
		}
	}
}

void YM2203::UpdatePhaseGenerator(uint32_t SlotId)
{
	uint32_t ChannelId = SlotId >> 2;
	auto& Chan = m_Channel[ChannelId];
	auto& Slot = m_Slot[SlotId];

	/*	Block shift (17-bit result) */
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
	auto& Chan = m_Channel[ChannelId];
	auto& Slot = m_Slot[SlotId];

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
	if (m_EgClock == ChannelId)
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

		if ((m_EgCounter & Mask) == 0) /* Counter overflowed */
		{
			uint16_t Level = Slot.EgLevel;

			/* Get update cycle (8 cycles in total) */
			uint32_t Cycle = (m_EgCounter >> Shift) & 0x07;

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
	auto& Slot = m_Slot[SlotId];

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

void YM2203::UpdateAccumulator(uint32_t ChannelId)
{
	int16_t Output = 0;
	uint32_t SlotId = ChannelId << 2;

	/* Accumulate output */
	switch (m_Channel[ChannelId].Algo)
	{
	case 0:
	case 1:
	case 2:
	case 3: /* S4 */
		Output = m_Slot[SlotId + S4].Output[0];
		break;

	case 4: /* S2 + S4 */
		Output = m_Slot[SlotId + S2].Output[0] + m_Slot[SlotId + S4].Output[0];
		break;

	case 5:
	case 6: /* S2 + S3 + S4 */
		Output = m_Slot[SlotId + S2].Output[0] + m_Slot[SlotId + S3].Output[0] + m_Slot[SlotId + S4].Output[0];
		break;

	case 7: /* S1 + S2 + S3 + S4 */
		Output = m_Slot[SlotId + S1].Output[0] + m_Slot[SlotId + S2].Output[0] + m_Slot[SlotId + S3].Output[0] + m_Slot[SlotId + S4].Output[0];
		break;
	}

	/* Limiter (signed 14-bit) */
	m_Channel[ChannelId].Output = std::clamp<int16_t>(Output, -8192, 8191);
}

int16_t YM2203::GetModulation(uint32_t Cycle)
{
	auto& Chan = m_Channel[Cycle >> 2];

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
			return (m_Slot[Cycle].Output[0] + m_Slot[Cycle].Output[1]) >> (10 - Chan.FB);
		else
			return 0;

	case 0x01: /* Algo: 0 - S2 */
		return m_Slot[ChanId + S1].Output[0] >> 1;

	case 0x02: /* Algo: 0 - S3 */
		return m_Slot[ChanId + S2].Output[0] >> 1;

	case 0x03: /* Algo: 0 - S4 */
		return m_Slot[ChanId + S3].Output[0] >> 1;

	case 0x05: /* Algo: 1 - S2 */
		return 0;

	case 0x06: /* Algo: 1 - S3 */
		return (m_Slot[ChanId + S1].Output[1] + m_Slot[ChanId + S2].Output[0]) >> 1;

	case 0x07: /* Algo: 1 - S4 */
		return m_Slot[ChanId + S3].Output[0] >> 1;

	case 0x09: /* Algo: 2 - S2 */
		return 0;

	case 0x0A: /* Algo: 2 - S3 */
		return m_Slot[ChanId + S2].Output[0] >> 1;

	case 0x0B: /* Algo: 2 - S4 */
		return (m_Slot[ChanId + S1].Output[0] + m_Slot[ChanId + S3].Output[0]) >> 1;

	case 0x0D: /* Algo: 3 - S2 */
		return m_Slot[ChanId + S1].Output[0] >> 1;

	case 0x0E: /* Algo: 3 - S3 */
		return 0;

	case 0x0F: /* Algo: 3 - S4 */
		return (m_Slot[ChanId + S2].Output[1] + m_Slot[ChanId + S3].Output[0]) >> 1;

	case 0x11: /* Algo: 4 - S2 */
		return m_Slot[ChanId + S1].Output[0] >> 1;

	case 0x12: /* Algo: 4 - S3 */
		return 0;

	case 0x13: /* Algo: 4 - S4 */
		return m_Slot[ChanId + S3].Output[0] >> 1;

	case 0x15: /* Algo: 5 - S2 */
		return m_Slot[ChanId + S1].Output[0] >> 1;

	case 0x16: /* Algo: 5 - S3 */
		return m_Slot[ChanId + S1].Output[1] >> 1;

	case 0x17: /* Algo: 5 - S4 */
		return m_Slot[ChanId + S1].Output[0] >> 1;

	case 0x19: /* Algo: 6 - S2 */
		return m_Slot[ChanId + S1].Output[0] >> 1;

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
	auto& Slot = m_Slot[SlotId];

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
	auto& Slot = m_Slot[SlotId];

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
	if (m_TimerALoad)
	{
		if (--m_TimerACount == 0)
		{
			m_TimerACount = 1024 - m_TimerA;

			/* Overflow flag enabled */
			if (m_TimerAEnable) m_Status |= TIMER_A_OVERFLOW;

			/* CSM Key On */
			if (m_CsmMode)
			{
				/* CSM Key-On all channel 3 slots */
				m_Slot[8 + S1].CsmKeyLatch = 1;
				m_Slot[8 + S2].CsmKeyLatch = 1;
				m_Slot[8 + S3].CsmKeyLatch = 1;
				m_Slot[8 + S4].CsmKeyLatch = 1;
			}
		}
	}

	if (m_TimerBLoad)
	{
		if (--m_TimerBCount == 0)
		{
			m_TimerBCount = (256 - m_TimerB) << 4;

			/* Overflow flag enabled */
			if (m_TimerBEnable) m_Status |= TIMER_B_OVERFLOW;
		}
	}
}