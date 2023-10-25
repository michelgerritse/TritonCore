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
#ifndef _AY8910_H_
#define _AY8910_H_

#include "../../Interfaces/ISoundDevice.h"

/* General Instrument AY-3-8910 */
class AY8910 : public ISoundDevice
{
public:
	AY8910(uint32_t ClockSpeed = 2'000'000);
	~AY8910() = default;

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

private:
	struct AYCHANNEL
	{
		uint32_t	Counter;		/* Tone counter (12-bit) */
		pair32_t	Period;			/* Tone period (12-bit) */
		uint32_t	Output;			/* Tone output (1-bit) */
		int16_t		Volume;			/* Tone volume */
		uint32_t	ToneDisable;	/* Tone mixer control */
		uint32_t	NoiseDisable;	/* Noise mixer control */
		uint32_t	AmpCtrl;		/* Amplitude control mode */
	};

	struct AYNOISE
	{
		uint32_t	Counter;		/* Noise counter (12-bit) */
		uint32_t	Period;			/* Noise period (12-bit) */
		uint32_t	Output;			/* Noise output (1-bit) */
		uint32_t	FlipFlop;
		uint32_t	LFSR;			/* Shift register (17-bit) */
	};

	struct AYENVELOPE
	{
		uint32_t	Counter;		/* Envelope counter (16-bit) */
		pair32_t	Period;			/* Envelope period (16-bit) */
		int16_t		Volume;			/* Envelope volume */
		uint32_t	FlipFlop;
		uint32_t	Step;			/* Current envelope step (0 - 15) */
		uint32_t	State;			/* Envelope state (4-bit) */
		uint32_t	Hld;			/* Envelope hold bit */
		uint32_t	Alt;			/* Envelope alternate bit */
	};
	
	AYCHANNEL	m_Tone[3];
	AYNOISE		m_Noise;
	AYENVELOPE	m_Envelope;

	uint32_t	m_Register[16];		/* Register array (8-bit) */
	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;
};

#endif // !_AY8910_H_