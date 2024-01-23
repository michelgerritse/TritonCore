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
#ifndef _YMZ280B_H_
#define _YMZ280B_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"

/* Yamaha YMZ280B 8-channel ADPCM/PCM decoder (PCMD8) */
class YMZ280B : public ISoundDevice, public IMemoryAccess
{
public:
	YMZ280B(uint32_t ClockSpeed = 16'934'400);
	~YMZ280B() = default;

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

	/* IMemoryAccess methods */
	void			CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size);

private:
	struct pcmd8_t
	{
		pair16_t Pitch;			/* Frequency number (9-bit) */
		uint32_t PitchCnt;		/* "Pitch" counter (10-bit: 1.9) */
		
		uint32_t KeyOn;			/* Key On / Off flag */
		uint32_t Mode;			/* Quantization Mode (2-bit) */
		uint32_t Loop;			/* Loop On / Off flag */
		uint32_t TotalLevel;	/* Total level (8-bit) */
		uint32_t PanAttnL;		/* Panpot L (4-bit) */
		uint32_t PanAttnR;		/* Panpot R (4-bit) */
		pair32_t Start;			/* Start address (24-bit) */
		pair32_t End;			/* End address (24-bit) */
		pair32_t LoopStart;		/* Loop start address (24-bit) */
		pair32_t LoopEnd;		/* Loop end address (24-bit) */
		uint32_t Addr;			/* Current sample address (24-bit) */

		 int16_t SampleT0;		/* Sample interpolation T0 */
		 int16_t SampleT1;		/* Sample interpolation T1 */
		
		 int16_t Signal;		/* Decoded ADPCM signal */
		 int32_t Step;			/* ADPCM step */
		 int16_t LoopSignal;	/* Decoded ADPCM signal at loop start */
		 int32_t LoopStep;		/* ADPCM step at loop start */
		uint32_t NibbleShift;	/* Nibble selection shift */
	};

	pcmd8_t		m_Channel[8];
	uint8_t		m_AddressLatch;

	pair32_t	m_MemAddress;
	uint32_t	m_KeyEnabled;
	uint32_t	m_MemEnabled;
	uint32_t	m_IrqEnabled;
	uint32_t	m_DspEnabled;
	uint32_t	m_LsiTest;

	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	std::vector<uint8_t> m_Memory;

	void	WriteRegister(uint8_t Address, uint8_t Data);
	void	ProcessKeyOnOff(pcmd8_t& Channel, uint32_t NewState);
	int16_t UpdateSample4(pcmd8_t& Channel);
	int16_t UpdateSample8(pcmd8_t& Channel);
	int16_t UpdateSample16(pcmd8_t& Channel);
};

#endif // !_YMZ280B_H_