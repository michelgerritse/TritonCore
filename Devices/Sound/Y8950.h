/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2024, Michel Gerritse
All rights reserved.

This source code is available under the BSD-3-Clause license.
See LICENSE.txt in the root directory of this source tree.

*/
#ifndef _Y8950_H_
#define _Y8950_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"
#include "YM.h"

/* Yamaha Y8950 (MSX-AUDIO) */
class Y8950 : public ISoundDevice, public IMemoryAccess
{
public:
	Y8950(uint32_t ClockSpeed = 3'579'545);
	~Y8950() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName();
	void			Reset(ResetType Type);
	void			SendExclusiveCommand(uint32_t Command, uint32_t Value);

	/* ISoundDevice methods */
	bool			EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	uint32_t		Read(uint32_t Address);
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer);

	/* IMemoryAccess methods */
	void			CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);

private:

	/* OPL data type */
	struct opl_t
	{
		YM::OPL::operator_t	Slot[18];
		YM::OPL::channel_t	Channel[9];
		YM::OPL::timer_t	Timer1;
		YM::OPL::timer_t	Timer2;

		uint32_t	Timer;			/* Global timer (13-bit) */
		uint32_t	CSM;			/* CSM mode on/off flag */
		uint32_t	NTS;			/* Note select flag */
		uint32_t	RHY;			/* Rhythm mode on/off flag */
		uint8_t		Status;			/* Status register (8-bit) */
		uint8_t		StatusMask;		/* Status flag mask (8-bit) */
		int32_t		Out;			/* Accumulator ouput */

		uint32_t	LfoAmStep;		/* Current LFO-AM step */
		uint32_t	LfoAmShift;		/* LFO-AM depth selector */
		uint32_t	LfoAmLevel;		/* LFO-AM attn. level */

		uint32_t	LfoPmStep;		/* Current LFO-PM step */
		uint32_t	LfoPmShift;		/* LFO-PM depth selector */

		uint32_t	NoiseLFSR;		/* Noise shift register (23-bit) */
		uint32_t	NoiseOut;		/* Noise output (1-bit) */

		uint8_t		PhaseHH8;		/* High hat phase bit (b8) */
		uint8_t		PhaseHH;		/* High hat phase bits (b7, b3, b2) */
		uint8_t		PhaseTC;		/* Top cymbal phase bits (b5, b3) */

		uint32_t	LsiTest2;		/* LSI test bit 2 */
	};

	uint32_t		m_ClockSpeed;
	uint32_t		m_CyclesToDo;

	uint8_t			m_AddressLatch;		/* Address latch (8-bit) */
	opl_t			m_OPL;				/* OPL unit */
	YM::adpcmb_t	m_ADPCMB;			/* ADPCM-B unit */
	uint8_t			m_IoCtrl;			/* I/O control (4-bit) */

	std::array<uint8_t, 0x40000>	m_Memory;	/* 256KB ADPCM-B memory */

	void		WriteRegisterArray(uint8_t Address, uint8_t Data);
	void		UpdateTimers();
	void		UpdatePhaseGenerator(uint32_t SlotId);
	void		UpdateEnvelopeGenerator(uint32_t SlotId);
	void		UpdateOperatorUnit(uint32_t SlotId);
	void		UpdateNoiseGenerator();
	void		UpdateADPCMB();
	void		ClearOutput();
	void		GenerateOutput(uint32_t ChannelId);
	int16_t		GetModulation(uint32_t SlotId);
	void		SetStatusFlag(uint8_t Flag);
	void		ResetStatusFlag(uint8_t Flag);
	void		SetStatusMask(uint8_t Mask);
};

#endif // !_Y8950_H_