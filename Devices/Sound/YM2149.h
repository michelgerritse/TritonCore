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
#ifndef _YM2149_H_
#define _YM2149_H_

#include "../../Interfaces/ISoundDevice.h"
#include "AY.h"

/* Yamaha YM2149 (SSG) */
class YM2149 : public ISoundDevice
{
public:
	YM2149(uint32_t ClockSpeed = 4'000'000, bool SelIsLow = false);
	~YM2149() = default;

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
	AY::tone_t		m_Tone[3];
	AY::noise_t		m_Noise;
	AY::envelope_t	m_Envelope;

	std::array<uint8_t, 16> m_Register;
	
	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;
};

#endif // !_YM2149_H_