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
#ifndef _ISOUND_DEVICE_H_
#define _ISOUND_DEVICE_H_

#include "IDevice.h"
#include "IAudioBuffer.h"

/* Speaker positions, taken from mmreg.h */
#define SPEAKER_FRONT_LEFT              0x1
#define SPEAKER_FRONT_RIGHT             0x2
#define SPEAKER_FRONT_CENTER            0x4
#define SPEAKER_LOW_FREQUENCY           0x8
#define SPEAKER_BACK_LEFT               0x10
#define SPEAKER_BACK_RIGHT              0x20
#define SPEAKER_FRONT_LEFT_OF_CENTER    0x40
#define SPEAKER_FRONT_RIGHT_OF_CENTER   0x80
#define SPEAKER_BACK_CENTER             0x100
#define SPEAKER_SIDE_LEFT               0x200
#define SPEAKER_SIDE_RIGHT              0x400
#define SPEAKER_TOP_CENTER              0x800
#define SPEAKER_TOP_FRONT_LEFT          0x1000
#define SPEAKER_TOP_FRONT_CENTER        0x2000
#define SPEAKER_TOP_FRONT_RIGHT         0x4000
#define SPEAKER_TOP_BACK_LEFT           0x8000
#define SPEAKER_TOP_BACK_CENTER         0x10000
#define SPEAKER_TOP_BACK_RIGHT          0x20000

/* Sound device audio output description */
struct AUDIO_OUTPUT_DESC
{
	uint32_t		SampleRate;
	uint32_t		SampleFormat; /* ignore for now */
	uint32_t		Channels;
	uint32_t		ChannelMask;
	std::wstring	Description;
};

/* Abstract sound device interface */
struct __declspec(novtable) ISoundDevice : public IDevice
{
	virtual bool			EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc) = 0;
	virtual void			SetClockSpeed(uint32_t ClockSpeed) = 0;
	virtual uint32_t		GetClockSpeed() = 0;
	virtual void			Write(uint32_t Address, uint32_t Data) = 0;
	virtual void			Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer) = 0;
};

#endif // !_ISOUND_DEVICE_H_