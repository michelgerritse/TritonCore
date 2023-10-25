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

static int16_t s_Volume[16] =
{
	0, 87, 123, 173, 245, 345, 488, 689, 973, 1375, 1942, 2744, 3875, 5474, 7732, 10922
};

static uint32_t s_EnvelopeStep[16][16]
{
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0}, /* b0000 */
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* b0001 */
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0}, /* b0010 */
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* b0011 */
	
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15}, /* b0100 */
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* b0101 */
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15}, /* b0110 */
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* b0111 */

	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0}, /* b1000 */
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* b1001 */
	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15}, /* b1010 */
	{ 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, /* b1011 */

	{  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15}, /* b1100 */
	{ 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15}, /* b1101 */
	{ 15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0}, /* b1110 */
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, /* b1111 */
};


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
	for (auto i = 0; i < 16; i++)
	{
		m_Register[i] = 0;
	}

	/* Reset tone channels to default state */
	for (auto i = 0; i < 3; i++)
	{
		m_Tone[i].Counter = 0;
		m_Tone[i].Period.u32 = 0;
		m_Tone[i].Output = 0;
		m_Tone[i].Volume = 0;

		m_Tone[i].ToneDisable = 0;
		m_Tone[i].NoiseDisable = 0;
		m_Tone[i].AmpCtrl = 0;
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
	m_Envelope.Volume = s_Volume[15];
	m_Envelope.FlipFlop = 0;
	m_Envelope.Step = 0;
	m_Envelope.State = 0;
	m_Envelope.Hld = 1;
	m_Envelope.Alt = 0;
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
	Data &= 0xFF;

	m_Register[Address] = Data;

	switch (Address)
	{
		case 0x00: /* Channel A Tone Period (Fine Tune) */
			m_Tone[0].Period.u8ll = Data;
			break;

		case 0x01: /* Channel A Tone Period (Coarse Tune) */
			m_Tone[0].Period.u8lh = Data & 0x0F;
			break;

		case 0x02: /* Channel B Tone Period (Fine Tune) */
			m_Tone[1].Period.u8ll = Data;
			break;

		case 0x03: /* Channel B Tone Period (Coarse Tune) */
			m_Tone[1].Period.u8lh = Data & 0x0F;
			break;

		case 0x04: /* Channel C Tone Period (Fine Tune) */
			m_Tone[2].Period.u8ll = Data;
			break;

		case 0x05: /* Channel C Tone Period (Coarse Tune) */
			m_Tone[2].Period.u8lh = Data & 0x0F;
			break;

		case 0x06: /* Noise Period */
			m_Noise.Period = Data & 0x1F;
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
			m_Tone[0].Volume = s_Volume[Data & 0x0F];
			m_Tone[0].AmpCtrl = (Data & 0x10) >> 4;
			break;

		case 0x09: /* Channel B Amplitude Control */
			m_Tone[1].Volume = s_Volume[Data & 0x0F];
			m_Tone[1].AmpCtrl = (Data & 0x10) >> 4;
			break;

		case 0x0A: /* Channel C Amplitude Control */
			m_Tone[2].Volume = s_Volume[Data & 0x0F];
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
			m_Envelope.Step = 0;

			m_Envelope.State = Data & 0x0C; /* CONT and ATT bits only */
			m_Envelope.Hld = (Data & 0x08) ? (Data & 0x01) : 1;
			m_Envelope.Alt = Data & 0x02;

			m_Envelope.Volume = s_Volume[(Data & 0x04) ? 0 : 15];
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

	while (Samples-- != 0)
	{
		/* Update envelope generator */
		if ((m_Envelope.Counter += 2) >= m_Envelope.Period.u32) //FIXME: should be += 1
		{
			/* Reset counter */
			m_Envelope.Counter = 0;

			/* Update envelope when flipflop transitioned from 0 to 1 */
			if (m_Envelope.FlipFlop ^= 1)
			{
				if (++m_Envelope.Step & 16) /* Envelope period completed */
				{
					m_Envelope.Step = 0;
					m_Envelope.State |= m_Envelope.Hld;
					m_Envelope.State ^= m_Envelope.Alt;
				}

				/* Lookup "real" envelope step */
				uint32_t EnvStep = s_EnvelopeStep[m_Envelope.State][m_Envelope.Step];

				/* Lookup new volume (or amplitude) */
				m_Envelope.Volume = s_Volume[EnvStep];
			}
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

		/* Update, mix and output tone channels */
		for (auto i = 0; i < 3; i++)
		{
			auto& Tone = m_Tone[i];
			
			Out = 0;

			if ((Tone.Counter += 2) >= Tone.Period.u32) //FIXME: should be += 1
			{
				/* Reset counter */
				Tone.Counter = 0;

				/* Toggle output flag */
				Tone.Output ^= 1;
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