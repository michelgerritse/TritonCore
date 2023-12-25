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
#include "YM3526.h"

/*
	Yamaha YM3526 (OPL)

	Notes:
	- The EG-TYPE bit (B5 : 0x20-0x35) is wrongly documented. 0 = percussive sound, 1 = non-percussive sound
	  Note: The YM2413 manual has it right.
*/

#define FLAG_TIMER2	0x20 /* Timer 2 overflow */
#define FLAG_TIMER1	0x40 /* Timer 1 overflow */
#define FLAG_IRQ	0x80 /* Interrupt request */

/* Audio output enumeration */
enum AudioOut
{
	OPL = 0
};

/* Slot naming */
enum SlotName
{
	S1 = 0, /* Carrier */
	S2		/* Modulator */
};

/* Channel naming */
enum ChannelName
{
	CH1 = 0, CH2, CH3, CH4, CH5, CH6, CH7, CH8, CH9
};

static const uint32_t SlotToChannel[18] =
{
	CH1, CH2, CH3, CH1, CH2, CH3,
	CH4, CH5, CH6, CH4, CH5, CH6,
	CH7, CH8, CH9, CH7, CH8, CH9
};

/* Envelope phases */
enum ADSR : uint32_t
{
	Attack = 0,
	Decay,
	Sustain,
	Release
};

YM3526::YM3526(uint32_t ClockSpeed):
	m_ClockSpeed(ClockSpeed)
{
	/* Build OPL tables */
	YM::OPL::BuildTables();

	/* Reset device */
	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YM3526::GetDeviceName()
{
	return L"Yamaha YM3526";
}

void YM3526::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Reset latches */
	m_AddressLatch = 0;

	/* Reset OPL unit */
	memset(&m_OPL, 0, sizeof(m_OPL));

	m_OPL.LfoAmShift = 4; /* 1.0dB */
	m_OPL.LfoPmShift = 1; /* 7 cents */

	/* Default operator register state */
	for (auto& Slot : m_OPL.Slot)
	{
		Slot.Multi   = YM::OPL::Multiply[0]; /* x0.5 */
		
		Slot.EgPhase = ADSR::Release;
		Slot.EgLevel = YM::OPL::MaxAttenuation;
		Slot.EgType  = 1; /* non-percussive sound */
		
		Slot.KeyScaling = 2;
		Slot.KeyScaleShift = YM::OPL::KeyScaleShift[0];

		Slot.WaveTable = &YM::OPL::WaveTable[0][0];
	}
}

void YM3526::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	Write(0x00, Command & 0xFF);
	Write(0x01, Value);
}

bool YM3526::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == AudioOut::OPL)
	{
		Desc.SampleRate = m_ClockSpeed / (4 * 18);
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"FM";
		return true;
	}

	return false;
}

void YM3526::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM3526::GetClockSpeed()
{
	return m_ClockSpeed;
}

uint32_t YM3526::Read(uint32_t Address)
{
	if ((Address & 0x01) == 0)
	{
		return m_OPL.Status;
	}

	return 0;
}

void YM3526::Write(uint32_t Address, uint32_t Data)
{
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;

	/* 1-bit address bus (A0) */
	Address &= 0x01;

	if (Address == 0) /* Address write mode */
	{
		m_AddressLatch = Data;
	}
	else /* Data write mode */
	{
		WriteRegister(m_AddressLatch, Data);
	}
}

