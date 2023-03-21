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
	  To work around this only odd samples will be output, effectively halving the sample rate
*/

static int16_t s_VolumeTable[16];	/* Non-linear volume table */

void BuildTables()
{
	static bool Initialized = false;

	if (!Initialized)
	{
		double Volume = 32767.0 / 8.0; /* This needs validation */

		for (auto i = 0; i < 15; i++)
		{
			s_VolumeTable[i] = (int16_t)Volume;
			Volume /= 1.258925412; /* 2dB drop per step (10 ^ (2/20)) */
		}
		s_VolumeTable[15] = 0; /* OFF (full attenuation) */

		Initialized = true;
	}
}

SN76489::SN76489(uint32_t Model, uint32_t Flags) :
	m_Model(Model),
	m_Flags(Flags),
	m_LFSRDefault(0x8000),
	m_ClockSpeed(4000000),
	m_ClockDivider(16)
{
	/* Build any table required for this device */
	BuildTables();

	/* Reset device to initial state */
	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* SN76489::GetDeviceName()
{
	return L"Texas Instruments SN76489";
}

void SN76489::Reset(ResetType Type)
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
		m_Tone[i].Volume	= s_VolumeTable[15];
	}

	/* Reset noise generator */
	m_Noise.Counter	= 0;
	m_Noise.FlipFlop = 1;
	m_Noise.Control	= 0;
	m_Noise.Period	= 16;
	m_Noise.LFSR	= m_LFSRDefault;
	m_Noise.Volume	= s_VolumeTable[15];
	m_Noise.Output	= 0;

	m_SampleHack = 0; // FIXME: see notes section
}

uint32_t SN76489::GetOutputCount()
{
	return 1;
}

uint32_t SN76489::GetSampleRate(uint32_t ID)
{
	return (m_ClockSpeed / m_ClockDivider) / 2; // FIXME: See notes section
}

uint32_t SN76489::GetSampleFormat(uint32_t ID)
{
	return 0;
}

uint32_t SN76489::GetChannelMask(uint32_t ID)
{
	return SPEAKER_FRONT_CENTER;;
}

const wchar_t* SN76489::GetOutputName(uint32_t ID)
{
	return L"Test";
}

void SN76489::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t SN76489::GetClockSpeed()
{
	return m_ClockSpeed;
}

void SN76489::Write(uint32_t Address, uint32_t Data)
{
	if (Address == 0x01) /* Might change this to 0x06 to match the port on the Game Gear */
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
		case 0: /* Tone 1 period */
			if (Latch)
				m_Tone[0].Period = (m_Tone[0].Period & 0x3F0) | (Data & 0x0F);
			else
				m_Tone[0].Period = (m_Tone[0].Period & 0x00F) | ((Data & 0x3F) << 4);
			break;

		case 2: /* Tone 2 period */
			if (Latch)
				m_Tone[1].Period = (m_Tone[1].Period & 0x3F0) | (Data & 0x0F);
			else
				m_Tone[1].Period = (m_Tone[1].Period & 0x00F) | ((Data & 0x3F) << 4);
			break;

		case 4: /* Tone 3 period */
			if (Latch)
				m_Tone[2].Period = (m_Tone[2].Period & 0x3F0) | (Data & 0x0F);
			else
				m_Tone[2].Period = (m_Tone[2].Period & 0x00F) | ((Data & 0x3F) << 4);

			if ((m_Noise.Control & 0x03) == 0x03)
			{
				/* Sync noise period with tone 3 period */
				m_Noise.Period = m_Tone[2].Period;
			}
			break;

		case 6: /* Noise control */
			m_Noise.Control = Data & 0x07;		/* Always update, fixes Micro Machines */
			m_Noise.LFSR	= m_LFSRDefault;	/* Reset shift register to initial state */
			m_Noise.Output	= 0;				/* Reset noise output */

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

		case 1: /* Tone 1 attenuation */
			m_Tone[0].Volume = s_VolumeTable[Data & 0x0F];
			break;

		case 3: /* Tone 2 attenuation */
			m_Tone[1].Volume = s_VolumeTable[Data & 0x0F];
			break;

		case 5: /* Tone 3 attenuation */
			m_Tone[2].Volume = s_VolumeTable[Data & 0x0F];
			break;

		case 7: /* Noise attenuation */
			m_Noise.Volume = s_VolumeTable[Data & 0x0F];
			break;
	}
}

void SN76489::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*> &OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int16_t  Out;

	while (Samples != 0)
	{
		UpdateToneGenerators();
		UpdateNoiseGenerator();

		/* Mix channels */
		if (!(m_SampleHack ^= 1)) //FIXME: See notes section
		{
			Out =  (m_Tone[0].Volume & m_Tone[0].FlipFlop);
			Out += (m_Tone[1].Volume & m_Tone[1].FlipFlop);
			Out += (m_Tone[2].Volume & m_Tone[2].FlipFlop);
			Out += (m_Noise.Volume & m_Noise.Output);

			/* Output sample to buffer */
			OutBuffer[0]->WriteSampleS16(Out);
		}

		Samples--;
	}
}

void SN76489::UpdateToneGenerators()
{
	/*	Note: The flipflop is used as a mask over the volume output */

	for (auto i = 0; i < 3; i++)
	{
		if (--m_Tone[i].Counter <= 0)
		{
			/* Reload counter */
			m_Tone[i].Counter = m_Tone[i].Period;

			/* Toggle flip-flop */
			m_Tone[i].FlipFlop ^= 0xFFFF;
		}
	}
}

void SN76489::UpdateNoiseGenerator()
{
	uint32_t Seed;

	if (--m_Noise.Counter <= 0)
	{
		/* Reload counter */
		m_Noise.Counter = m_Noise.Period;

		/* Update LFSR (only when flipflop transitioned from 0 to 1) */
		if (m_Noise.FlipFlop ^= 1)
		{
			/* Update volume output mask */
			m_Noise.Output = (m_Noise.LFSR & 1) ? 0xFFFF : 0;

			if (m_Noise.Control & 0x04) /* "White" noise */
			{
				/* Tap bits 3 and 0 (XOR) */
				Seed = ((m_Noise.LFSR >> 3) ^ (m_Noise.LFSR >> 0)) & 0x0001;
			}
			else /* "Periodic" noise */
			{
				/* Tap bit 0 only */
				Seed = m_Noise.LFSR & 0x0001;
			}

			/* Shift LFSR and apply seed (16-bit wide) */
			m_Noise.LFSR = (m_Noise.LFSR >> 1) | (Seed << 15);
		}
	}
}
