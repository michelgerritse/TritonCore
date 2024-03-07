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
#include "YM3413.h"

/*
	Yamaha YM3413 - Digital Signal Processor (LDSP)

	General description:
	--------------------
	
	The YM3413 is a DSP, specifically designed for delay and reverb effects. It can be daisy chained
	with an YM3415 (LEF) effect processor, as seen in the Yamaha SY77.
	It has been used in several products, suchs as the Yamaha TG-100, PSR510, TQ-5 and more.

	Pin layout:
	-----------
	  ____    ____
	-|  1 \__/ 40 |-
	-|  2      39 |-
	-|  3      38 |-
	-|  4      37 |-
	-|  5      36 |-
	-|  6      35 |-
	-|  7      34 |-
	-|  8      33 |-
	-|  9      32 |-
	-| 10      31 |-
	-| 11      30 |-
	-| 12      29 |-
	-| 13      28 |-
	-| 14      27 |-
	-| 15      26 |-
	-| 16      25 |-
	-| 17      24 |-
	-| 18      23 |-
	-| 19      22 |-
	-|_20______21_|-
	  (40-pin DIP)

	 Pin  | I/O | Symbol | Function                | Pin  | I/O | Symbol | Function
	------+-----+--------+-------------------------+------+-----+--------+-------------------------
	   1  |  I  | Vdd    | Power supply (+5.0V)    |  21  |  O  | A5     | Address bus
	   2  | I/O | D7     | Data bus                |  22  |  O  | A6     | Address bus
	   3  | I/O | D6     | Data bus                |  23  |  O  | A7     | Address bus
	   4  | I/O | D5     | Data bus                |  24  |  O  | A8     | Address bus
	   5  | I/O | D4     | Data bus                |  25  |  O  | A9     | Address bus
	   6  | I/O | D3     | Data bus                |  26  |  O  | A10    | Address bus
	   7  | I/O | D2     | Data bus                |  27  |  O  | A11    | Address bus
	   8  | I/O | D1     | Data bus                |  28  |  O  | A12    | Address bus
	   9  | I/O | D0     | Data bus                |  29  |  O  | A13    | Address bus
	  10  |  I  | SI0    | Serial data input (0)   |  30  |  O  | A14    | Address bus
	  11  |  I  | SI1    | Serial data input (1)   |  31  |  O  | A15    | Address bus
	  12  |  I  | SYW    | Sync pulse              |  32  |  O  | A16    | Address bus
	  13  |  O  | /WE    | Write enable            |  33  |  O  | SO0    | Serial data output (0)
	  14  |  O  | /OE    | Output enable           |  34  |  I  | XCLK   | Clock
	  15  |  O  | A0     | Address bus             |  35  |  I  | /IC    | Initial clear
	  16  |  O  | A1     | Address bus             |  36  |  I  | CRS    | Command counter reset
	  17  |  O  | A2     | Address bus             |  37  |  I  | CDI    | Command data input
	  18  |  O  | A3     | Address bus             |  38  |  O  | CDO    | Command data output
	  19  |  O  | A4     | Address bus             |  39  |  O  | SO1    | Serial data output (1)
	  20  |  I  | Vss    | Ground                  |  40  |  I  | CLK    | Clock

	Functional description:
	-----------------------

	Other than the pin layout/description, there is no information publicly available as far as I'm aware.
	The LDSP can handle 2 stereo data streams:
	- Channel 0
	- Channel 1

	Data is serially clocked in, synchronized by the sync pulse signal.
	Input command data (pin CDI) is directly ouput at pin CDO in order to allow daisy chaining.

	The LDSP can access up to 128KB of PSRAM. Interestingly enough the PSR510 has 128KB of PSRAM installed, but address pin A16 is not connected.

	DSP command data format:
	------------------------

	The command data format is unknown, the following command sequences are observed:
	(taken from the TG-100 demo song)

		 LSB | MSB
		-----+-----
		0x00 - 0x80
		0x03 - 0x07
		0x00 - 0x80
		0x02 - 0x2A
		0x00 - 0x80
		0x04 - 0x00
		0x00 - 0x80
		0x02 - 0x2D
		0x00 - 0x80
		0x04 - 0x00
		0x00 - 0x80
		0x02 - 0x2B
		0x00 - 0x80
		0x04 - 0x40
		0x00 - 0x80
		0x02 - 0x2E
		0x00 - 0x80
		0x04 - 0x40
		0x00 - 0x80
		0x06 - 0xFF

	If we rewrite the above sequence into 32-bit values we get this:

		0x07038000
		0x2A028000
		0x00048000
		0x2D028000
		0x00048000
		0x2B028000
		0x40048000
		0x2E028000
		0x40048000
		0xFF068000
	  
	Later on in the TG100 demo VGM, after the song intro, a large sequence is written but ends with:
		0x06 - 0xFA

	There is a clear pattern to be observed:
	The LSB 16-bit is the trigger/identifier for the YM3413. This appears to be 0x8000
	The MSB 16-bit is the actual command which can be split into 2 bytes:
	The LSB byte is the register / function
	The MSB byte is the value / parameter

	The following DSP functions have been observed:

		0x00: Clear DSP program ?
		0x01: Unknown
		0x02: Unknown
		0x03: Channel enable flags ?
		0x04: Unknown
		0x05: Unknown
		0x06: Set DSP volume (0x00 = Mute, 0xFF = Max. volume)

	DSP processing:
	---------------
	  
	It takes 32 clock cycles for a 16-bit stereo sample to get send (and in parallel it will output the previously generated stereo sample).
	This means a DSP program should finish in 32 clock cycles as well.
*/

YM3413::YM3413(size_t MemorySize)
{
	/* Set DSP memory size (128KB maximum) */
	MemorySize = std::min<size_t>(MemorySize, 0x20000);
	m_Memory.resize(MemorySize);

	/* Reset */
	InitialClear();
}

void YM3413::InitialClear()
{
	m_CommandCounter = 0;
	m_Volume = 0;

	/* Clear DSP memory */
	memset(m_Memory.data(), 0, m_Memory.size());
}

void YM3413::ResetCommandCounter()
{
	m_CommandCounter = 0;
}

void YM3413::SendCommandData(uint32_t Command)
{
	pair32_t Data{.u32 = Command};
	
	if (Data.u16l == 0x8000)
	{
		switch (Data.u8hl)
		{
		case 0x00: /* Unknown */
			break;

		case 0x01: /* Unknown */
			break;

		case 0x02: /* Unknown */
			break;

		case 0x03: /* Unknown */
			break;

		case 0x04: /* Unknown */
			break;

		case 0x05: /* Unknown */
			break;

		case 0x06: /* Volume level */
			m_Volume = Data.u8hh;
			break;

		default:
			__debugbreak();
			break;
		}
	}
}

void YM3413::ProcessChannel0(int16_t* pChanL, int16_t* pChanR)
{	
	/* Store new samples */
	int16_t NewSampleL = *pChanL;
	int16_t NewSampleR = *pChanR;

	/* Output previously generated samples */
	*pChanL = 0;
	*pChanR = 0;

	//TODO: Process DSP program for 32 cycles
}

void YM3413::ProcessChannel1(int16_t* pChanL, int16_t* pChanR)
{
	/* Store new samples */
	int16_t NewSampleL = *pChanL;
	int16_t NewSampleR = *pChanR;

	/* Output previously generated samples */
	*pChanL = 0;
	*pChanR = 0;

	//TODO: Process DSP program for 32 cycles
}