void YM3526::WriteRegister(uint8_t Address, uint8_t Data)
{
	/* Address to slot mapping: */
	static const int32_t SlotMap[32] =
	{
		 0,  1,  2,  3,  4,  5, -1, -1,
		 6,  7,  8,  9, 10, 11, -1, -1,
		12, 13, 14, 15, 16, 17, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1
	};

	switch (Address & 0xF0)
	{
	case 0x00: /* Mode data */
		switch (Address & 0xF)
		{
		case 0x01: /* LSI test */
			break;

		case 0x02: /* Timer 1 */
			m_OPL.Timer1.Period = Data;
			break;

		case 0x03: /* Timer 2 */
			m_OPL.Timer2.Period = Data;
			break;

		case 0x04: /* IRQ reset / Timer control */
		{
			if (Data & 0x80)
			{
				m_OPL.Status = 0; //TODO: Clear interrupt line
				return;
			}

			m_OPL.Timer1.Mask = (Data >> 6) & 0x01;
			m_OPL.Timer2.Mask = (Data >> 5) & 0x01;

			/* Timer 1 and 2 start / stop */
			auto ST1 = (Data >> 0) & 0x01;
			auto ST2 = (Data >> 1) & 0x01;

			if (m_OPL.Timer1.Start ^ ST1)
			{
				m_OPL.Timer1.Start = ST1;
				m_OPL.Timer1.Counter = 256 - m_OPL.Timer1.Period;
			}

			if (m_OPL.Timer2.Start ^ ST2)
			{
				m_OPL.Timer2.Start = ST2;
				m_OPL.Timer2.Counter = 256 - m_OPL.Timer2.Period;
			}
			break;
		}

		case 0x08: /* CSM mode / Note select */
			m_OPL.CSM = (Data >> 7) & 0x01;
			m_OPL.NTS = (Data >> 6) & 0x01;
			break;

		default: /* Not used */
			assert(false);
			break;
		}
		break;

	case 0x20:
	case 0x30: /* AM / PM / EG-Type / KSR / Multiply */
	{
		int32_t SlotId = SlotMap[Address & 0x1F]; if (SlotId == -1) return;
		auto& Slot = m_OPL.Slot[SlotId];

		Slot.LfoAmOn = (Data & 0x80) ? ~0 : 0; /* Tremolo on / off mask */
		Slot.LfoPmOn = (Data & 0x40) ? ~0 : 0; /* Vibrato on / off mask */

		Slot.EgType = (Data & 0x20) ? 0 : 1;
		Slot.KeyScaling = (Data & 0x10) ? 0 : 2;

		Slot.Multi = YM::OPL::Multiply[Data & 0x0F];

		break;
	}

	case 0x40:
	case 0x50: /* KSL / Total level */
	{
		int32_t SlotId = SlotMap[Address & 0x1F]; if (SlotId == -1) return;
		auto& Slot = m_OPL.Slot[SlotId];

		Slot.KeyScaleShift = YM::OPL::KeyScaleShift[(Data >> 6) & 0x03];
		Slot.TotalLevel = (Data >> 0) & 0x3F;
		break;
	}

	case 0x60:
	case 0x70: /* AR / DR */
	{
		int32_t SlotId = SlotMap[Address & 0x1F]; if (SlotId == -1) return;
		auto& Slot = m_OPL.Slot[SlotId];

		Slot.EgRate[ADSR::Attack] = (Data >> 4) & 0x0F;
		Slot.EgRate[ADSR::Decay] = (Data >> 0) & 0x0F;
		break;
	}

	case 0x80:
	case 0x90: /* SL / RR */
	{
		int32_t SlotId = SlotMap[Address & 0x1F]; if (SlotId == -1) return;
		auto& Slot = m_OPL.Slot[SlotId];

		Slot.SustainLvl = (Data >> 4) & 0x0F;
		Slot.EgRate[ADSR::Release] = (Data >> 0) & 0x0F;

		/* If all SL bits are set, SL is -93dB. See OPL4 manual page 47 */
		Slot.SustainLvl |= (Slot.SustainLvl + 1) & 0x10;
		break;
	}

	case 0xA0: /* F-Number (L) */
	{
		auto& Channel = m_OPL.Channel[(Address & 0x0F) % 9]; /* Keeps the compiler happy */

		Channel.FNum &= 0x300;
		Channel.FNum |= Data;
		break;
	}

	case 0xB0: /* Key On / Block / F-Number (H) */
	{
		if (Address == 0xBD) /* AM, PM depth / Rhythm */
		{
			m_OPL.LfoAmShift = (Data & 0x80) ? 2 : 4; /* Depth = 4.8 or 1.0dB */
			m_OPL.LfoPmShift = (Data & 0x40) ? 0 : 1; /* Depth = 7 or 14 cents */
			m_OPL.RHY = (Data >> 5) & 0x01;

			//if (m_OPL.RHY) /* Key on drum instruments */
			//{
			//	if (Data & 0x10); /* BD */
			//	if (Data & 0x08); /* SD */
			//	if (Data & 0x04); /* TOM */
			//	if (Data & 0x02); /* TC */
			//	if (Data & 0x01); /* HH */
		}
		else
		{
			auto& Channel = m_OPL.Channel[(Address & 0x0F) % 9]; /* Keeps the compiler happy */

			Channel.KeyLatch = (Data >> 5) & 0x01;
			Channel.Block = (Data >> 2) & 0x07;

			Channel.FNum &= 0x0FF;
			Channel.FNum |= (Data & 0x03) << 8;

			/* Calculate keycode */
			Channel.KeyCode = (Channel.Block << 1);
			Channel.KeyCode |= (Channel.FNum >> (9 - m_OPL.NTS)) & 0x01; /* Select FNUM b9 or b8 */
		}
		break;
	}

	case 0xC0: /* Feedback / Connection */
	{
		auto& Channel = m_OPL.Channel[(Address & 0x0F) % 9]; /* Keeps the compiler happy */

		Channel.FB = (Data >> 1) & 0x07;
		Channel.Algo = (Data >> 0) & 0x01;
		break;
	}

	default: /* Not used */
		assert(false);
		break;
	}
}

