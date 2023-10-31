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
#include "YM2149.h"

/*
	Yamaha YM2149
*/

YM2149::YM2149(uint32_t ClockSpeed, bool SelIsLow) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(SelIsLow ? 32 : 16) //FIXME: should be 16 : 8
{
	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* YM2149::GetDeviceName()
{
	return L"Yamaha YM2149";
}

void YM2149::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	/* Clear register array (initial state is 0 for all registers) */
	m_Register.fill(0);

	/* Reset tone generators */
	for (auto& Tone : m_Tone)
	{
		memset(&Tone, 0, sizeof(AY::channel_t));
		Tone.Volume = AY::Volume32[1];
	}

	/* Reset noise generator */
	m_Noise.Counter = 0;
	m_Noise.Period = 0;
	m_Noise.Output = 0;
	m_Noise.FlipFlop = 0;
	m_Noise.LFSR = 1 << (17 - 1);

	/* Reset envelope generator */
	m_Envelope.Counter = 0;
	m_Envelope.Period.u32 = 0;
	m_Envelope.Volume = AY::Volume32[31];
	m_Envelope.Step = 31;
	m_Envelope.StepDec = 1;
	m_Envelope.Hld = 1;
	m_Envelope.Alt = 31;
	m_Envelope.Inv = 0;
}

void YM2149::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
}

bool YM2149::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
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

void YM2149::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t YM2149::GetClockSpeed()
{
	return m_ClockSpeed;
}

void YM2149::Write(uint32_t Address, uint32_t Data)
{
	Address &= 0x0F;
	m_Register[Address] = Data;

	Data &= AY::Mask[Address];

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
		m_Tone[0].Volume = AY::Volume32[((Data & 0x0F) << 1) | 1];
		m_Tone[0].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x09: /* Channel B Amplitude Control */
		m_Tone[1].Volume = AY::Volume32[((Data & 0x0F) << 1) | 1];
		m_Tone[1].AmpCtrl = (Data & 0x10) >> 4;
		break;

	case 0x0A: /* Channel C Amplitude Control */
		m_Tone[2].Volume = AY::Volume32[((Data & 0x0F) << 1) | 1];
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
		m_Envelope.Volume = AY::Volume32[m_Envelope.Step ^ m_Envelope.Inv];
		break;

	case 0x0E: /* I/O Port A Data Store */
		/* Not implemented */
		break;

	case 0x0F: /* I/O Port B Data Store */
		/* Not implemented */
		break;
	}
}

void YM2149::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int16_t Out;

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

			/* Apply output inversion and lookup volume */
			m_Envelope.Volume = AY::Volume32[m_Envelope.Step ^ m_Envelope.Inv];
		}

		/* Update noise generator */
		if ((m_Noise.Counter += 2) >= m_Noise.Period) //FIXME: should be += 1
		{
			/* Reset counter */
			m_Noise.Counter = 0;

			/* Update LFSR when flipflop transitioned from 0 to 1 */
			if (m_Noise.FlipFlop ^= 1)
			{
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

			Out = 0;

			if (Tone.Period.u32 == 0) //FIXME: No special case needed for period = 0
			{
				Tone.Output = 1;
			}
			else
			{
				if ((Tone.Counter += 2) >= Tone.Period.u32) //FIXME: should be += 1
				{
					/* Reset counter */
					Tone.Counter = 0;

					/* Toggle output flag */
					Tone.Output ^= 1;
				}
			}

			if ((Tone.Output | Tone.ToneDisable) & (m_Noise.Output | Tone.NoiseDisable))
			{
				/* Volume control */
				Out = Tone.AmpCtrl ? m_Envelope.Volume : Tone.Volume;
			}

			/* 16-bit output */
			OutBuffer[i]->WriteSampleS16(Out);
		}
	}
}