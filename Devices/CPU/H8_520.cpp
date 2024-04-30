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
#include "H8_520.h"

/* Static class member initialization */
const std::wstring H8_520::s_DeviceName = L"Hitachi H8/520";

H8_520::H8_520()
{
	ResetToDefaults();

	/* Clear on-chip ROM */
	OnchipROM.fill(0x00);
	
	/* Put CPU in reset state */
	State = CpuState::RESET;
	ResetPin = PinState::Low;
}

void H8_520::ResetToDefaults()
{
	R[0] = 0xDEAD;
	R[1] = 0xDEAD;
	R[2] = 0xDEAD;
	R[3] = 0xDEAD;
	R[4] = 0xDEAD;
	R[5] = 0xDEAD;
	R[6] = 0xDEAD;
	R[7] = 0xDEAD;

	PC = 0xDEAD;

	SR = 0x0700;
	
	CP = 0x00;
	DP = 0x00;
	EP = 0x00;
	TP = 0x00;
	BR = 0x00;

	OnchipRAM.fill(0x00);

	//TODO: On-chip device registers
}

void H8_520::SetOperatingMode(McuMode NewMode)
{
	ModePins = NewMode & 0x07;

	if (ModePins == McuMode::Mode6)
	{
		/* Terminate execution and go into hardware standby state */
		State = CpuState::HSTBY;
	}
}

void H8_520::SetResetPinState(PinState NewState)
{
	if (NewState == PinState::Low)
	{
		/* Terminate execution and move to the reset state */
		State = CpuState::RESET;
	}

	ResetPin = NewState;
}

void H8_520::GenerateException(ExceptionType Type)
{
	uint32_t VectorAddr;

	if (IsMinimum)
	{
		VectorAddr = Type * 2;
	}
	else
	{
		VectorAddr = Type * 4;
	}
}

void H8_520::Execute()
{
	switch (State)
	{
	case CpuState::HSTBY:
		/* Do nothing, we can only leave this state by setting /RES low */
		return;

	case CpuState::SSTBY:
		/* TODO */
		return;

	case CpuState::RESET:
		if (ResetPin == PinState::Low) return;

		/* We are moving out the reset state, reset CPU to default values */
		ResetToDefaults();

		/* Latch mode pins into MDCR register */
		MDCR = 0xC0 | (ModePins & 0x07); /* b7 and b6 are always set */
		
		/* Check CPU mode compatibility */
		switch (MDCR & 0x07)
		{
		//TODO: Create memory map based on operating mode

		case McuMode::Mode0:
			__debugbreak();
			break;

		case McuMode::Mode1:
			AddrMask = 0x00FFFF; /* 16-bit address bus */
			IsMinimum = true;
			IsExpanded = true;
			HasOnchipROM = false;
			break;

		case McuMode::Mode2:
			AddrMask = 0x00FFFF; /* 16-bit address bus */
			IsMinimum = true;
			IsExpanded = true;
			HasOnchipROM = true;
			break;

		case McuMode::Mode3:
			AddrMask = 0x0FFFFF; /* 20-bit address bus */
			IsMinimum = true;
			IsExpanded = true;
			HasOnchipROM = false;
			break;

		case McuMode::Mode4:
			AddrMask = 0x0FFFFF; /* 20-bit address bus */
			IsMinimum = true;
			IsExpanded = true;
			HasOnchipROM = true;
			break;

		case McuMode::Mode5:
			__debugbreak;
			break;

		case McuMode::Mode6:
			State = CpuState::HSTBY;
			return;

		case McuMode::Mode7:
			AddrMask = 0x00FFFF; /* 20-bit address bus */
			IsMinimum = true;
			IsExpanded = false;
			HasOnchipROM = true;
			break;
		}
		
		if (IsMinimum)
		{
			PC = Read16(0x0000);
		}
		else
		{
			CP = Read16(0x0000) & 0x00FF;
			PC = Read16(0x0002);
		}
		break;

	case CpuState::EXCEP:
		/* TODO */
		return;

	case CpuState::IEXEC:
		/* TODO */
		return;

	case CpuState::SLEEP:
		/* TODO */
		return;

	case CpuState::BUSRL:
		/* Not supported on H8/520. We should not get here */
		__debugbreak();
		return;
	}
}

uint8_t H8_520::Read8(uint32_t Address)
{
	return 0;
}

uint16_t H8_520::Read16(uint32_t Address)
{
	if (Address & 0x01)
	{
		GenerateException(ExceptionType::AddressError);
		Address &= 0x00FFFFFE; /* Is this correct ?? */
	}
	
	return (Read8(Address) << 8) | Read8(Address + 1);
}