void YM3526::UpdateTimers()
{
	/* Update global timer (13-bit) */
	m_OPL.Timer = (m_OPL.Timer + 1) & 0x1FFF;
	
	/* Update LFO-AM (tremolo) */
	if ((m_OPL.Timer & YM::OPL::LfoAmPeriod) == 0)
	{
		m_OPL.LfoAmStep = (m_OPL.LfoAmStep + 1) % YM::OPL::LfoAmSteps;

		if (m_OPL.LfoAmStep < (YM::OPL::LfoAmSteps / 2)) /* Increase */
		{
			m_OPL.LfoAmLevel = m_OPL.LfoAmStep >> m_OPL.LfoAmShift;
		}
		else /* Decrease */
		{
			m_OPL.LfoAmLevel = (YM::OPL::LfoAmSteps - m_OPL.LfoAmStep) >> m_OPL.LfoAmShift;
		}
	}

	/* Update LFO-PM (vibrato) */
	if ((m_OPL.Timer & YM::OPL::LfoPmPeriod) == 0)
	{
		m_OPL.LfoPmStep = (m_OPL.LfoPmStep + 1) & YM::OPL::LfoPmSteps;
	}

	/* Update timer 1 */
	if (m_OPL.Timer1.Start)
	{
		if ((m_OPL.Timer & YM::OPL::Timer1Mask) == 0)
		{
			if (--m_OPL.Timer1.Counter == 0)
			{
				m_OPL.Timer1.Counter = 256 - m_OPL.Timer1.Period;

				/* Overflow flag enabled */
				if (m_OPL.Timer1.Mask == 0) m_OPL.Status |= (FLAG_IRQ | FLAG_TIMER1); //TODO: Set interrupt line

				/* CSM Key On */
				if (m_OPL.CSM)
				{
					for (auto& Slot : m_OPL.Slot) Slot.KeyLatch |= 1;
				}
			}
		}
	}

	/* Update timer 2 */
	if (m_OPL.Timer2.Start)
	{
		if ((m_OPL.Timer & YM::OPL::Timer2Mask) == 0)
		{
			if (--m_OPL.Timer2.Counter == 0)
			{
				m_OPL.Timer2.Counter = 256 - m_OPL.Timer2.Period;

				/* Overflow flag enabled */
				if (m_OPL.Timer1.Mask == 0) m_OPL.Status |= (FLAG_IRQ | FLAG_TIMER2); //TODO: Set interrupt line
			}
		}
	}
}

void YM3526::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	static const uint32_t SlotOrder[] =
	{
		0,  1,  2,  3,  4,  5,  6,  7,  8,
		9, 10, 11, 12, 13, 14, 15, 16, 17
	};

	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / (18 * 4);
	m_CyclesToDo = TotalCycles % (18 * 4);

	while (Samples-- != 0)
	{
		UpdateTimers();
		ClearAccumulator();

		/* Update slots (operators) */
		for (auto& SlotId : SlotOrder)
		{
			UpdatePhaseGenerator(SlotId);
			UpdateEnvelopeGenerator(SlotId);
			UpdateOperatorUnit(SlotId);
		}

		UpdateAccumulator(CH1);
		UpdateAccumulator(CH2);
		UpdateAccumulator(CH3);
		UpdateAccumulator(CH4);
		UpdateAccumulator(CH5);
		UpdateAccumulator(CH6);

		if (m_OPL.RHY == 0)
		{
			UpdateAccumulator(CH7);
			UpdateAccumulator(CH8);
			UpdateAccumulator(CH9);
		}

		/* Limiter (signed 16-bit) */
		int16_t Out = std::clamp(m_OPL.Out, -32768, 32767);

		/* 16-bit output */
		OutBuffer[AudioOut::OPL]->WriteSampleS16(Out);
	}
}

