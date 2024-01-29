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
#ifndef _YM2612_H_
#define _YM2612_H_

#include "../../Interfaces/ISoundDevice.h"
#include "YM_OPN.h"

/* Yamaha YM2612 (OPN2) */
class YM2612 : public ISoundDevice
{
public:
	YM2612(uint32_t ClockSpeed = 8'000'000);
	~YM2612() = default;

	/* IDevice methods */
	const wchar_t*  GetDeviceName();
	void			Reset(ResetType Type);
	void			SendExclusiveCommand(uint32_t Command, uint32_t Value);

	/* ISoundDevice methods */
	bool			EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	uint32_t		Read(uint32_t Address);
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);

private:

	/* OPN2 data type */
	struct opn2_t
	{
		YM::OPN::operator_t	Slot[24];
		YM::OPN::channel_t	Channel[6];
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
		uint32_t	DacSelect;			/* DAC Select flag */
		int16_t		DacData;			/* DAC Data (9-bit) */
		int32_t		OutL;				/* Accumulator output (L) */
		int32_t		OutR;				/* Accumulator output (R) */
	};

	uint8_t		m_AddressLatch;		/* Address latch (8-bit) */
	uint8_t		m_PortLatch;		/* Port latch (1-bit) */

	opn2_t		m_OPN;				/* OPN2 unit */
	
	uint32_t	m_ClockSpeed;
	uint32_t	m_CyclesToDo;

	void		WriteMode(uint8_t Register, uint8_t Data);
	void		WriteFM(uint8_t Register, uint8_t Port, uint8_t Data);
	void		SetStatusFlags(uint8_t Flags);
	void		ClearStatusFlags(uint8_t Flags);

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

#endif // !_YM2612_H_