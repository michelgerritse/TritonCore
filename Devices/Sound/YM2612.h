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

/* Yamaha YM2612 (OPN2) */
class YM2612 : public ISoundDevice
{
public:
	YM2612(uint32_t ClockSpeed = 8000000);
	~YM2612() = default;

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

private:

	/* Envelope phases */
	enum ADSR : uint32_t
	{
		Attack = 0,
		Decay,
		Sustain,
		Release
	};

	struct SLOT
	{
		uint32_t	KeyOn;		/* Key On / Off state */
		uint32_t	KeyLatch;	/* Latched Key On / Off flag */
		uint32_t	CsmKeyLatch;/* Latched CSM Key On / Off flag */
		
		uint32_t	FNum;		/* Frequency Nr. (11-bit) */
		uint32_t	Block;		/* Block (3-bit) */
		uint32_t	KeyCode;	/* Key code (5-bit) */
		
		uint32_t	Detune;		/* Detune (3-bit) */
		uint32_t	Multi;		/* Multiplier (4-bit) */
		uint32_t	TotalLevel;	/* Total Level (7-bit) */
		uint32_t	KeyScale;	/* Key Scale (2-bit) */
		uint16_t	SustainLvl;	/* Sustain Level (4-bit) */
		uint32_t	AmOn;		/* AM LFO On flag */
		
		uint32_t	SsgEnable;	/* SSG-EG Enable flag */
		uint32_t	SsgEgInv;	/* SSG-EG Inversion mode flag */
		uint32_t	SsgEgAlt;	/* SSG-EG Alternate mode flag */
		uint32_t	SsgEgHld;	/* SSG-EG Hold mode flag */
		uint32_t	SsgEgInvOut;/* SSG-EG Inverted output flag */

		uint32_t	EgRate[4];	/* Envelope rates (5-bit) */
		uint32_t	EgPhase;	/* Envelope phase */
		uint16_t	EgLevel;	/* Envelope internal level (10-bit) */
		uint16_t	EgOutput;	/* Envelope output (12-bit) */

		uint32_t	PgPhase;	/* Phase Counter (20-bit) */

		int16_t		Output[2];	/* Slot output (14-bit) */
	};

	struct CHANNEL
	{
		uint32_t	FNum;		/* Frequency Nr. (11-bit) */
		uint32_t	Block;		/* Block (3-bit) */
		uint32_t	KeyCode;	/* Key code (5-bit) */
		uint32_t	Algo;		/* Algorithm (3-bit) */
		uint32_t	AMS;		/* AM Sensitivity (2-bit) */
		uint32_t	PMS;		/* PM Sensitivity (3-bit) */
		uint32_t	FB;			/* Feedback (3-bit) */
		uint32_t	MaskL;		/* Channel L output mask */
		uint32_t	MaskR;		/* Channel R output mask */
		int16_t		Output;		/* Channel output (14-bit) */
	};

	SLOT		m_Slot[24];
	CHANNEL		m_Channel[6];

	uint8_t		m_AddressLatch;		/* Address latch (8-bit) */
	uint8_t		m_PortLatch;		/* Port latch (1-bit) */
	uint8_t		m_Status;			/* Status Register (8-bit) */
	uint32_t	m_Cycle;			/* Current clock cycle */

	uint8_t		m_FNumLatch;		/* Fnum2 / Block latch (6-bit) */
	uint8_t		m_3ChFNumLatch;		/* 3CH Fnum2 / Block latch mode (6-bit) */
	uint32_t	m_3ChFNum[3];		/* 3CH Frequency Nr. (11+1-bit) */
	uint32_t	m_3ChBlock[3];		/* 3CH Block (3-bit) */
	uint32_t	m_3ChKeyCode[3];	/* 3CH Key code (5-bit) */
	
	uint32_t	m_3ChMode;			/* 3CH Mode enable flag */
	uint32_t	m_CsmMode;			/* CSM Mode enable flag */
	
	uint32_t	m_EgCounter;		/* EG counter (12-bit) */
	uint32_t	m_EgClock;			/* EG clock */
	uint32_t	m_EgClockInc;		/* EG clock increment */

	uint32_t	m_TimerA;			/* Timer A Register (10-bit) */
	uint32_t	m_TimerB;			/* Timer B Register (8-bit) */
	uint32_t	m_TimerAEnable;		/* Timer A Enable flag */
	uint32_t	m_TimerBEnable;		/* Timer B Enable flag */
	uint32_t	m_TimerALoad;		/* Timer A Load flag */
	uint32_t	m_TimerBLoad;		/* Timer B Load flag */
	uint32_t	m_TimerACount;		/* Timer A Counter */
	uint32_t	m_TimerBCount;		/* Timer B Counter */

	uint32_t	m_DacSelect;		/* DAC Select flag */
	int16_t		m_DacData;			/* DAC Data (9-bit) */

	uint32_t	m_LfoCounter;		/* LFO counter */
	uint32_t	m_LfoPeriod;		/* LFO period */
	uint8_t		m_LfoStep;			/* LFO step counter (7-bit) */
	uint32_t	m_LfoEnable;		/* LFO enable flag */

	uint32_t	m_ClockSpeed;
	uint32_t	m_CyclesToDo;

	void		WriteMode(uint8_t Register, uint8_t Data);
	void		WriteFM(uint8_t Register, uint8_t Port, uint8_t Data);

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

#endif // !_YM2612_H_