void YM3526::UpdatePhaseGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPL.Channel[SlotToChannel[SlotId]];
	auto& Slot = m_OPL.Slot[SlotId];

	uint32_t FNum = Chan.FNum;

	/* Apply LFO-PM (vibrato) */
	if (Slot.LfoPmOn != 0)
	{
		/* LFO-PM shape (8 steps):
		
		      2
		     / \
		    1   3
		   /     \  
		--0-------4-------0--
		           \     /
		            5   7
				     \ /
				      6
		*/        
		
		uint32_t Inc = FNum >> 7;

		switch (m_OPL.LfoPmStep)
		{
		case 0:
		case 4: /* Center */
			break;

		case 1:
		case 3: /* Halfway - positive */
			FNum += (Inc >> (1 + m_OPL.LfoPmShift));
			break;

		case 2: /* Top - positive */
			FNum += (Inc >> m_OPL.LfoPmShift);
			break;

		case 5:
		case 7: /* Halfway - negative */
			FNum -= (Inc >> (1 + m_OPL.LfoPmShift));
			break;

		case 6: /* Top - negative */
			FNum -= (Inc >> m_OPL.LfoPmShift);
		}
	}

	/* Block shift (16-bit) */
	uint32_t Inc = (FNum << Chan.Block) >> 1;

	/* Multiply (19-bit) */
	Inc = (Inc * Slot.Multi) >> 1;

	/* Update phase counter (19-bit: 10.9) */
	Slot.PgPhase += Inc;
}

void YM3526::UpdateEnvelopeGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPL.Channel[SlotToChannel[SlotId]];
	auto& Slot = m_OPL.Slot[SlotId];

	/*-------------------------------------*/
	/* Step 1: Key On / Off event handling */
	/*-------------------------------------*/
	uint32_t NewKeyState = (Chan.KeyLatch | Slot.KeyLatch);
	uint32_t EnvelopeStart = 0;
	uint32_t EnvelopeRun = 1;

	/* Clear CSM / Drum key on flag */
	Slot.KeyLatch = 0;

	switch ((NewKeyState << 1) | Slot.KeyState)
	{
	case 0x00:
	case 0x03: /* Ignore */
		break;

	case 0x01: /* Key Off */
		Slot.EgPhase  = ADSR::Release;
		Slot.KeyState = 0;
		break;

	case 0x02: /* Key On */
		Slot.EgPhase  = ADSR::Attack;
		Slot.PgPhase  = 0;
		Slot.KeyState = 1;
		EnvelopeStart = 1;
		break;
	}

	/*-------------------------------*/
	/* Step 2: Envelope update cycle */
	/*-------------------------------*/
	uint32_t Rate = 0;

	switch (Slot.EgPhase)
	{
		case ADSR::Attack:
			Rate = Slot.EgRate[ADSR::Attack];
			break;

		case ADSR::Decay:
			EnvelopeRun = (Slot.EgLevel & 0x1F8) ^ 0x1F8;
			Rate = Slot.EgRate[ADSR::Decay];
			break;

		case ADSR::Sustain:
			EnvelopeRun = (Slot.EgLevel & 0x1F8) ^ 0x1F8;

			/* Note: EG-Type selects sustain or release */
			Rate = Slot.EgRate[ADSR::Sustain + Slot.EgType];
			break;

		case ADSR::Release:
			EnvelopeRun = (Slot.EgLevel & 0x1F8) ^ 0x1F8;
			Rate = Slot.EgRate[ADSR::Release];
			break;
	}

	if (EnvelopeRun == 0) Slot.EgLevel = YM::OPL::MaxAttenuation;

	if ((Rate | EnvelopeRun) != 0)
	{
		/* Calculate scaled rate: (4 * rate) + scale value */
		uint32_t ScaledRate = std::min((Rate << 2) + (Chan.KeyCode >> Slot.KeyScaling), 63u);

		/* Get timer resolution */
		uint32_t Shift = YM::OPL::EgShift[ScaledRate];
		uint32_t Mask = (1 << Shift) - 1;

		if ((m_OPL.Timer & Mask) == 0) /* Timer expired */
		{
			uint16_t Level = Slot.EgLevel;
			
			/* Get update cycle (8 cycles in total) */
			uint32_t Cycle = (m_OPL.Timer >> Shift) & 0x07;

			/* Lookup attenuation adjustment */
			uint32_t AttnInc = YM::OPL::EgLevelAdjust[ScaledRate][Cycle];

			switch (Slot.EgPhase)
			{
			case ADSR::Attack:
				if (ScaledRate >= 60)
				{
					/* Instant attack */
					if (EnvelopeStart) Level = 0;
				}
				else
				{
					Level += ((~Level * AttnInc) >> 4);
				}

				if (Level == 0)
				{
					Slot.EgPhase = (Slot.SustainLvl != 0) ? ADSR::Decay : ADSR::Sustain;
				}
				break;

			case ADSR::Decay:
				Level += AttnInc;

				if (((uint32_t) Level >> 4) == Slot.SustainLvl) Slot.EgPhase = ADSR::Sustain;
				break;

			case ADSR::Sustain:
			case ADSR::Release:
				Level += AttnInc;
				break;
			}

			Slot.EgLevel = Level;
		}
	}

	/*-------------------------------------*/
	/* Step 3: Envelope output calculation */
	/*-------------------------------------*/
	uint32_t Attn = Slot.EgLevel + (Slot.TotalLevel << 2);

	/* Apply key scale level */
	Attn += YM::OPL::KeyScaleLevel[Chan.FNum >> 6][Chan.Block] >> Slot.KeyScaleShift; //TODO: Pre-calculate KSL

	/* Apply LFO-AM (tremolo) */
	Attn += m_OPL.LfoAmLevel & Slot.LfoAmOn;

	/* Limit and shift from 4.5 to 4.8 */
	Slot.EgOutput = std::min(Attn, YM::OPL::MaxAttenuation) << 3;
}

