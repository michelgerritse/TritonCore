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
#ifndef _YMW258F_H_
#define _YMW258F_H_

#include "../../Interfaces/ISoundDevice.h"
#include "../../Interfaces/IMemoryAccess.h"
#include "YM_GEW.h"
#include "DSP/YM3413.h"

/* Yamaha YMW258-F (Advanced Wave Memory) */
class YMW258F : public ISoundDevice, public IMemoryAccess
{
public:
	YMW258F(uint32_t ClockSpeed = 9'400'000, bool HasLDSP = true, size_t MemorySizeLSDP = 0x20000); /* Default clock taken from PSR510 service manual */
	~YMW258F() = default;

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
	
	YM::GEW8::channel_t	m_Channel[28];

	uint8_t		m_ChannelLatch;		/* PCM address latch */
	uint8_t		m_RegisterLatch;	/* PCM register latch */
	uint32_t	m_Timer;			/* Global timer */
	pair32_t	m_MemoryAddress;	/* External memory address (22-bit) */
	pair32_t	m_DspCommand;		/* DSP command data (32-bit) */
	uint32_t	m_DspCommandCnt;	/* DSP command counter */
	
	uint32_t	m_ClockSpeed;
	uint32_t	m_ClockDivider;
	uint32_t	m_CyclesToDo;

	uint32_t	m_Banking;			/* Banking enable flag */
	uint32_t	m_Bank0;			/* PCM memory bank 0 */
	uint32_t	m_Bank1;			/* PCM memory bank 1 */

	std::vector<uint8_t>	m_Memory;
	std::unique_ptr<YM3413>	m_LDSP;

	void	WritePcmData(uint8_t ChannelNr, uint8_t Register, uint8_t Data);
	void	LoadWaveTable(YM::GEW8::channel_t& Channel);
	
	void	UpdateLFO(YM::GEW8::channel_t& Channel);
	void	UpdateAddressGenerator(YM::GEW8::channel_t& Channel);
	void	UpdateEnvelopeGenerator(YM::GEW8::channel_t& Channel);
	void	UpdateMultiplier(YM::GEW8::channel_t& Channel);
};

#endif // !_YMW258F_H_