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
#ifndef _SEGA_PCM_H_
#define _SEGA_PCM_H_

#include "Interfaces/ISoundDevice.h"
#include "Interfaces/IMemoryAccess.h"

/* SegaPCM (315-5218) */
class SegaPCM : public ISoundDevice, public IMemoryAccess
{
public:
	SegaPCM(uint32_t Flags = 0);
	~SegaPCM() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName();
	void			Reset(ResetType Type);

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
	/* PCM Channel */
	struct CHANNEL
	{
		uint32_t	ON;			/* Channel On / Off flag */
		uint32_t	LOOP;		/* Loop On / Off flag */
		uint32_t	ADDR;		/* Current memory address (16.8 fixed point) */
		uint32_t	BANK;		/* Current memory bank (6-bit) */
		uint8_t		PANL;		/* Panpot (L) (7-bit) */
		uint8_t		PANR;		/* Panpot (R) (7-bit) */
		uint32_t	FD;			/* Frequency Delta (8-bit) */
		uint32_t	LS;			/* Loop Start address (16-bit) */
		uint32_t	END;		/* End address (16-bit) */
	};

	CHANNEL		m_Channel[16];
	uint32_t	m_Shift;		/* 16.8 fixed point shit */
	uint32_t	m_OutputMask;	/* 10 / 12 bit DAC output mask */
	uint32_t	m_BankShift;
	uint32_t	m_BankMask;

	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	std::vector<uint8_t> m_Memory;
};

#endif // !_SEGA_PCM_H_