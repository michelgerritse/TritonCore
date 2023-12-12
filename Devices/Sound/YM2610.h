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
#ifndef _YM2610_H_
#define _YM2610_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"
#include "AY.h"
#include "YM.h"

/* Yamaha YM2610 (OPNB) */
class YM2610 : public ISoundDevice, public IMemoryAccess
{
public:
	YM2610(uint32_t ClockSpeed = 8'000'000);
	~YM2610() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName();
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

	/* OPNB data type */
	struct opnb_t
	{
		YM::OPN::operator_t	Slot[16];
		YM::OPN::channel_t	Channel[4];
		YM::OPN::timer_t	TimerA;
		YM::OPN::timer_t	TimerB;
		YM::OPN::lfo_t		LFO;

		uint32_t	FnumLatch;			/* Fnum latch (3-bit) */
		uint32_t	FnumLatch3CH;		/* Fnum latch 3CH (3-bit) */
		uint32_t	BlockLatch;			/* Block latch (3-bit) */
		uint32_t	BlockLatch3CH;		/* Block latch 3CH (3-bit) */
		uint32_t	Fnum3CH[3];			/* 3CH Frequency Nr. (11-bit) */
		uint32_t	Block3CH[3];		/* 3CH Block (3-bit) */
		uint32_t	KeyCode3CH[3];		/* 3CH Key code (5-bit) */
		uint32_t	EgCounter;			/* EG counter (12-bit) */
		uint32_t	EgClock;			/* EG clock (/3 divisor) */
		uint32_t	Mode3CH;			/* 3CH Mode enable flag */
		uint32_t	ModeCSM;			/* CSM Mode enable flag */
		uint8_t		Status;				/* Status register (8-bit) */
		uint8_t		FlagCtrl;			/* Flag control register (8-bit) */
		uint8_t		IrqEnable;			/* IRQ enable flags */
		int16_t		OutL;				/* Accumulator ouput (L) */
		int16_t		OutR;				/* Accumulator ouput (R) */
	};

	uint8_t				m_AddressLatch;		/* Address latch (8-bit) */

	AY::ssg_t			m_SSG;				/* SSG unit */
	opnb_t				m_OPN;				/* OPNB unit */
	YM::adpcma_t		m_ADPCMA;			/* ADPCM-A unit */
	YM::adpcmb_t		m_ADPCMB;			/* ADPCM-B unit */

	std::array<uint8_t, 0x1000000>	m_MemoryADPCMA;	/* 16MB ADPCM-A memory */
	std::array<uint8_t, 0x1000000>	m_MemoryADPCMB;	/* 16MB ADPCM-B memory */

	uint32_t	m_ClockSpeed;
	uint32_t	m_CyclesToDoSSG;
	uint32_t	m_CyclesToDoOPN;

	void		WriteSSG(uint8_t Address, uint8_t Data);
	void		WriteADPCMA(uint8_t Address, uint8_t Data);
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
	void		ClearAccumulator();
	void		UpdateAccumulator(uint32_t SlotId);
	void		UpdateLFO();
	void		UpdateTimers();

	int16_t		GetModulation(uint32_t SlotId);
	uint8_t		CalculateRate(uint8_t Rate, uint8_t KeyCode, uint8_t KeyScale);
	void		ProcessKeyEvent(uint32_t SlotId);
	void		StartEnvelope(uint32_t SlotId);
};

#endif // !_YM2610_H_