void YM3526::UpdateOperatorUnit(uint32_t SlotId)
{
	auto& Slot = m_OPL.Slot[SlotId];

	/* Phase modulation (10-bit) */
	uint32_t Phase = (Slot.PgPhase >> 9) + GetModulation(SlotId);

	/* Attenuation (4.8 + 4.8 = 5.8 fixed point) */
	uint32_t Level = Slot.WaveTable[Phase & 0x1FF] + Slot.EgOutput;

	/* dB to linear conversion (12-bit) */
	int16_t Output = YM::OPL::ExpTable[Level & 0xFF] >> (Level >> 8);

	/* Inverse output (13-bit) */
	if (Phase & 0x200) Output = ~Output; /* Don't negate !*/

	/* The last 2 generated samples are stored */
	Slot.Output[1] = Slot.Output[0];
	Slot.Output[0] = Output;
}

void YM3526::ClearAccumulator()
{
	m_OPL.Out = 0;
}

void YM3526::UpdateAccumulator(uint32_t ChannelId)
{
	static const uint32_t Operator1[9] =
	{
		0, 1, 2, 6, 7, 8, 12, 13, 14
	};

	static const uint32_t Operator2[9] =
	{
		3, 4, 5, 9, 10, 11, 15, 16, 17
	};
	
	int16_t Output = 0;

	uint32_t SlotId1 = Operator1[ChannelId];
	uint32_t SlotId2 = Operator2[ChannelId];

	if (m_OPL.Channel[ChannelId].Algo == 0)
	{
		Output = m_OPL.Slot[SlotId2].Output[0];
	}
	else
	{
		Output = m_OPL.Slot[SlotId1].Output[0] + m_OPL.Slot[SlotId2].Output[0];
	}

	/* Limit (13-bit) and mix channel output */
	m_OPL.Out += std::clamp<int16_t>(Output, -4096, 4095);
}

int16_t YM3526::GetModulation(uint32_t SlotId)
{
	static const uint32_t IsCarrier[18] =
	{
		0, 0, 0, 1, 1, 1,
		0, 0, 0, 1, 1, 1,
		0, 0, 0, 1, 1, 1,
	};
	
	uint32_t ChannelId = SlotToChannel[SlotId];
	auto& Chan = m_OPL.Channel[ChannelId];

	switch ((Chan.Algo << 1) | IsCarrier[SlotId])
	{
	case 0x00: /* Algo: 0 - Modulator */
	case 0x02: /* Algo: 1 - Modulator */
		if (Chan.FB) /* Slot 1 self-feedback modulation (10-bit) */
			return (m_OPL.Slot[SlotId].Output[0] + m_OPL.Slot[SlotId].Output[1]) >> (9 - Chan.FB);
		else
			return 0;

	case 0x01: /* Algo: 0 - Carrier */
		return m_OPL.Slot[SlotId - 3].Output[0];

	case 0x03: /* Algo: 1 - Carrier */
		return 0;
	}

	return 0;
}