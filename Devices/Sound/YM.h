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
#ifndef _YM_H_
#define _YM_H_

#include <cstdint>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace YM /* Yamaha */
{
	/* Sine generator */
	static uint32_t GenerateSine(uint32_t Offset, uint32_t Range)
	{
		/*
		x = [0:255]
		y = round(-log(sin((x + 0.5) * pi / 2 / 256)) / log(2) * 256)

		The sine table has been re-constructed from actual YM3812 die shots:
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665
		*/

		/* Limit the offset to the given range */
		Offset = Offset % Range;

		return (uint32_t) round(-log(sin((Offset + 0.5) * std::numbers::pi / 2.0 / Range)) / std::numbers::ln2 * Range);
	};

	/* Exponent generator */
	static uint32_t GenerateExponent(uint32_t Value)
	{
		/*
		x = [0:255]
		y = round((power(2, x / 256) - 1) * 1024)

		The exponent table has been re-constructed from actual YM3812 die shots:
		https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo

		Credits to Matthew Gambrell and Olli Niemitalo
		http://yehar.com/blog/?p=665
		*/

		return (uint32_t) round((exp2(Value / 256.0) - 1) * 1024.0);
	};
	
	/* ADPCM-A data type */
	struct adpcma_t
	{
		struct channel_t
		{
			uint32_t KeyOn;			/* Key On / Off (flag) */

			uint32_t Level;			/* Channel level (5-bit) */
			 int16_t OutL;			/* Channel L output */
			 int16_t OutR;			/* Channel R output */
			uint32_t MaskL;			/* Channel L output mask */
			uint32_t MaskR;			/* Channel R output mask */

			pair32_t Start;			/* Start address (16-bit) */
			pair32_t End;			/* End address (16-bit) */
			uint32_t Addr;			/* Current address (16-bit) */

			 int16_t Signal;		/* Decoded ADPCM-A signal */
			 int32_t Step;			/* ADPCM-A step */
			uint32_t NibbleShift;	/* Nibble selection shift */
		};

		channel_t	Channel[6];
		uint32_t	TotalLevel;
		 int16_t	OutL;
		 int16_t	OutR;
	};

	/* ADPCM-B data type */
	struct adpcmb_t
	{
		uint8_t		Ctrl1;			/* Control 1 (8-bit) */
		uint8_t		Ctrl2;			/* Control 2 (8-bit) */
		pair32_t	Start;			/* Start address (16-bit) */
		pair32_t	Stop;			/* Stop address (16-bit) */
		pair32_t	Limit;			/* Limit address (16-bit) */
		pair32_t	Prescale;		/* Encoder prescaler (10-bit) */
		pair32_t	DeltaN;			/* Frequency delta (16-bit) */
		uint8_t		LevelCtrl;		/* Level control (8-bit) */
		uint32_t	MaskL;			/* Channel L output mask */
		uint32_t	MaskR;			/* Channel R output mask */
		 int16_t	OutL;			/* Channel L output */
		 int16_t	OutR;			/* Channel R output */

		uint32_t	Addr;			/* Current memory address */
		pair32_t	AddrDelta;		/* Address delta */
		uint32_t	Shift;			/* Address shift */

		int16_t		SignalT1;		/* Decoded ADPCM-B signal */
		int16_t		SignalT0;		/* Previous decoded ADPCM-B signal */
		int32_t		Step;			/* ADPCM-B step */
		uint32_t	NibbleShift;	/* Nibble selection shift */
	};

} // namespace YM

#endif // !_YM_H_