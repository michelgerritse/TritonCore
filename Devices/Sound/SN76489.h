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
#ifndef _SN76489_CORE_H_
#define _SN76489_CORE_H_

#include "../../Interfaces/ISoundDevice.h"

/*
	Supported SN76489 family (and clones) overview:

					|	1	|	2	|	3	|	4	|	5	|	6	|	7	|	8
	----------------+-------+-------+-------+-------+-------+-------+-------+-------+
	SN76489(N)		|	15		1		0	   XOR		N		Y		N		Y
	SN76489A(N)		|	17		3		2	   XOR		N		N		N		Y
					|					|
	Sega PSG		|	16		3		0	   XOR		Y		Y		N		Y
	Sega GG PSG		|	16		3		0	   XOR		Y		Y		Y		Y
					|
	NCR 8496		|	16		5		1	   XNOR		N		Y?		N		Y
	PSSJ-3			|	16		5		1	   XNOR		N		N?		N		Y

	1 = LFSR bit width
	2 = Tapped bit 1
	3 = Tapped bit 2
	4 = Logical operation
	5 = Period of 0 equals 0 (otherwise 0x400)
	6 = Inverse output
	7 = Stereo support
	8 = Additional /8 clock divider

	Note: The T6W28 (used in the Neo Geo Pocket) is not included. It's basically a dual SN76489 in a single package

*/

namespace SNPSG
{
	/* Texas Instruments SN76489 (family / clone) sound generator */
	template<
		//const wchar_t* Name,
		uint32_t Width,
		uint32_t Tap1,
		uint32_t Tap2,
		bool UseXOR,
		bool AllowZeroPeriod,
		bool Inverted,
		bool IsStereo,
		uint32_t Divider
	>
	class Core : public ISoundDevice
	{
	public:
		/* Constructor */
		Core() :
			m_ClockDivider(Divider)
		{
			//double Volume = 32767.0 / 8.0; /* This needs validation */

			/*
			Each channel outputs 0.8V for a total of ~3.3V. We map this to a 5.0V max output
			https://scarybeastsecurity.blogspot.com/2020/06/sampled-sound-1980s-style-from-sn76489.html
			*/

			double Volume = ((32767 * 3.3) / 5.0) / 4;

			for (auto i = 0; i < 15; i++)
			{
				if constexpr (Inverted) m_VolumeTable[i] = (int16_t)-Volume;
				else m_VolumeTable[i] = (int16_t)Volume;
				
				Volume /= 1.258925412; /* 2dB drop per step (10 ^ (2/20)) */
			}
			m_VolumeTable[15] = 0; /* OFF (full attenuation) */

			/* Reset device to initial state */
			Reset(ResetType::PowerOnDefaults);
		}

		/* Destructor */
		~Core() = default;

		/* IDevice methods */
		const wchar_t* GetDeviceName()
		{
			//TODO: is it possible to pass the device name as a template argument?
			return L"Texas Instruments SN76489 (family/clone)";
		}

		void Reset(ResetType Type)
		{
			m_CyclesToDo = 0;

			m_Register = 3; /* Channel 2 volume register is latched on reset */
			if constexpr (IsStereo) m_StereoMask = 0xFF;

			/* Reset tone generators */
			for (auto i = 0; i < 3; i++)
			{
				m_Tone[i].Counter = 0;
				m_Tone[i].Period = 0;
				m_Tone[i].FlipFlop = 0xFFFF;
				m_Tone[i].Volume = m_VolumeTable[15];
			}

			/* Reset noise generator */
			m_Noise.Counter = 0;
			m_Noise.FlipFlop = 1;
			m_Noise.Control = 0;
			m_Noise.Period = 16;
			m_Noise.LFSR = 1 << (Width - 1);
			m_Noise.Volume = m_VolumeTable[15];
			m_Noise.Output = 0;

			m_SampleHack = 0; // FIXME
		}

		void SendExclusiveCommand(uint32_t Command, uint32_t Value)
		{
			if constexpr (IsStereo)
			{
				if (Command == 0x06)
				{
					m_StereoMask = Value & 0xFF;
				}
			}
		}

