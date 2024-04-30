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
*/

#define FLAG_TIMER2	0x20 /* Timer 2 overflow */
#define FLAG_TIMER1	0x40 /* Timer 1 overflow */
#define FLAG_IRQ	0x80 /* Interrupt request */

/* Audio output enumeration */
enum AudioOut
{
	Default = 0
};

/* Slot naming */
enum SlotName
{
	S1 = 0,	/* Modulator */
	S2		/* Carrier   */
};

/* Channel naming */
enum ChannelName
{
	CH1 = 0, CH2, CH3, CH4, CH5, CH6, CH7, CH8, CH9
};

/* Envelope phases */
enum ADSR : uint32_t
{
	Attack = 0,
	Decay,
	Sustain,
	Release
};

/* Drum instruments */
enum Rhythm : uint32_t
{
	BD1 = 12,	/* Bass drum 1 (CH7 - S1) */
	BD2 = 13,	/* Bass drum 2 (CH7 - S2) */
	HH = 14,	/* High hat    (CH8 - S1) */
	SD = 15,	/* Snare drum  (CH8 - S2) */
	TOM = 16,	/* Tom tom     (CH9 - S1) */
	TC = 17		/* Top cymbal  (CH9 - S2) */
};

/* Static class member initialization */
const std::wstring YM3526::s_DeviceName = L"Yamaha YM3526";

