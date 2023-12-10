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
#ifndef _YM2608_H_
#define _YM2608_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"
#include "AY.h"
#include "YM.h"

/* Yamaha YM2608 (OPNA) */
class YM2608 : public ISoundDevice, public IMemoryAccess
{
public:
	YM2608(uint32_t ClockSpeed = 8'000'000);
	~YM2608() = default;

	/* IDevice methods */
	const wchar_t* GetDeviceName();
	void			Reset(ResetType Type);
	void			SendExclusiveCommand(uint32_t Command, uint32_t Value);

	/* ISoundDevice methods */
	bool			EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	uint32_t		Read(int32_t Address);
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);

	/* IMemoryAccess methods */
	void			CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);

private:
	uint8_t			m_AddressLatch;		/* Address latch (8-bit) */

	uint32_t		m_PreScalerOPN;		/* OPN Prescaler (/6 /3 /2) */
	uint32_t		m_PreScalerSSG;		/* SSG Prescaler (/4 /2 /1) */

	AY::ssg_t		m_SSG;				/* SSG unit */
	YM::OPN::opna_t m_OPN;				/* OPNA unit */
	YM::adpcma_t	m_ADPCMA;			/* ADPCM-A unit */
	YM::adpcmb_t	m_ADPCMB;			/* ADPCM-B unit */
	
	uint32_t		m_RhythmChannels;	/* ADPCM-A channel alternating (4 / 6) */

	std::array<uint8_t, 0x200000>	m_MemoryADPCMB;	/* 2MB ADPCM-B memory */

	uint32_t	m_ClockSpeed;
	 int32_t	m_ClockADPCMA;
	 int32_t	m_ClockADPCMB;
	uint32_t	m_CyclesToDoSSG;
	uint32_t	m_CyclesToDoOPN;

	void		WriteSSG(uint8_t Address, uint8_t Data);
	void		WriteRSS(uint8_t Address, uint8_t Data);
	void		WriteADPCMB(uint8_t Address, uint8_t Data);
	void		WriteMode(uint8_t Address, uint8_t Data);
	void		WriteFM(uint8_t Address, uint8_t Port, uint8_t Data);

	void		SetStatusFlags(uint8_t Flags);
	void		ClearStatusFlags(uint8_t Flags);

	void		UpdateSSG(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);
	void		UpdateOPN(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);
	
	void		UpdateADPCMA();
	void		UpdateADPCMB();

	void		PrepareSlot(uint32_t SlotId);
	void		UpdatePhaseGenerator(uint32_t SlotId);
	void		UpdateEnvelopeGenerator(uint32_t SlotId);
	void		UpdateOperatorUnit(uint32_t SlotId);
	void		UpdateAccumulator(uint32_t SlotId);
	void		UpdateLFO();
	void		UpdateTimers();

	int16_t		GetModulation(uint32_t SlotId);
	uint8_t		CalculateRate(uint8_t Rate, uint8_t KeyCode, uint8_t KeyScale);
	void		ProcessKeyEvent(uint32_t SlotId);
	void		StartEnvelope(uint32_t SlotId);
};

#endif // !_YM2608_H_