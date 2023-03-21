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
#ifndef _SN76489_H_
#define _SN76489_H_

<<<<<<<< HEAD:Devices/Sound/SN76489.h
#include "Interfaces/ISoundDevice.h"
#include "Interfaces/IAudioBuffer.h"
========
#include "Source/Interfaces/ISoundDevice.h"
>>>>>>>> 4333eb0f0248882e14a289f2963ff3d59a96b815:TritonCore/Devices/Sound/SN76489.h

/* Texas Instruments SN76489 Sound Generator */
class SN76489 : public ISoundDevice
{
public:
	SN76489(uint32_t Model = 0, uint32_t Flags = 0);
	~SN76489() = default;

	/* IDevice methods */
	const wchar_t*	GetDeviceName();
	void			Reset(ResetType Type);

	/* ISoundDevice methods */
	uint32_t		GetOutputCount();
	uint32_t		GetSampleRate(uint32_t ID);
	uint32_t		GetSampleFormat(uint32_t ID);
	uint32_t		GetChannelMask(uint32_t ID);
	const wchar_t*	GetOutputName(uint32_t ID);
	void			SetClockSpeed(uint32_t ClockSpeed);
	uint32_t		GetClockSpeed();
	void			Write(uint32_t Address, uint32_t Data);
	void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*> &OutBuffer);

private:	
	struct NOISE
	{
		int32_t		Counter;		/* Noise counter */
		uint32_t	Period;			/* Noise half-period */
		uint32_t	LFSR;			/* Linear Feedback Shift Register (16-bit) */
		uint32_t	FlipFlop;		/* Output flip-flop */
		uint32_t	Control;		/* Noise control register */
		int16_t		Volume;			/* Noise output volume */
		uint16_t	Output;			/* Volume ouput mask */
	};

	struct TONE
	{
		int32_t		Counter;		/* Tone counter */
		uint32_t	Period;			/* Tone half-period (10-bit) */
		uint16_t	FlipFlop;		/* Output flip-flop */
		int16_t		Volume;			/* Tone output volume */
	};

	int16_t		m_VolumeTable[16];	/* Non-linear volume table */
	uint32_t	m_Register;			/* Current latched register */
	uint8_t		m_StereoMask;		/* Game Gear stereo mask */
	TONE		m_Tone[3];			/* Tone channels */
	NOISE		m_Noise;			/* Noise channel */

	uint32_t m_Model;
	uint32_t m_Flags;
	uint32_t m_ClockSpeed;
	uint32_t m_ClockDivider;
	uint32_t m_CyclesToDo;

	void UpdateToneGenerators();
	void UpdateNoiseGenerator();
};

#endif // !_SN76489_H_