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
#ifndef _YMF292F_H_
#define _YMF292F_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"

/* Yamaha YMF292-F (Saturn Custom Sound Processor) */
class YMF292F : public ISoundDevice, public IMemoryAccess
{
public:
	YMF292F(uint32_t ClockSpeed = 22'579'200);
	~YMF292F() = default;

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
	static const std::wstring s_DeviceName;

	/* Common registers data type */
	struct common_t
	{
		uint32_t	MemoryMask;		/* MEM4MB: Memory address mask */
		uint32_t	Dac18bit;		/* DAC18B: DAC 16/18bit output selection */
		uint8_t		Version;		/* VER:    Version number (4-bit) */
		uint8_t		MasterVolume;	/* MVOL:   Master volume (4-bit) */
		uint8_t		RingBufLength;	/* RBL:    Ring buffer length */
		uint32_t	RingBufAddr;	/* RBP:    Ring buffer lead address (20-bit) */
		uint8_t		MidiFifoFlags;	/* MIDI input/output FIFO flags */
		//			MiBuf;			/* MIBUF:  MIDI input data buffer */
		//			MoBuf;			/* MOBUF:  MIDI output data buffer */
		uint8_t		MonitorSlot;	/* MSLC:   Monitor slot (5-bit) */
		uint8_t		CallAddress;	/* CA:     Call address (3-bit) */

		// TODO: To be completed
	};

	/* Slot registers data type */
	struct slot_t
	{
		uint32_t	KeyState;		/* Key on/off state */
		uint32_t	KeyLatch;		/* Latched key on/off flag */
		uint32_t	KeyExLatch;		/* Latched key ex on/off flag */
	};

	common_t	m_Common;
	slot_t		m_Slot[32];

	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	std::vector<uint8_t>	m_Memory;

	void	WriteCommonControl8(uint32_t Address, uint8_t Data);
};

#endif // !_YMF292F_H_