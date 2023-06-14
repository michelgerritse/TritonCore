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
	- 1 shared LFO unit for PM and AM
	- 3CH mode (individual frequency settings for the channel 3 operators)
	- CSM mode (Timer A generated key-on events)
	- Channel 6 can be used for PCM playback
	- SSG-EG envelope modes
	- Built-in 9-bit DAC

	A couple of notes:
	- The FM prescaler is fixed to 6
	- There is an overflow bug in the EG counter, causing it to never reach 0
	- The DAC has a discontinuity error aka. "Ladder Effect"

	SSG-EG mode cheat sheet:
	------------------------
	
	0x00 - Normal cycles:			0x04 - Inverted normal cycles:

	|\  |\  |\  |\					  /|  /|  /|  /|
	| \ | \ | \ | \					 / | / | / | / |
    |  \|  \|  \|  \				/  |/  |/  |/  |

	0x01 - One cycle, hold low:		0x05 - Inverted one cycle, hold low
										______________
	|\								  /
	| \								 /
	|  \______________				/

	0x02 - Alternating cycles:		0x06 - Inverted alternating cycles

	|\     ^      ^					   ^      ^     /
	| \  /   \  /   \				 /   \  /   \  /
	|  \/     \/     \				/     \/     \/

	0x03 - One cycle, hold high:	0x07 - Inverted one cycle, hold high
	     _____________
	|\  |							  /|
	| \ |							 / |
	|  \|							/  |_______________


	Mode | Inv | Alt | Hld |
	-----+-----+-----+-----+
	0x00 |  0  |  0  |  0  |
	0x01 |  0  |  0  |  1  |
	0x02 |  0  |  1  |  0  |
	0x03 |  0  |  1  |  1  |
	0x04 |  1  |  0  |  0  |
	0x05 |  1  |  0  |  1  |
	0x06 |  1  |  1  |  0  |
	0x07 |  1  |  1  |  1  |
	-----+-----+-----+-----+
*/

/* Timer A overflow flag */
#define TIMER_A_OVERFLOW 0x01

/* Timer B overflow flag */
#define TIMER_B_OVERFLOW 0x02

/* Busy status flag */
#define BUSY_STATUS_FLAG 0x80

/* Channel output mask */
#define OUTPUT_MASK ~0

/* Slot ordering */
enum SLOT_NUM
{
	S1 = 0, S2, S3, S4
};

static const uint32_t SlotOrder[24] =
{
	 0 + S1,  0 + S3,  0 + S2,  0 + S4, /* Channel 1 */
	 4 + S1,  4 + S3,  4 + S2,  4 + S4, /* Channel 2 */
	 8 + S1,  8 + S3,  8 + S2,  8 + S4, /* Channel 3 */
	12 + S1, 12 + S3, 12 + S2, 12 + S4, /* Channel 4 */
	16 + S1, 16 + S3, 16 + S2, 16 + S4, /* Channel 5 */
	20 + S1, 20 + S3, 20 + S2, 20 + S4  /* Channel 6 */
};

YM2612::YM2612(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(6 * 24)
{
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

	/* Reset DAC */
	m_DacSelect = 0;
	m_DacData = 0;

	/* Reset LFO */
	m_LfoCounter = 0;
	m_LfoPeriod = YM::OPN::LfoPeriod[0];
	m_LfoStep = 0;
	m_LfoEnable = 0;

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

		/* All channels are ON by default */
		Channel.MaskL = OUTPUT_MASK;
		Channel.MaskR = OUTPUT_MASK;
	}
}

void YM2612::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	if (Command == 0x2A) /* Handle VGM DAC stream */
	{
		/* Convert from unsigned 8 to signed 14-bit */
		m_DacData = (m_DacData & 0x01) | ((Value - 0x80) << 1);
	}
}

bool YM2612::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
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

void YM2612::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM2612::GetClockSpeed()
{
	return m_ClockSpeed;
}

