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
#ifndef _H8_520_H_
#define _H8_520_H_

#include "../../TritonCore.h"

/* Hitachi H8/520 */
class H8_520
{
public:
	enum CpuState : uint32_t
	{
		HSTBY,	/* Hardware standby mode */
		SSTBY,	/* Software standby mode */
		RESET,	/* Reset state */
		EXCEP,	/* Exception-handling state */
		IEXEC,	/* Program execution state */
		SLEEP,	/* Sleep mode */
		BUSRL,	/* Bus release state (not supported by H8/520) */
	};

	enum PinState : uint32_t
	{
		Low,
		High
	};

	enum McuMode : uint32_t
	{
		Mode0,	/* Invalid mode */
		Mode1,	/* Expanded minimum mode, ROM disabled */
		Mode2,	/* Expanded minimum mode, ROM enabled */
		Mode3,	/* Expanded maximum mode, ROM disabled */
		Mode4,	/* Expanded maximum mode, ROM enabled */
		Mode5,	/* Invalid mode */
		Mode6,	/* Hardware standy mode */
		Mode7	/* Single-chip mode, ROM enabled */
	};

	enum ExceptionType : uint32_t
	{
		Reset,
		Reserved0,
		InvalidInstruction,
		DivideByZero,
		Trap,
		Reserved1,
		Reserved2,
		Reserved3,
		AddressError,
		Trace,
		Reserved4,
		NonMaskableInterrupt,
		Reserved5,
		Reserved6,
		Reserved7,
		Reserved8,
		TrapA0,
		TrapA1,
		TrapA2,
		TrapA3,
		TrapA4,
		TrapA5,
		TrapA6,
		TrapA7,
		TrapA8,
		TrapA9,
		TrapA10,
		TrapA11,
		TrapA12,
		TrapA13,
		TrapA14,
		TrapA15
	};

	H8_520();
	~H8_520() = default;

	void ResetToDefaults();
	void SetOperatingMode(McuMode NewMode);
	void SetResetPinState(PinState NewState);
	void GenerateException(ExceptionType Type);
	void Execute();

private:
	static const std::wstring s_DeviceName;

	uint32_t	R[8];		/* General Registers (16-bit) */
	uint32_t	PC;			/* Program Counter (16-bit) */
	uint32_t	SR;			/* Status Register (16-bit) */
	uint32_t	CP;			/* Code Page Register (8-bit) */
	uint32_t	DP;			/* Date Page Register (8-bit) */
	uint32_t	EP;			/* Extended Page Register (8-bit) */
	uint32_t	TP;			/* Stack Page Register (8-bit) */
	uint32_t	BR;			/* Base Register (8-bit) */

	uint8_t		MDCR;		/* Mode Control Register */

	uint32_t	AddrMask;
	bool		IsMinimum;
	bool		IsExpanded;
	bool		HasOnchipROM;

	uint32_t	State;		/* MCU operating state */
	PinState	ResetPin;	/* /RES pin */
	uint32_t	ModePins;	/* MD0, MD1 and MD2 pin states */

	std::array<uint8_t, 512>	OnchipRAM;	/* 512B on-chip RAM */
	std::array<uint8_t, 16384>	OnchipROM;	/* 16KB on-chip ROM */

	uint8_t		Read8(uint32_t Address);
	uint16_t	Read16(uint32_t Address);
};

#endif // !_H8_520_H_