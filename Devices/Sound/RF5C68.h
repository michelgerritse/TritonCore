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
#ifndef _RF5C68_H_
#define _RF5C68_H_

#include "Interfaces/ISoundDevice.h"
#include "Interfaces/IMemoryAccess.h"

/* Ricoh RF5C68 / RF5C164 PCM Sound Source */
class RF5C68 : public ISoundDevice, public IMemoryAccess
{
public:
	enum MODEL
	{
		MODEL_RF5C68 = 0,
		MODEL_RF5C164
	};

	RF5C68(uint32_t Model = 0, bool UseRAMAX = false);
	~RF5C68() = default;

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

	/* IMemoryAccess methods */
	void			CopyToMemory(size_t Offset, uint8_t* Data, size_t Size);
	void			CopyToMemoryIndirect(size_t Offset, uint8_t* Data, size_t Size);

private:

	/* PCM Channel */
	struct CHANNEL
	{
		uint32_t	ON;		/* Channel On / Off flag */
		uint8_t		ENV;	/* Envelope Data (8-bit) */
		uint8_t		PAN_L;	/* Pan Data (L) (4-bit) */
		uint8_t		PAN_R;	/* Pan Data (R) (4-bit) */
		uint32_t	FD;		/* Frequency Delta (16-bit) */
		uint32_t	LS;		/* Loop Address (16-bit) */
		uint32_t	ST;		/* Start Address (16-bit) */

		/* Internal State */
		uint32_t	ADDR;		/* Current address counter */
		uint32_t	PREMUL_L;	/* ENV * PAN (L) pre-multiplied data */
		uint32_t	PREMUL_R;	/* ENV * PAN (R) pre-multiplied data */
	};

	CHANNEL		m_Channel[8];	/* PCM channels */
	uint32_t	m_Sounding;		/* IC sounding */
	uint32_t	m_WaveBank;		/* Current wave bank */
	uint32_t	m_ChannelBank;	/* Current channel */
	uint8_t		m_ChannelCtrl;	/* Channel control register */

	uint32_t m_Model;			/* Device model */
	uint32_t m_Shift;			/* Fixed point shift (16.11 or 17.10) */
	uint32_t m_OutputMask;		/* DAC output mask */

	uint32_t m_ClockSpeed;
	uint32_t m_ClockDivider;
	uint32_t m_CyclesToDo;

	std::vector<uint8_t> m_Memory;
};

#endif // !_RF5C68_H_