		/* ISoundDevice methods */
		bool EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
		{
			if (OutputNr == 0)
			{
				Desc.SampleRate		= (m_ClockSpeed / m_ClockDivider) / 2; // FIXME
				Desc.SampleFormat	= 0; //TODO
				Desc.Channels		= IsStereo ? 2 : 1;
				Desc.ChannelMask	= IsStereo ? (SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT) : SPEAKER_FRONT_CENTER;
				Desc.Description	= L"";

				return true;
			}

			return false;
		}

		void SetClockSpeed(uint32_t ClockSpeed)
		{
			m_ClockSpeed = ClockSpeed;
		}

		uint32_t GetClockSpeed()
		{
			return m_ClockSpeed;
		}

		void Write(uint32_t Address, uint32_t Data)
		{
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
				m_Noise.LFSR = 1 << (Width - 1);	/* Reset shift register to initial state */
				m_Noise.Output = 0;					/* Reset noise output */

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
				m_Tone[0].Volume = m_VolumeTable[Data & 0x0F];
				break;

			case 3: /* Tone 2 attenuation */
				m_Tone[1].Volume = m_VolumeTable[Data & 0x0F];
				break;

			case 5: /* Tone 3 attenuation */
				m_Tone[2].Volume = m_VolumeTable[Data & 0x0F];
				break;

			case 7: /* Noise attenuation */
				m_Noise.Volume = m_VolumeTable[Data & 0x0F];
				break;
			}
		}

		void Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
		{
			uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
			uint32_t Samples = TotalCycles / m_ClockDivider;
			m_CyclesToDo = TotalCycles % m_ClockDivider;

			if constexpr (IsStereo)
			{
				UpdateStereo(Samples, OutBuffer);
			}
			else
			{
				UpdateMono(Samples, OutBuffer);
			}
		}

	private:

		void UpdateMono(uint32_t Samples, std::vector<IAudioBuffer*>& OutBuffer)
		{
			int16_t  Out;

			while (Samples != 0)
			{
				UpdateToneGenerators();
				UpdateNoiseGenerator();

				/* Mix channels */
				if (!(m_SampleHack ^= 1)) //FIXME
				{
					Out = (m_Tone[0].Volume & m_Tone[0].FlipFlop);
					Out += (m_Tone[1].Volume & m_Tone[1].FlipFlop);
					Out += (m_Tone[2].Volume & m_Tone[2].FlipFlop);
					Out += (m_Noise.Volume & m_Noise.Output);

					/* Output sample to buffer */
					OutBuffer[0]->WriteSampleS16(Out);
				}

				Samples--;
			}
		}

		void UpdateStereo(uint32_t Samples, std::vector<IAudioBuffer*>& OutBuffer)
		{
			int16_t OutL;
			int16_t OutR;

			while (Samples != 0)
			{
				OutL = OutR = 0;

				UpdateToneGenerators();
				UpdateNoiseGenerator();

				/* Mix channels */
				if (!(m_SampleHack ^= 1)) //FIXME
				{
					if (m_StereoMask & 0x10) OutL += (m_Tone[0].Volume & m_Tone[0].FlipFlop);
					if (m_StereoMask & 0x20) OutL += (m_Tone[1].Volume & m_Tone[1].FlipFlop);
					if (m_StereoMask & 0x40) OutL += (m_Tone[2].Volume & m_Tone[2].FlipFlop);
					if (m_StereoMask & 0x80) OutL += (m_Noise.Volume & m_Noise.Output);

					if (m_StereoMask & 0x01) OutR += (m_Tone[0].Volume & m_Tone[0].FlipFlop);
					if (m_StereoMask & 0x02) OutR += (m_Tone[1].Volume & m_Tone[1].FlipFlop);
					if (m_StereoMask & 0x04) OutR += (m_Tone[2].Volume & m_Tone[2].FlipFlop);
					if (m_StereoMask & 0x08) OutR += (m_Noise.Volume & m_Noise.Output);

					/* Output samples to buffer */
					OutBuffer[0]->WriteSampleS16(OutL);
					OutBuffer[0]->WriteSampleS16(OutR);
				}

				Samples--;
			}
		}

