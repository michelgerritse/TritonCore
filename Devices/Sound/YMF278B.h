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
#ifndef _YMF278B_H_
#define _YMF278B_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"

/* Yamaha YMF278B (FM + Wave Table Synthesizer) */
class YMF278B : public ISoundDevice, public IMemoryAccess
{
public:
	YMF278B();
	~YMF278B() = default;

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
	void			CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);

private:

	/* Envelope phases */
	enum ADSR : uint32_t
	{
		Attack = 0,
		Decay,
		Sustain,
		Release
	};

	struct CHANNEL
	{
		pair16_t	WaveNr;		/* Wave table number (9-bit) */
		uint32_t	FNum;		/* Frequency number (10-bit) */
		uint32_t	FNum9;		/* Copy of FNum bit 9 */
		int32_t		Octave;		/* Octave (signed 4-bit) */
		uint32_t	PanAttnL;	/* Pan attenuation left */
		uint32_t	PanAttnR;	/* Pan attenuation right */
		uint32_t	TL;			/* Total Level (7-bit) */
		uint32_t	TargetTL;	/* Interpolated TL */

		uint32_t	KeyOn;		/* Key On / Off flag */
		uint32_t	KeyPending;	/* Key On / Off pending state */
		uint32_t	EgPhase;	/* Envelope phase */
		uint32_t	EgLevel;	/* Envelope output level (10-bit) */

		uint32_t	SampleCount;	/* Sample address (whole part) */
		uint32_t	SampleDelta;	/* Sample address (fractional) */

		uint32_t	Format;		/* Wave format (2-bit) */
		pair32_t	Start;		/* Start address (22-bit) */
		pair16_t	Loop;		/* Loop address (16-bit) */
		uint32_t	End;		/* End address (16-bit) */
		uint8_t		Rate[4];	/* ADSR rates (4-bit) */
		uint32_t	DL;			/* Decay level (4-bit) */
		uint32_t	RC;			/* Rate correction (4-bit) */

		uint32_t	LfoCounter;	/* LFO counter */
		uint32_t	LfoPeriod;	/* LFO period */
		uint8_t		LfoStep;	/* LFO step counter (8-bit) */
		uint32_t	LfoReset;	/* LFO reset flag */
		uint32_t	PmDepth;	/* Vibrato depth (3-bit) */
		uint32_t	AmDepth;	/* Tremolo depth (3-bit) */

		int16_t		SampleT0;	/* Sample interpolation T0 */
		int16_t		SampleT1;	/* Sample interpolation T1 */
		int16_t		Sample;		/* Interpolated sample */
		int16_t		OutputL;	/* Channel output (left) */
		int16_t		OutputR;	/* Channel output (right) */
	};

	CHANNEL		m_Channel[24];
	uint8_t		m_AddressLatch;

	uint32_t	m_New;				/* OPL3 enable flag */
	uint32_t	m_New2;				/* OPL4 enable flag */

	pair32_t	m_MemoryAddress;	/* Memory address register (22-bit) */
	uint32_t	m_MemoryAccess;		/* Memory access mode flag */
	uint32_t	m_MemoryType;		/* Memory type flag: 0 = ROM, 1 = SRAM + ROM */
	uint32_t	m_WaveTableHeader;	/* Wave table header memory offset (384 to 511) */

	uint32_t	m_MixCtrlFML;		/* Mix control attenuation (FM Left) */
	uint32_t	m_MixCtrlFMR;		/* Mix control attenuation (FM Right) */
	uint32_t	m_MixCtrlPCML;		/* Mix control attenuation (PCM Left) */
	uint32_t	m_MixCtrlPCMR;		/* Mix control attenuation (PCM Right) */
	
	uint32_t	m_EnvelopeCounter;	/* Global envelope counter */
	uint32_t	m_InterpolCounter;	/* Global TL interpolation counter */

	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	std::vector<uint8_t> m_Memory;

	void WriteFM0(uint8_t Register, uint8_t Data);
	void WriteFM1(uint8_t Register, uint8_t Data);
	void WritePCM(uint8_t Register, uint8_t Data);

	void LoadWaveTable(CHANNEL& Channel);
	int16_t ReadSample(CHANNEL& Channel);
	
	void	UpdateLFO(CHANNEL& Channel);
	void	UpdateAddressGenerator(CHANNEL& Channel);
	void	UpdateEnvelopeGenerator(CHANNEL& Channel);
	void	UpdateMultiplier(CHANNEL& Channel);
	void	UpdateInterpolator(CHANNEL& Channel);
	
	void	ProcessKeyOnOff(CHANNEL& Channel);
	uint8_t CalculateRate(CHANNEL& Channel, uint8_t Rate);
};

#endif // !_YMF278B_H_