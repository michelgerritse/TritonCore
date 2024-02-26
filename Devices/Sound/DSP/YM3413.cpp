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

	Overview:
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

*/

YM3413::YM3413(size_t MemorySize)
{
	/* Set DSP memory size */
	m_Memory.resize(MemorySize);

	/* Reset */
	InitialClear();
}

void YM3413::InitialClear()
{
	m_CommandCounter = 0;

	/* Clear DSP memory */
	memset(m_Memory.data(), 0, m_Memory.size());
}

void YM3413::ResetCommandCounter()
{
	m_CommandCounter = 0;
}

void YM3413::SendCommandData(uint8_t Command)
{
}

void YM3413::SendSerialInput0(int16_t ChL, int16_t ChR)
{
}

void YM3413::SendSerialInput1(int16_t ChL, int16_t ChR)
{
}