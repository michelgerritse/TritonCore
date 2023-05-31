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
#ifndef _SEGAPWM_H_
#define _SEGAPWM_H_

#include "../../Interfaces/ISoundDevice.h"

/* Sega 32X PWM */
class SEGAPWM : public ISoundDevice
{
public:
	SEGAPWM(uint32_t ClockSpeed = 23011360);
	~SEGAPWM() = default;

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
	uint32_t	m_PwmControl;		/* PWM Control Register */
	uint32_t	m_CycleReg;			/* Cycle Register */
	 int16_t	m_PulseWidthL;		/* Pulse Width L Register */
	 int16_t	m_PulseWidthR;		/* Pulse Width R Register */
	 int16_t	m_BaseLineL;
	 int16_t	m_BaseLineR;
	
	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;
};

#endif // !_SEGAPWM_H_