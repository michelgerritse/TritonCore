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
#include "YM3014.h"

/*
	Yamaha YM3014(B) - Serial Input Floating D/A Converter (DAC-SS)

	Overview:
	  ____   ____
	-|  1 \_/  8 |-
	 |           |
	-|  2      7 |-
	 |           |
	-|  3      6 |-
	 |           |
	-|  4      5 |-
	 |___________|
	   YM3014(B)
	  (8-pin DIP)
	
	 Pin  |     |        |
	 8DIP | I/O | Symbol | Function
	------+-----+--------+---------------------------
	   1  |  I  | Vdd    | Power supply (+5.0V)
	   2  |  O  | ToBuff | Analog output
	   3  |  I  | Load   | Load signal
	   4  |  I  | SD     | Serial data
	   5  |  I  | Clock  | External clock input
	   6  |  I  | Vss    | Power supply (GND)
	   7  |  O  | Rb     | Bias voltage (1/2Vdd)
	   8  |  I  | MP     | Reference voltage (1/2Vdd)

	  ____   ____
	-|  1 \_/ 16 |-
	-|  2     15 |-
	-|  3     14 |-
	-|  4     13 |-
	-|  5     12 |-
	-|  6     11 |-
	-|  7     10 |-
	-|__8______9_|-
	   YM3014B-F
	  (16-pin SOP)

	 Pin   |     |        |
	16SOP  | I/O | Symbol | Function
	-------+-----+--------+---------------------------
	   1   |  I  | Vdd    | Power supply (+5.0V)
	   2   |  O  | ToBuff | Analog output
	   4   |  I  | Load   | Load signal
	   7   |  I  | SD     | Serial data
	  10   |  I  | Clock  | External clock input
	  12   |  I  | Vss    | Power supply (GND)
	  14   |  O  | Rb     | Bias voltage (1/2Vdd)
	  16   |  I  | MP     | Reference voltage (1/2Vdd)
	(All other pins are not connected)

	Serial input data format (16-bit):
	---------------------------------------------------------------------------------
	| S2 | S1 | S0 | D9 | D8 | D7 | D6 | D5 | D4 | D3 | D2 | D1 | D0 | xx | xx | xx |
	---------------------------------------------------------------------------------

	Sx = Exponent (3-bit, S2 + S1 + S0 = 0: not allowed)
	Dx = Mantissa (10-bit)
	D9 = Sign (1: positive, 0: negative)
	xx = Ignored

	Function:

	Vout = 1/2 Vdd + 1/4 Vdd * (-1 + D9 + (D8 * 2^-1) + ..... + (D0 * 2^-9) + 2^-10) * 2^-N

	N = (_S2 * 2^2) + (_S1 * 2^1) + _S0 (Note: S2-S0 are inverted)

	Die shot: https://siliconpr0n.org/map/yamaha/y3014/
*/

/* Static class member initialization */
const std::wstring YM3014::s_DeviceName = L"Yamaha YM3014";

const std::wstring& YM3014::GetDeviceName()
{
	return s_DeviceName;
}

uint32_t YM3014::GetAudioFormat()
{
	return AudioFormat::AUDIO_FMT_F32;
}

uint32_t YM3014::GetAudioChannels()
{
	return 1;
}

float YM3014::SendDigitalData(int16_t Data)
{	
	/* Invert negative data (1's complement) */
	uint32_t uData = (Data ^ (Data >> 15));

	/* Count leading zero's (ignoring the sign bit) */
	uint32_t ZeroCount = std::countl_zero(uData << 17);
	
	/* Extract the 10 significant bits, invert sign */
	uint32_t Shift = std::min(ZeroCount, 6u);
	uData = ((Data >> (6 - Shift)) & 0x3FF) ^ 0x200;

	/* Exponent */
	float Exponent = (float) (1 << Shift);
	
	/* Mantissa */
	float Mantissa = -1.0f + 0.0009765625f;   //-1 + 2^-10
	Mantissa += (1.0f / 512.0f) * uData;
	
	//TODO: Get actual voltage levels from hardware testing
	//if (uData & 0x001) Mantissa += 0.001953125f; //2^-9
	//if (uData & 0x002) Mantissa += 0.00390625f;  //2^-8
	//if (uData & 0x004) Mantissa += 0.0078125f;   //2^-7
	//if (uData & 0x008) Mantissa += 0.015625f;    //2^-6
	//if (uData & 0x010) Mantissa += 0.03125f;     //2^-5
	//if (uData & 0x020) Mantissa += 0.0625f;      //2^-4
	//if (uData & 0x040) Mantissa += 0.125f;       //2^-3
	//if (uData & 0x080) Mantissa += 0.25f;        //2^-2
	//if (uData & 0x100) Mantissa += 0.5f;         //2^-1
	//if (uData & 0x200) Mantissa += 1.0f;         //2^ 0

	/* Analog shift */
	float Vout = Mantissa / Exponent;

	return Vout;
}