		void UpdateToneGenerators()
		{
			/*	Note: The flipflop is used as a mask over the volume output */

			for (auto i = 0; i < 3; i++)
			{
				if constexpr (AllowZeroPeriod) m_Tone[i].Counter--;
				else m_Tone[i].Counter = (m_Tone[i].Counter - 1) & 0x3FF;

				if (m_Tone[i].Counter <= 0)
				{
					/* Reload counter */
					m_Tone[i].Counter = m_Tone[i].Period;

					/* Toggle flip-flop */
					m_Tone[i].FlipFlop ^= 0xFFFF;
				}
			}
		}

		void UpdateNoiseGenerator()
		{
			if constexpr (AllowZeroPeriod) m_Noise.Counter--;
			else m_Noise.Counter = (m_Noise.Counter - 1) & 0x3FF;

			if (m_Noise.Counter <= 0)
			{
				/* Reload counter */
				m_Noise.Counter = m_Noise.Period;

				/* Update LFSR (only when flipflop transitioned from 0 to 1) */
				if (m_Noise.FlipFlop ^= 1)
				{
					/* Update volume output mask */
					//m_Noise.Output = (m_Noise.LFSR & 1) ? 0xFFFF : 0;
					m_Noise.Output = ~((m_Noise.LFSR & 1) - 1);

					uint32_t Feedback;
					uint32_t BitTap1 = (m_Noise.Control & 0x04) ? (m_Noise.LFSR >> Tap1) : 0; /* Tap1 is always 0 for periodic noise */
					uint32_t BitTap2 = m_Noise.LFSR >> Tap2;

					if constexpr (UseXOR) /* XOR */
					{
						Feedback = (BitTap1 ^ BitTap2) & 1;
					}
					else /* XNOR */
					{
						Feedback = (~(BitTap1 ^ BitTap2)) & 1;
					}

					/* Shift LFSR right and apply feedback */
					m_Noise.LFSR = (m_Noise.LFSR >> 1) | (Feedback << (Width - 1));
				}
			}
		}

		struct NOISE
		{
			int32_t		Counter;		/* Noise counter */
			uint32_t	Period;			/* Noise half-period */
			uint32_t	LFSR;			/* Linear Feedback Shift Register (16-bit) */
			uint32_t	FlipFlop;		/* Output flip-flop */
			uint32_t	Control;		/* Noise control register */
			int16_t		Volume;			/* Noise output volume */
			uint32_t	Output;			/* Volume ouput mask */
		};

		struct TONE
		{
			int32_t		Counter;		/* Tone counter */
			uint32_t	Period;			/* Tone half-period (10-bit) */
			uint16_t	FlipFlop;		/* Output flip-flop */
			int16_t		Volume;			/* Tone output volume */
		};

		uint32_t	m_Register;			/* Current latched register */
		uint8_t		m_StereoMask;		/* Game Gear stereo mask */
		TONE		m_Tone[3];			/* Tone channels */
		NOISE		m_Noise;			/* Noise channel */

		int16_t		m_VolumeTable[16];	/* Non-linear volume table */

		uint32_t	m_ClockSpeed;
		uint32_t	m_ClockDivider;
		uint32_t	m_CyclesToDo;
		uint32_t	m_SampleHack; //FIXME
	};
} // namespace SNPSG

using SN76489	= SNPSG::Core<15, 1, 0, true,  false, true,  false, 16>;
using SN76489A	= SNPSG::Core<17, 3, 2, true,  false, false, false, 16>;
using SEGAPSG	= SNPSG::Core<16, 3, 0, true,  true,  true,  false, 16>;
using SEGAPSG2	= SNPSG::Core<16, 3, 0, true,  true,  true,  true,  16>; /* Game Gear version */
using NCR8496	= SNPSG::Core<16, 5, 1, false, false, true,  false, 16>;
using PSSJ3		= SNPSG::Core<16, 5, 1, false, false, false, false, 16>;

#endif // !_SN76489_CORE_H_