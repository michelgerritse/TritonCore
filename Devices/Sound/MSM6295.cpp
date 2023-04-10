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
#include "MSM6295.h"
#include "ADPCM.h"

/* 
	The Oki MSM6295 is a 4-channel mixing ADPCM voice synthesis chip:
	- 4 channels using Dialogic ADPCM encoded 4-bit samples
	- 12-bit built-in DAC
	- 9 attenuation steps (0dB - -24dB, -3dB/step)
	- 8-bit controller bus
	- 8-bit memory data bus
	- 18-bit memory address bus
	- 1MHz to 5MHz clock input
	- Clock select pin (/132 or /165)

	This LSI comes in 2 different packages:
	- 44-pin QFP (QFP44-P-910-V1K or QFP44-P-910-K)
	- 42-pin DIP (DIP42-P-600)

	The 42-pin version can only address 128KB of external memory (it lacks A17)

	Things that need validation:
	- Is it really impossible to select phrase 0? The manual states that only phrase 1 - 127 are valid.
	- What happens when you try to load a phrase on multiple channels at once ? The manual states that you can
	  only select 1 channel at a time.
	- 9 attenuation levels are specified (0 - 8), what happens when you set it to 9 - 15 ?
*/

const float MSM6295::s_AttnTable[16] =
{
	/* Attenuation table:

	The following 9 values are defined by the manual:

	0 =   0.0dB	
	1 =  -3.2dB	
	2 =  -6.0dB	
	3 =  -9.2dB
	4 = -12.0dB	
	5 = -14.5dB	
	6 = -18.0dB	
	7 = -20.5dB
	8 = -24.0dB	
	9 - 15 = silence (assumption)

	Formula used to calculate the attenuation levels:
	dB = 10 ^ (value / 20)
	
	*/

	1.0000000f, 0.6918310f, 0.5011872f, 0.3467369f,
	0.2511886f, 0.1883649f, 0.1258925f, 0.0944061f,
	0.0630957f, 0.0000000f, 0.0000000f, 0.0000000f,
	0.0000000f, 0.0000000f, 0.0000000f, 0.0000000f
};

MSM6295::MSM6295(bool PinSS) :
	m_ClockDivider(PinSS ? 132 : 165)
{
	/* Initialize ADPCM decoder */
	OKI::ADPCM::InitDecoder();
	
	/* Set memory size to 256KB */
	m_Memory.resize(0x40000);

	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* MSM6295::GetDeviceName()
{
	return L"Oki MSM6295";
}

void MSM6295::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	m_NextByte = 0; /* Start at 1st byte */
	m_PhraseLatch = 0;

	/* Reset all channels */
	memset(m_Channel, 0, sizeof(m_Channel));

	if (Type == ResetType::PowerOnDefaults)
	{
		/* Clear PCM memory */
		memset(m_Memory.data(), 0, m_Memory.size());
	}
}

void MSM6295::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	/* No implementation needed */
}

bool MSM6295::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == 0)
	{
		Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 1;
		Desc.ChannelMask = SPEAKER_FRONT_CENTER;
		Desc.Description = L"Dialogic ADPCM";

		return true;
	}

	return false;
}

void MSM6295::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t MSM6295::GetClockSpeed()
{
	return m_ClockSpeed;
}

void MSM6295::Write(uint32_t Address, uint32_t Data)
{
	/*
	Write order of data:
		- if data bit7 = 1: we expect 2 bytes: 1st byte selects the phrase, 2nd byte selects channel and attenuation
		- if data bit7 = 0: we expect 1 byte, which will suspend the selected channels
	*/

	if (m_NextByte) /* Channel and attenuation selection (2nd byte) */
	{
		uint32_t AttnIndex = Data & 0x0F;

		assert(TritonCore::GetParity(Data & 0xF0) == 1); /* Only 1 channel can be selected according to the manual */

		if (Data & 0x10) LoadPhrase(0, m_PhraseLatch, AttnIndex);
		if (Data & 0x20) LoadPhrase(1, m_PhraseLatch, AttnIndex);
		if (Data & 0x40) LoadPhrase(2, m_PhraseLatch, AttnIndex);
		if (Data & 0x80) LoadPhrase(3, m_PhraseLatch, AttnIndex);

		m_NextByte = 0;
	}
	else
	{
		if (Data & 0x80) /* Phrase selection (1st byte) */
		{
			m_PhraseLatch = Data & 0x7F;
			m_NextByte = 1;
		}
		else /* Channel suspension */
		{
			if (Data & 0x08) m_Channel[0].On = 0;
			if (Data & 0x10) m_Channel[1].On = 0;
			if (Data & 0x20) m_Channel[2].On = 0;
			if (Data & 0x40) m_Channel[3].On = 0;
		}
	}
}

void MSM6295::LoadPhrase(uint32_t Index, uint32_t Phrase, uint32_t AttnIndex)
{
	auto& Channel = m_Channel[Index];

	if (Phrase == 0) return; /* Invalid phrase selection, only values 1 - 127 are valid. Some games still incorrectly set this */

	if (!Channel.On)
	{
		uint32_t Offset = Phrase << 3; /* Each phrase header is 8 bytes */
		uint32_t Start = (m_Memory[Offset + 0] << 16) | (m_Memory[Offset + 1] << 8) | (m_Memory[Offset + 2] << 0);
		uint32_t End   = (m_Memory[Offset + 3] << 16) | (m_Memory[Offset + 4] << 8) | (m_Memory[Offset + 5] << 0);
		
		/* Start channel */
		Channel.On = 1;
		Channel.Addr = Start & 0x3FFFF;
		Channel.End = (End + 1) & 0x3FFFF; /* Inclusive */
		Channel.Attn = s_AttnTable[AttnIndex];

		/* Reset decoder state */
		Channel.Signal = 0;
		Channel.Step = 0;
		Channel.NibbleShift = 4; /* Start with high order nibble */
	}
}

void MSM6295::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int16_t Out;

	while (Samples != 0)
	{
		Out = 0;

		for (auto &Channel : m_Channel)
		{
			if (Channel.On)
			{
				/* Load nibble from memory */
				uint8_t Nibble = (m_Memory[Channel.Addr] >> Channel.NibbleShift) & 0x0F;

				/* Alternate between 1st and 2nd nibble */
				Channel.NibbleShift ^= 4;

				/* Increase address counter (do this after the ^= 4) */
				Channel.Addr += (Channel.NibbleShift >> 2);

				/* Check for end of phrase */
				if (Channel.Addr == Channel.End) Channel.On = 0;

				/* Decode ADPCM nibble */
				OKI::ADPCM::Decode(Nibble, &Channel.Step, &Channel.Signal);

				/* Apply attenuation and accumulate */
				Out += (int16_t) (Channel.Signal * Channel.Attn * 4);
			}
		}

		OutBuffer[0]->WriteSampleS16(Out);

		Samples--;
	}
}

void MSM6295::CopyToMemory(size_t Offset, uint8_t* Data, size_t Size)
{
	if ((Offset + Size) > m_Memory.size()) return;

	memcpy(m_Memory.data() + Offset, Data, Size);
}

void MSM6295::CopyToMemoryIndirect(size_t Offset, uint8_t* Data, size_t Size)
{
	/* No specialized implementation needed */
	CopyToMemory(Offset, Data, Size);
}