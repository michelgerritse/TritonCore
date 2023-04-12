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
#ifndef _MSM6295_H_
#define _MSM6295_H_

#include "Interfaces/ISoundDevice.h"
#include "Interfaces/IMemoryAccess.h"

/* Oki MSM6295 4-channel mixing ADPCM voice synthesis LSI */
class MSM6295 : public ISoundDevice, public IMemoryAccess
{
public:
	MSM6295(bool PinSS = false);
	~MSM6295() = default;

	/* IDevice methods */
	const wchar_t*  GetDeviceName();
	void			Reset(ResetType Type);
	void			SendExclusiveCommand(uint32_t Command, uint32_t Value);

	/* ISoundDevice methods */
	bool			EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);

	/* IMemoryAccess methods */
	void			CopyToMemory(size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(size_t Offset, uint8_t* Data, size_t Size);

private:
	struct CHANNEL
	{
		uint32_t On;			/* Channel playing = 1, suspended = 0 */
		uint32_t End;			/* End address of speech data (18-bit) */
		uint32_t Addr;			/* Current address of speech data (18-bit) */
		uint8_t	 Volume;		/* Volume (9 defined values) */

		int16_t	 Signal;		/* Decoded ADPCM signal (12-bit) */
		int32_t  Step;			/* ADPCM step */
		uint32_t NibbleShift;	/* Nibble selection shift */
	};

	CHANNEL		m_Channel[4];
	uint32_t	m_PhraseLatch;	/* Selected phrase (1 - 127) */
	uint32_t	m_NextByte;		/* 1st or 2nd data byte */

	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	std::vector<uint8_t> m_Memory;

	static const uint8_t s_VolumeTable[16];

	void LoadPhrase(uint32_t Index, uint32_t Phrase, uint32_t AttnIndex);
};

#endif // !_MSM6295_H_