/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023, Michel Gerritse
All rights reserved.

This source code is available under the BSD-style license.
See LICENSE.txt file in the root directory of this source tree.

*/
#include "SN76489.h"

/*
This sound emulator implements the Sega style PSG:
	- Shift register is 16-bit wide
	- Tap bit 0 and 3 (XOR) for white noise feedback
	- Tap bit 0 for periodic noise feedback
	- Tone/noise period of 0 equals period of 1
	- Internal /16 clock divider

The following features are missing:
	- READY signal is not implemented

Known issues:
	- The Triton audio engine currently does not allow sample rates > 200KHz. This is an XAudio2 limit
	  To work around this the clock divider is doubled and the counters count down with 2
*/

#define MAX_VOLUME		32767 / 2
#define LFSR_FEEDBACK	0x8000

/* Clock divider */
const uint32_t ClockDivider = 32; //FIXME: Should be 16

void SN76489::IncRef()
{
}

const wchar_t* SN76489::GetDeviceName()
{
	return L"Texas Instruments SN76489";
}

void SN76489::Initialize(uint32_t ClockSpeed, uint32_t Model, uint32_t Flags)
{
	m_ClockSpeed = ClockSpeed;
	m_SampleRate = ClockSpeed / ClockDivider;

	double Volume = MAX_VOLUME / 4.0; /* Avoid clipping when volume of all 4 channels is MAX */

	/* Build DAC output table */
	for (auto i = 0; i < 15; i++)
	{
		m_VolumeTable[i] = (int16_t) Volume;
		Volume /= 1.258925412; /* 2dB drop per step (10 ^ (2/20)) */
	}
	m_VolumeTable[15] = 0; /* OFF (full attenuation) */

	Reset();
}

void SN76489::Reset()
{
	m_CyclesToDo = 0;

	m_Register = 3; /* Channel 2 volume register is latched on reset */
	m_StereoMask = 0xFF;

	/* Reset tone generators */
	for (auto i = 0; i < 3; i++)
	{
		m_Tone[i].Counter	= 0;
		m_Tone[i].Period	= 0;
		m_Tone[i].FlipFlop	= 0xFFFF;
		m_Tone[i].Volume	= m_VolumeTable[15];
	}

	/* Reset noise generator */
	m_Noise.Counter	= 0;
	m_Noise.FlipFlop = 1;
	m_Noise.Control	= 0;
	m_Noise.Period	= 16;
	m_Noise.LFSR	= LFSR_FEEDBACK;
	m_Noise.Volume	= m_VolumeTable[15];
	m_Noise.Output	= 0;
}

bool SN76489::EnumOutputs(uint32_t OutputId, SOUND_OUTPUT_DESC& Desc)
{
	if (OutputId == 0)
	{
		Desc.SampleRate	 = m_SampleRate;
		Desc.Channels	 = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;

		return true;
	}

	return false;
}

uint32_t SN76489::GetClockSpeed()
{
	return m_ClockSpeed;
}

