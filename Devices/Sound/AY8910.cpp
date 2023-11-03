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
#include "AY8910.h"

/*
	General Instrument AY-3-8910
*/

AY8910::AY8910(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(16) //FIXME: should be 8
{
	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* AY8910::GetDeviceName()
{
	return L"General Instrument AY-3-8910";
}

void AY8910::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Clear register array (initial state is 0 for all registers) */
	m_Register.fill(0);

	/* Reset tone generators */
	for (auto& Tone : m_Tone)
	{
		memset(&Tone, 0, sizeof(AY::tone_t));
	}

	/* Reset noise generator */
	memset(&m_Noise, 0, sizeof(AY::noise_t));
	m_Noise.LFSR = 1 << (17 - 1);

	/* Reset envelope generator */
	m_Envelope.Counter = 0;
	m_Envelope.Period.u32 = 0;
	m_Envelope.Amplitude = AY::Amplitude16[15];
	m_Envelope.Prescaler = 0;
	m_Envelope.Step = 15;
	m_Envelope.StepDec = 1;
	m_Envelope.Hld = 1;
	m_Envelope.Alt = 15;
	m_Envelope.Inv = 0;
}

void AY8910::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
}

bool AY8910::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	switch (OutputNr)
	{
		case 0: /* Channel A */
			Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"Channel A";
			break;

		case 1: /* Channel B */
			Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"Channel B";
			break;

		case 2: /* Channel C */
			Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
			Desc.SampleFormat = 0;
			Desc.Channels = 1;
			Desc.ChannelMask = SPEAKER_FRONT_CENTER;
			Desc.Description = L"Channel C";
			break;

		default:
			return false;
	}
	
	return true;
}

void AY8910::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t AY8910::GetClockSpeed()
{
	return m_ClockSpeed;
}

void AY8910::Write(uint32_t Address, uint32_t Data)
{
	Address &= 0x0F;
	Data &= AY::Mask[Address]; /* Mask unused bits before storing */

	m_Register[Address] = Data;

	switch (Address)
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
			m_Tone[0].Amplitude = AY::Amplitude16[Data & 0x0F];
			m_Tone[0].AmpCtrl = (Data & 0x10) >> 4;
			break;

		case 0x09: /* Channel B Amplitude Control */
			m_Tone[1].Amplitude = AY::Amplitude16[Data & 0x0F];
			m_Tone[1].AmpCtrl = (Data & 0x10) >> 4;
			break;

		case 0x0A: /* Channel C Amplitude Control */
			m_Tone[2].Amplitude = AY::Amplitude16[Data & 0x0F];
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
			m_Envelope.Step = 15;
			m_Envelope.StepDec = 1;

			/* If attacking, apply output inversion */
			m_Envelope.Inv = (Data & 0x04) ? 15 : 0;
			
			if (Data & 0x08) /* Continuous cycles */
			{								
				m_Envelope.Hld = Data & 0x01;

				if (m_Envelope.Hld)
					m_Envelope.Alt = (Data & 0x02) ? 0 : 15;
				else
					m_Envelope.Alt = (Data & 0x02) ? 15 : 0;
			}
			else /* Single cycle */
			{
				m_Envelope.Hld = 1;
				m_Envelope.Alt = m_Envelope.Inv ^ 15;
			}

			/* Set initial ouput volume */
			m_Envelope.Amplitude = AY::Amplitude16[m_Envelope.Step ^ m_Envelope.Inv] + AY::DCOffset02V;
			break;

		case 0x0E: /* I/O Port A Data Store */
			/* Not implemented */
			break;

		case 0x0F: /* I/O Port B Data Store */
			/* Not implemented */
			break;
	}
}

void AY8910::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int16_t Out;
	uint32_t Mask;

	while (Samples-- != 0)
	{
		/* Update envelope generator */
		if (m_Envelope.Prescaler ^= 1)
		{
			if ((m_Envelope.Counter += 2) >= m_Envelope.Period.u32) //FIXME: should be += 1
			{
				/* Reset counter */
				m_Envelope.Counter = 0;

				/* Count down step counter (15 -> 0) */
				m_Envelope.Step -= m_Envelope.StepDec;

				if (m_Envelope.Step & 16) /* Envelope cycle completed */
				{
					/* Restart cycle */
					m_Envelope.Step = 15;

					/* Stop counting (if needed) */
					m_Envelope.StepDec = m_Envelope.Hld ^ 1;

					/* Toggle output inversion */
					m_Envelope.Inv ^= m_Envelope.Alt;
				}

				/* Apply output inversion and lookup amplitude */
				m_Envelope.Amplitude = AY::Amplitude16[m_Envelope.Step ^ m_Envelope.Inv] + AY::DCOffset02V;
			}
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