void YM2612::Write(uint32_t Address, uint32_t Data)
{
	/* 8-bit data bus (D0 - D7) */
	Data &= 0xFF;

	switch (Address & 0x03) /* 2-bit address bus (A0 - A1) */
	{
	case 0x00: /* Address write mode */
	case 0x02:
		m_AddressLatch = Data;
		m_PortLatch = (Address & 0x02) >> 1;
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

void YM2612::WriteMode(uint8_t Register, uint8_t Data)
{
	switch (Register) /* 0x20 - 0x2F */
	{
	case 0x21: /* LSI Test */
		/* Not implemented */
		break;

	case 0x22: /* LFO Control */
		m_LfoEnable = (Data & 0x08) ? ~0 : 0; /* Note: LFO Enable is implemented as a mask */
		m_LfoPeriod = YM::OPN::LfoPeriod[Data & 0x07];
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
		break;

	case 0x28: /* Key On/Off */
	{
		if ((Data & 0x03) == 0x03) break; /* Invalid channel */
		
		uint32_t ChannelId = ((Data & 0x03) + ((Data & 0x04) ? 3 : 0)) << 2;

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

	case 0x2A: /* DAC data */
		/* Convert from unsigned 8 to signed 14-bit */
		m_DacData = (m_DacData & 0x01) | ((Data - 0x80) << 1); /* bit [8:1] */
		break;

	case 0x2B: /* DAC select */
		m_DacSelect = Data >> 7;
		break;

	case 0x2C: /* LSI Test 2 */
		m_DacData = (m_DacData & ~0x01) | ((Data >> 3) & 0x01); /* bit [0] */
		break;

	default:
		break;
	}
}

void YM2612::WriteFM(uint8_t Register, uint8_t Port, uint8_t Data)
{
	/* Slot address mapping: S1 - S3 - S2 - S4 */
	static const int32_t SlotMap[2][16] =
	{
		{  0,  4,  8, -1,  2,  6, 10, -1,  1,  5,  9, -1,  3,  7, 11, -1 }, /* Channel 1-2-3 (port 0) */
		{ 12, 16, 20, -1, 14, 18, 22, -1, 13, 17, 21, -1, 15, 19, 23, -1 }	/* Channel 4-5-6 (port 1) */
	};

	int32_t SlotId = SlotMap[Port][Register & 0x0F];
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
			Slot.SsgEnable= (Data >> 3) & 0x01;
			Slot.SsgEgInv = (Data >> 2) & 0x01;
			Slot.SsgEgAlt = (Data >> 1) & 0x01;
			Slot.SsgEgHld = (Data >> 0) & 0x01;
			break;
		}
	}
	else /* Channel register map (0xA0 - 0xB6) */
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
			if (Port == 0)
			{				
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
			}
			break;

		case 0xAC: /* 3 Ch-3 F-Num / Block Latch */
			if (Port == 0) m_3ChFNumLatch = Data & 0x3F;
			break;

		case 0xB0: /* Feedback / Connection */
			Chan.FB = (Data >> 3) & 0x07;
			Chan.Algo = Data & 0x07;
			break;

		case 0xB4: /* PMS / AMS / Panning */
			Chan.MaskL = (Data & 0x80) ? OUTPUT_MASK : 0;
			Chan.MaskR = (Data & 0x40) ? OUTPUT_MASK : 0;
			Chan.AMS = (Data >> 4) & 0x03;
			Chan.PMS = Data & 0x07;
			break;
		}
	}
}

