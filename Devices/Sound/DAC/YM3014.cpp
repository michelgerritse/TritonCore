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
	  ____   ____              ____   ____	
	-|	1 \_/  8 |-          -|  1 \_/ 16 |-
	 |           |           -|  2     15 |-
	-|  2      7 |-          -|  3     14 |-
	 |           |           -|  4     13 |-
	-|  3      6 |-          -|  5     12 |-
	 |           |           -|  6     11 |-
	-|  4      5 |-          -|  7     10 |-
	 |___________|           -|__8______9_|-
	   YM3014(B)                YM3014B-F
	  (8-pin DIP)              (16-pin SOP)
	
	 Pin  |  Pin   |     |        |
     8DIP | 16SOP  | I/O | Symbol | Function
	------+--------+-----+--------+---------------------------
	   1  |    1   |  I  | Vdd    | Power supply (+5.0V)
	   2  |    2   |  O  | ToBuff | Analog output
	   3  |    4   |  I  | Load   | Load signal
	   4  |    7   |  I  | SD     | Serial data
	   5  |   10   |  I  | Clock  | External clock input
	   6  |   12   |  I  | Vss    | Power supply (GND)
	   7  |   14   |  O  | Rb     | Bias voltage (1/2Vdd)
	   8  |   16   |  I  | MP     | Reference voltage (1/2Vdd)

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

	N = (S2 * 2^2) + (S1 * 2^1) + S0

	Die shot: https://siliconpr0n.org/map/yamaha/y3014/
*/

int16_t YM3014::Process(uint16_t Data)
{
	int16_t AnalogOut = 0;
	
	return AnalogOut;
}