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
#include "SEGAPWM.h"

/*
	Sega 32X PWM

	How to calculate the sample rate:
	---------------------------------
	
	SampleRate = MasterClock / ((CycleReg - 1) & 0xFFF)

	Where:	MasterClock = ~23MHz
			CycleReg = 0 - 4095

	A couple of notes:
	------------------
	- Both channels use a 3-step FIFO
	- When the FIFO is empty, the last pulled value will be used
	- When the FIFO is full, the oldest data stored will be discarded
	- Writing to the mono pulse width register will write to both L + R FIFOs
	- When both channels are OFF, they output normally
	- When the cycle register is set to 1, no sound will be output
	- Pulse width can not exceed the cycle time;

	Cheat sheet:
	------------

	PWM Control Register:
	
	| b15 | b14 | b13 | b12 | b11 | b10 | b09 | b08 | b07 | b06 | b05 | b04 | b03 | b02 | b01 | b00 |
	+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
	| --- | --- | --- | --- | TM3 | TM2 | TM1 | TM0 | RTP | --- | --- | --- | RMD0| RMD1| LMD0| LMD1|

	
	| RMD0| RMD1| OUT |		| LMD0| LMD1| OUT |
	+-----+-----+-----+		+-----+-----+-----+
	|  0  |  0  | OFF |		|  0  |  0  | OFF |
	|  0  |  1  |  R  |		|  0  |  1  |  L  |
	|  1  |  0  |  L  |		|  1  |  0  |  R  |
	|  1  |  1  | N/A |		|  1  |  1  | N/A |

*/

#define MAX_DIV 256

SEGAPWM::SEGAPWM(uint32_t ClockSpeed) :
	m_ClockSpeed(ClockSpeed),
	m_ClockDivider(MAX_DIV)
{
	Reset(ResetType::PowerOnDefaults);
}

const wchar_t* SEGAPWM::GetDeviceName()
{
	return L"Sega 32X PWM";
}

void SEGAPWM::Reset(ResetType Type)
{
	m_CyclesToDo = 0;

	m_PwmControl = 0;
	m_CycleReg = 0;
	m_PulseWidthL = 0;
	m_PulseWidthR = 0;
}

void SEGAPWM::SendExclusiveCommand(uint32_t Command, uint32_t Value)
{
	/* This is used for VGM DAC streams, just forward the command */
	Write(Command, Value);
}

bool SEGAPWM::EnumAudioOutputs(uint32_t OutputNr, AUDIO_OUTPUT_DESC& Desc)
{
	if (OutputNr == 0)
	{
		Desc.SampleRate = m_ClockSpeed / m_ClockDivider;
		Desc.SampleFormat = 0;
		Desc.Channels = 2;
		Desc.ChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
		Desc.Description = L"";

		return true;
	}

	return false;
}

void SEGAPWM::SetClockSpeed(uint32_t ClockSpeed)
{
	m_ClockSpeed = ClockSpeed;
}

uint32_t SEGAPWM::GetClockSpeed()
{
	return m_ClockSpeed;
}

void SEGAPWM::Write(uint32_t Address, uint32_t Data)
{	
	Data &= 0xFFF;
	
	switch (Address & 0x0F)
	{
		/* PWM Control Register (MD: A15130H, SH2: 20004030H) */
		case 0x00:
			m_PwmControl = Data;
			break;

		/* Cycle Register (MD: A15132H, SH2: 20004032H) */
		case 0x01:
			m_CycleReg = (Data - 1) & 0xFFF;

			/* Limit max. allowed sample rate */
			if (m_CycleReg < MAX_DIV) m_CycleReg = 0;

			break;

		/* Lch Pulse Width Register (MD: A15134H, SH2: 20004034H) */
		case 0x02:
			m_PulseWidthL = Data;
			break;

		/* Rch Pulse Width Register (MD: A15136H, SH2: 20004036H) */
		case 0x03:
			m_PulseWidthR = Data;
			break;

		/* Mono Pulse Width Register (MD: A15138H, SH2: 20004038H) */
		case 0x04:
			m_PulseWidthL = Data;
			m_PulseWidthR = Data;
			break;

		default:
			break;
	}
}

void SEGAPWM::Update(uint32_t ClockCycles, std::vector<IAudioBuffer*>& OutBuffer)
{
	uint32_t TotalCycles = ClockCycles + m_CyclesToDo;
	uint32_t Samples = TotalCycles / m_ClockDivider;
	m_CyclesToDo = TotalCycles % m_ClockDivider;

	int16_t OutL = 0;
	int16_t OutR = 0;

	if (m_CycleReg != 0)
	{
		/* Limit pulse width (required by some games e.g Tempo) */
		if (m_PulseWidthL > m_CycleReg) m_PulseWidthL = m_CycleReg;
		if (m_PulseWidthR > m_CycleReg) m_PulseWidthR = m_CycleReg;

		/* Calculate zero/base line */
		int16_t ZeroLine = (m_CycleReg / 2);
		
		/* Convert pulse width into a 16-bit signed output */
		int16_t LMD = ((m_PulseWidthL - ZeroLine) * 0x7FFF) / ZeroLine;
		int16_t RMD = ((m_PulseWidthR - ZeroLine) * 0x7FFF) / ZeroLine;

		/* Output mapping */
		switch (m_PwmControl & 0x0F)
		{
		case 0x00: /* RMD = OFF, LMD = OFF (undocumented) */
		case 0x05: /* RMD = R, LMD = L */
			OutL = LMD;
			OutR = RMD;
			break;

		case 0x01: /* RMD = OFF, LMD = L */
			OutL = LMD;
			break;

		case 0x02: /* RMD = OFF, LMD = R */
			OutR = LMD;
			break;

		case 0x04: /* RMD = R, LMD = OFF */
			OutR = RMD;
			break;

		case 0x08: /* RMD = L, LMD = OFF */
			OutL = RMD;
			break;

		case 0x0A: /* RMD = L, LMD = R */
			OutL = RMD;
			OutR = LMD;
			break;

		default:
			__debugbreak();
			break;
		}
	}

	while (Samples-- != 0)
	{
		/* 16-bit DAC output (interleaved) */
		OutBuffer[0]->WriteSampleS16(OutL);
		OutBuffer[0]->WriteSampleS16(OutR);
	}
}