void YM2612::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
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

		/* Update Timer A, Timer B and LFO */
		UpdateTimers();
		UpdateLFO();

		/* Update Envelope Generator clock */
		m_EgClock = (m_EgClock + 1) % 3;
		
		/* Update Envelope Generator counter */
		m_EgCounter += (m_EgClock >> 1);
		m_EgCounter += (m_EgCounter >> 12); /* Overflow bug in the OPN unit */
		m_EgCounter &= 0xFFF;

		/* Update slots (operators) */
		for (auto &Slot : SlotOrder)
		{
			PrepareSlot(Slot);
			UpdatePhaseGenerator(Slot);
			UpdateEnvelopeGenerator(Slot);
			UpdateOperatorUnit(Slot);
		}

		/* Note: The output of the YM2612 is multiplexed
		Channel output order is: 1-5-3-2-6-4 */
		//TODO: 9-bit DAC emulation
		UpdateAccumulator(0);
		UpdateAccumulator(1);
		UpdateAccumulator(2);
		UpdateAccumulator(3);
		UpdateAccumulator(4);

		if (m_DacSelect) m_Channel[5].Output = m_DacData << 5;
		else UpdateAccumulator(5);

		OutL += (m_Channel[0].Output & m_Channel[0].MaskL);
		OutR += (m_Channel[0].Output & m_Channel[0].MaskR);

		OutL += (m_Channel[1].Output & m_Channel[1].MaskL);
		OutR += (m_Channel[1].Output & m_Channel[1].MaskR);

		OutL += (m_Channel[2].Output & m_Channel[2].MaskL);
		OutR += (m_Channel[2].Output & m_Channel[2].MaskR);

		OutL += (m_Channel[3].Output & m_Channel[3].MaskL);
		OutR += (m_Channel[3].Output & m_Channel[3].MaskR);

		OutL += (m_Channel[4].Output & m_Channel[4].MaskL);
		OutR += (m_Channel[4].Output & m_Channel[4].MaskR);

		OutL += (m_Channel[5].Output & m_Channel[5].MaskL);
		OutR += (m_Channel[5].Output & m_Channel[5].MaskR);
		
		/* Limiter (signed 16-bit) */
		OutL = std::clamp(OutL, -32768, 32767);
		OutR = std::clamp(OutR, -32768, 32767);

		/* 16-bit DAC output (interleaved) */
		OutBuffer[0]->WriteSampleS16(OutL);
		OutBuffer[0]->WriteSampleS16(OutR);

		Samples--;
	}
}

void YM2612::PrepareSlot(uint32_t SlotId)
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
		if ((ChannelId == 2) && (i != S4))
		{
			Slot.FNum = m_3ChFNum[i];
			Slot.Block = m_3ChBlock[i];
			Slot.KeyCode = m_3ChKeyCode[i];
		}
	}
}

void YM2612::UpdatePhaseGenerator(uint32_t SlotId)
{
	auto& Chan = m_Channel[SlotId >> 2];
	auto& Slot = m_Slot[SlotId];
	
	uint32_t FNum = Slot.FNum << 1; /* 11 to 12-bit */
	
	/* LFO frequency modulation (12-bit result) */
	FNum = (FNum + YM::OPN::LfoPmTable[FNum >> 5][m_LfoStep >> 2][Chan.PMS]) & 0xFFF;

	/*	Block shift (17-bit result) */
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
	auto& Chan = m_Channel[SlotId >> 2];
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
				if (Slot.SsgEgAlt == 0) Slot.PgPhase = 0;
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
	if (m_EgClock == 2)
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

	/* Apply AM LFO */
	Attn += (YM::OPN::LfoAmTable[m_LfoStep][Chan.AMS] & Slot.AmOn);

	/* Limit (10-bit = 4.6 fixed point) */
	if (Attn > 0x3FF) Attn = 0x3FF;

	/* Convert from 4.6 to 4.8 fixed point */
	Slot.EgOutput = Attn << 2;
}

void YM2612::UpdateOperatorUnit(uint32_t SlotId)
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

void YM2612::UpdateAccumulator(uint32_t ChannelId)
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

int16_t YM2612::GetModulation(uint32_t Cycle)
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

void YM2612::StartEnvelope(uint32_t SlotId)
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

void YM2612::UpdateLFO()
{
	if (++m_LfoCounter >= m_LfoPeriod)
	{
		/* Reset counter */
		m_LfoCounter = 0;

		/* Increase step counter (7-bit) */
		m_LfoStep = (m_LfoStep + 1) & 0x7F;
	}
	
	/* Enable / disable LFO */
	m_LfoStep &= m_LfoEnable;
}

void YM2612::UpdateTimers()
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