YM3526::YM3526(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(4 * 18)
{
	/* Create DAC */
	m_DAC = std::make_unique<YM3014>(5.0f);
	
	/* Build OPL tables */
	YM::OPL::BuildTables();

	/* Reset device */
	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YM3526::GetDeviceName()
{
	return s_DeviceName.c_str();
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

	m_OPL.NoiseLFSR = 1 << 22;

	/* Default operator register state */
	for (auto& Slot : m_OPL.Slot)
	{
		Slot.Multi = YM::OPL::Multiply[0]; /* x0.5 */

		Slot.EgPhase = ADSR::Release;
		Slot.EgLevel = YM::OPL::MaxAttenuation;
		Slot.EgType = 1; /* non-percussive sound */

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
	if (OutputNr == AudioOut::Default)
	{
		Desc.SampleRate		= m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat	= m_DAC->GetAudioFormat();
		Desc.Channels		= 1;
		Desc.ChannelMask	= SPEAKER_FRONT_CENTER;
		Desc.Description	= L"Analog out (" + m_DAC->GetDeviceName() + L")";
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
		WriteRegisterArray(m_AddressLatch, Data);
	}
}

void YM3526::WriteRegisterArray(uint8_t Address, uint8_t Data)
{
	/* Address to slot mapping */
	static const int32_t SlotMap[32] =
	{
		 0,  2,  4,  1,  3,  5, -1, -1,
		 6,  8, 10,  7,  9, 11, -1, -1,
		12, 14, 16, 13, 15, 17, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1
	};

	/* Address to channel mapping */
	static const int32_t ChannelMap[16] =
	{
		 CH1, CH2, CH3, CH4, CH5, CH6, CH7, CH8,
		 CH9,  -1,  -1,  -1,  -1,  -1,  -1,  -1
	};

	switch (Address & 0xF0)
	{
	case 0x00: /* Mode data */
		switch (Address & 0xF)
		{
		case 0x01: /* LSI test */
			m_OPL.LsiTest2 = (Data >> 2) & 0x01; /* Phase generator reset */
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
		int32_t ChannelId = ChannelMap[Address & 0x0F]; if (ChannelId == -1) return;
		auto& Channel = m_OPL.Channel[ChannelId];

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

			if (m_OPL.RHY) /* Key on /off drum instruments */
			{
				m_OPL.Slot[Rhythm::BD1].DrumLatch = (Data >> 4) & 0x01;
				m_OPL.Slot[Rhythm::BD2].DrumLatch = (Data >> 4) & 0x01;
				m_OPL.Slot[Rhythm::SD].DrumLatch = (Data >> 3) & 0x01;
				m_OPL.Slot[Rhythm::TOM].DrumLatch = (Data >> 2) & 0x01;
				m_OPL.Slot[Rhythm::TC].DrumLatch = (Data >> 1) & 0x01;
				m_OPL.Slot[Rhythm::HH].DrumLatch = (Data >> 0) & 0x01;
			}
		}
		else
		{
			int32_t ChannelId = ChannelMap[Address & 0x0F]; if (ChannelId == -1) return;
			auto& Channel = m_OPL.Channel[ChannelId];

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
		int32_t ChannelId = ChannelMap[Address & 0x0F]; if (ChannelId == -1) return;
		auto& Channel = m_OPL.Channel[ChannelId];

		Channel.FB = (Data >> 1) & 0x07;
		Channel.Algo = (Data >> 0) & 0x01;
		break;
	}

	default: /* Not used */
		break;
	}
}

void YM3526::UpdateTimers()
{
	/* Update global timer */
	m_OPL.Timer++;

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
					for (auto& Slot : m_OPL.Slot) Slot.CsmLatch |= 1;
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
		 0,  2,  4,  1,  3,  5,
		 6,  8, 10,  7,  9, 11,
		12, 14, 16, 13, 15, 17
	};

	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	while (Samples-- != 0)
	{
		ClearOutput();
		UpdateTimers();

		/* Update slots (operators) */
		for (auto& SlotId : SlotOrder)
		{
			UpdateEnvelopeGenerator(SlotId);
			UpdatePhaseGenerator(SlotId);
			UpdateOperatorUnit(SlotId);
			UpdateNoiseGenerator();
		}

		GenerateOutput(CH1);
		GenerateOutput(CH2);
		GenerateOutput(CH3);
		GenerateOutput(CH4);
		GenerateOutput(CH5);
		GenerateOutput(CH6);
		GenerateOutput(CH7);
		GenerateOutput(CH8);
		GenerateOutput(CH9);

		/* Limiter (signed 16-bit) */
		int16_t Out = std::clamp(m_OPL.Out, -32768, 32767);

		/* Digital to "analog" conversion */
		float AnalogOut = m_DAC->SendDigitalData(Out);

		OutBuffer[AudioOut::Default]->WriteSampleF32(AnalogOut);
	}
}

void YM3526::UpdatePhaseGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPL.Channel[SlotId >> 1];
	auto& Slot = m_OPL.Slot[SlotId];

	uint32_t FNum = Chan.FNum;

	/* Reset phase counter */
	if (Slot.PgReset | m_OPL.LsiTest2) Slot.PgPhase = 0;

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
	Slot.PgOutput = Slot.PgPhase >> 9;

	if (m_OPL.RHY)
	{
		switch (SlotId)
		{
		case Rhythm::HH:
		{
			/* Get high hat b8; map it to b1 of the result (b0 of the result is reserved for the noise bit) */
			m_OPL.PhaseHH8 = (Slot.PgOutput >> 7) & 0x02; /* b8 */

			/* Get high hat b7, b3 and b2; map it to b4, b3, and b2 of the result */
			m_OPL.PhaseHH = (Slot.PgOutput >> 3) & 0x10; /* b7 */
			m_OPL.PhaseHH |= (Slot.PgOutput >> 0) & 0x0C; /* b3 and b2 */

			/* Lookup the phase input bit */
			uint32_t PhaseIn = YM::OPL::PhaseIn[m_OPL.PhaseHH | m_OPL.PhaseTC];

			/* Lookup the high hat phase output bits */
			Slot.PgOutput = YM::OPL::PhaseOutHH[(PhaseIn << 1) | m_OPL.NoiseOut];
			break;
		}

		case Rhythm::SD:
			/* Lookup the snare drum phase output bits */
			Slot.PgOutput = YM::OPL::PhaseOutSD[m_OPL.PhaseHH8 | m_OPL.NoiseOut];
			break;

		case Rhythm::TC:
		{
			/* Get top cymbal b5 and b3; map it to b1 and b0 of the result */
			m_OPL.PhaseTC = (Slot.PgOutput >> 4) & 0x02; /* b5 */
			m_OPL.PhaseTC |= (Slot.PgOutput >> 3) & 0x01; /* b3 */

			/* Lookup the phase input bit */
			uint32_t PhaseIn = YM::OPL::PhaseIn[m_OPL.PhaseHH | m_OPL.PhaseTC];

			/* Calculate the top cymbal phase output bits */
			Slot.PgOutput = (PhaseIn << 9) | 0x80;
			break;
		}
		}
	}
}

void YM3526::UpdateEnvelopeGenerator(uint32_t SlotId)
{
	auto& Chan = m_OPL.Channel[SlotId >> 1];
	auto& Slot = m_OPL.Slot[SlotId];

	/*-------------------------------------*/
	/* Step 1: Key On / Off event handling */
	/*-------------------------------------*/
	uint32_t NewKeyState = (Chan.KeyLatch | Slot.CsmLatch | Slot.DrumLatch);
	uint32_t EnvelopeStart = 0;

	/* Clear CSM key on flag */
	Slot.CsmLatch = 0;

	switch ((NewKeyState << 1) | Slot.KeyState)
	{
	case 0x00:
	case 0x03: /* No key state changes */
		Slot.PgReset = 0;
		break;

	case 0x01: /* Key off state */
		Slot.EgPhase = ADSR::Release;
		Slot.PgReset = 0;
		Slot.KeyState = 0;
		break;

	case 0x02: /* Key on state */
		Slot.EgPhase = ADSR::Attack;
		Slot.PgReset = 1;
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
		Rate = Slot.EgRate[ADSR::Decay];
		break;

	case ADSR::Sustain:
		/* Note: EG-Type selects sustain or release */
		Rate = Slot.EgRate[ADSR::Sustain + Slot.EgType];
		break;

	case ADSR::Release:
		Rate = Slot.EgRate[ADSR::Release];
		break;
	}

	if (Rate != 0)
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
					if (Level != 0) Level += ((~Level * AttnInc) >> 3);
				}

				if (Level == 0) Slot.EgPhase = (Slot.SustainLvl != 0) ? ADSR::Decay : ADSR::Sustain;
				break;

			case ADSR::Decay:
				Level += AttnInc;
				if ((Level >> 4) == Slot.SustainLvl) Slot.EgPhase = ADSR::Sustain;
				break;

			case ADSR::Sustain:
			case ADSR::Release:
				Level += AttnInc;
				if (Level >= YM::OPL::MaxEgLevel) Level = YM::OPL::MaxAttenuation;
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
	uint32_t Phase = Slot.PgOutput + GetModulation(SlotId);

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

void YM3526::UpdateNoiseGenerator()
{
	/* ! This needs validation ! */
	m_OPL.NoiseOut = m_OPL.NoiseLFSR & 1;

	uint32_t Seed = ((m_OPL.NoiseLFSR >> 14) ^ (m_OPL.NoiseLFSR >> 0)) & 1;
	m_OPL.NoiseLFSR = (m_OPL.NoiseLFSR >> 1) | (Seed << 22);
}

void YM3526::ClearOutput()
{
	m_OPL.Out = 0;
}

void YM3526::GenerateOutput(uint32_t ChannelId)
{
	/* ! This needs validation ! */
	auto& Chan = m_OPL.Channel[ChannelId];

	int16_t Output = 0;

	if (m_OPL.RHY)
	{
		switch (ChannelId)
		{
		case CH7: /* Bass drum */
			Output = m_OPL.Slot[BD2].Output[0];

			/* Limit (13-bit) and mix channel output */
			m_OPL.Out += std::clamp<int16_t>(Output, -4096, 4095) * 2;
			return;

		case CH8: /* High hat + Snare drum */
			Output = m_OPL.Slot[HH].Output[1]; /* Delayed by 1 sample ? */
			Output += m_OPL.Slot[SD].Output[0];

			/* Limit (13-bit) and mix channel output */
			m_OPL.Out += std::clamp<int16_t>(Output, -4096, 4095) * 2;
			return;

		case CH9: /* Tom + Top cymbal */
			Output = m_OPL.Slot[TOM].Output[1]; /* Delayed by 1 sample ? */
			Output += m_OPL.Slot[TC].Output[0];

			/* Limit (13-bit) and mix channel output */
			m_OPL.Out += std::clamp<int16_t>(Output, -4096, 4095) * 2;
			return;
		}
	}

	uint32_t SlotBase = ChannelId << 1;

	if (Chan.Algo == 0)
	{
		Output = m_OPL.Slot[SlotBase + S2].Output[0];
	}
	else
	{
		Output += m_OPL.Slot[SlotBase + S1].Output[1]; /* Delayed by 1 sample */
		Output += m_OPL.Slot[SlotBase + S2].Output[0];
	}

	/* Limit (13-bit) and mix channel output */
	m_OPL.Out += std::clamp<int16_t>(Output, -4096, 4095);
}

int16_t YM3526::GetModulation(uint32_t SlotId)
{
	auto& Chan = m_OPL.Channel[SlotId >> 1];

	if (m_OPL.RHY)
	{
		/*
			A special case is needed for the drum instruments that don't have modulation input.
			The bass drum and tom can be processed normally.

			High hat can still do self feedback ?
		*/

		switch (SlotId)
		{
			//case Rhythm::HH: return 0;
		case Rhythm::SD: return 0;
		case Rhythm::TC: return 0;
		}
	}

	switch ((Chan.Algo << 1) | (SlotId & 1))
	{
	case 0x00: /* Algo: 0 - Modulator */
	case 0x02: /* Algo: 1 - Modulator */
		if (Chan.FB) /* Slot 1 self-feedback modulation */
			return (m_OPL.Slot[SlotId].Output[0] + m_OPL.Slot[SlotId].Output[1]) >> (9 - Chan.FB);
		else
			return 0;

	case 0x01: /* Algo: 0 - Carrier */
		return m_OPL.Slot[SlotId - 1].Output[1]; /* Delayed by 1 sample */

	case 0x03: /* Algo: 1 - Carrier */
		return 0;
	}

	return 0;
}