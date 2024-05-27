/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023 - 2024, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#ifndef _SEGA_PCM_H_
#define _SEGA_PCM_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"

/* SegaPCM (315-5218) */
class SegaPCM : public ISoundDevice, public IMemoryAccess
{
public:
	SegaPCM(uint32_t ClockSpeed = 16'000'000, uint32_t BankFlags = 0);
	~SegaPCM() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName();
	void			Reset(ResetType Type);
	void			SendExclusiveCommand(uint32_t Command, uint32_t Value);

	/* ISoundDevice methods */
	bool			EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);

	/* IMemoryAccess methods */
	void			CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);

private:
	static const std::wstring s_DeviceName;

	/* PCM Channel */
	struct channel_t
	{
		uint32_t	On;			/* Channel enable flag */
		uint32_t	Loop;		/* Loop enable flag */
		uint32_t	Bank;		/* Memory bank (6-bit) */
		uint8_t		PanL;		/* Panpot (L) (7-bit) */
		uint8_t		PanR;		/* Panpot (R) (7-bit) */
		uint32_t	Delta;		/* Frequency delta (8-bit) */
		pair32_t	Addr;		/* Current memory address (24-bit: 16.8) */
		pair32_t	LoopAddr;	/* Loop address (16-bit) */
		uint32_t	StopAddr;	/* Stop address (16-bit) */
	};

	channel_t	m_Channel[16];
	uint32_t	m_BankShift;
	uint32_t	m_BankMask;

	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	std::vector<uint8_t> m_Memory;
};

#endif // !_SEGA_PCM_H_