void SN76489::Write(uint32_t Address, uint8_t Data)
{
	if (Address & 0x01) /* Might change this to 0x06 to match the port on the Game Gear */
	{
		m_StereoMask = Data;
		return;
	}
	
	auto Latch = Data & 0x80;
	
	if (Latch) /* Latch/Data write */
	{
		m_Register = (Data >> 4) & 0x07;
	}

	switch (m_Register)
	{
		case 0: /* Channel 1 period */
			if (Latch)
				m_Tone[0].Period = (m_Tone[0].Period & 0x3F0) | (Data & 0x0F);
			else
				m_Tone[0].Period = (m_Tone[0].Period & 0x00F) | ((Data & 0x3F) << 4);
			break;

		case 2: /* Channel 2 period */
			if (Latch)
				m_Tone[1].Period = (m_Tone[1].Period & 0x3F0) | (Data & 0x0F);
			else
				m_Tone[1].Period = (m_Tone[1].Period & 0x00F) | ((Data & 0x3F) << 4);
			break;

		case 4: /* Channel 3 period */
			if (Latch)
				m_Tone[2].Period = (m_Tone[2].Period & 0x3F0) | (Data & 0x0F);
			else
				m_Tone[2].Period = (m_Tone[2].Period & 0x00F) | ((Data & 0x3F) << 4);

			if ((m_Noise.Control & 0x03) == 0x03)
			{
				/* Sync noise period with channel 3 period */
				m_Noise.Period = m_Tone[2].Period;
			}
			break;

		case 6: /* Noise control */
			m_Noise.Control = Data & 0x07;		/* Always update, fixes Micro Machines */
			m_Noise.LFSR	= LFSR_FEEDBACK;	/* Reset shift register to initial state */
			m_Noise.Output	= 0;

			switch (m_Noise.Control & 0x03)
			{						
				case 0x00: /* N / 512 */
					m_Noise.Period = 16;
					break;

				case 0x01: /* N / 1024 */
					m_Noise.Period = 32;
					break;

				case 0x02: /* N / 2048 */
					m_Noise.Period = 64;
					break;

				case 0x03: /* Sync noise period with channel 3 period */
					m_Noise.Period = m_Tone[2].Period;
					break;
			}
			break;

		case 1: /* Channel 1 volume */
			m_Tone[0].Volume = m_VolumeTable[Data & 0x0F];
			break;

		case 3: /* Channel 2 volume */
			m_Tone[1].Volume = m_VolumeTable[Data & 0x0F];
			break;

		case 5: /* Channel 3 volume */
			m_Tone[2].Volume = m_VolumeTable[Data & 0x0F];
			break;

		case 7: /* Noise volume */
			m_Noise.Volume = m_VolumeTable[Data & 0x0F];
			break;
	}
}

void SN76489::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*> &OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / ClockDivider;
	m_CyclesToDo = TotalCycles % ClockDivider;

	int16_t  Out;

	while (Samples != 0)
	{
		UpdateToneGenerators();
		UpdateNoiseGenerator();

		/* Mix channels */
		Out  = (m_Tone[0].Volume & m_Tone[0].FlipFlop);
		Out += (m_Tone[1].Volume & m_Tone[1].FlipFlop);
		Out += (m_Tone[2].Volume & m_Tone[2].FlipFlop);
		Out += (m_Noise.Volume & m_Noise.Output);

		/* Output sample to buffer */
		OutBuffer[0]->WriteSampleS16(Out);

		Samples--;
	}
}

inline void SN76489::UpdateToneGenerators()
{
	/*	The flipflop is used as a mask for the volume output
		Normally you would do:
		m_Tone[i].FlipFlop ^= 1;
	*/

	for (auto i = 0; i < 3; i++)
	{
		m_Tone[i].Counter -= 2; //FIXME: should be -= 1

		if (m_Tone[i].Counter <= 0)
		{
			/* Reload counter */
			m_Tone[i].Counter = m_Tone[i].Period;

			/* Toggle flip-flop */
			m_Tone[i].FlipFlop ^= 0xFFFF;
		}
	}
}

inline void SN76489::UpdateNoiseGenerator()
{
	uint32_t Seed;

	m_Noise.Counter -= 2; //FIXME: should be -= 1

	if (m_Noise.Counter <= 0)
	{
		/* Reload counter */
		m_Noise.Counter = m_Noise.Period;

		/* Update LFSR (only when flipflop transitioned from 0 to 1) */
		if (m_Noise.FlipFlop ^= 1)
		{
			/* Update volume output mask */
			m_Noise.Output = (m_Noise.LFSR & 1) ? 0xFFFF : 0;

			if (m_Noise.Control & 0x04) /* "White" Noise */
			{
				/* Tap bits 3 and 0 (XOR) */
				Seed = ((m_Noise.LFSR >> 3) ^ m_Noise.LFSR) & 0x0001;
			}
			else /* "Periodic" Noise */
			{
				/* Tap bit 0 */
				Seed = m_Noise.LFSR & 0x0001;
			}

			/* Shift LFSR and apply seed (16-bits wide) */
			m_Noise.LFSR = (m_Noise.LFSR >> 1) | (Seed << 15);
		}
	}
}
