/*
 _____    _ _            ___
|_   _| _(_) |_ ___ _ _ / __|___ _ _ ___
  | || '_| |  _/ _ \ ' \ (__/ _ \ '_/ -_)
  |_||_| |_|\__\___/_||_\___\___/_| \___|

Copyright © 2023, Michel Gerritse
All rights reserved.

This source code is available under the BSD-style license.
See LICENSE.txt file in the root directory of this source tree.

*/
#ifndef _TRITON_CORE_H_
#define _TRITON_CORE_H_

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/* 32-bit memory pair */
typedef union
{
	/* Memory layout:
	
	31                                  0
	+-----------------------------------+
	|                u32                |
	+-----------------------------------+
	|      u16h       |      u16l       |
	+-----------------+-----------------+
	|  u8hh  |  u8hl  |  u8lh  |  u8ll  |
	+--------+--------+--------+--------+

	*/
	
	uint32_t u32;
	
	struct
	{ 
		uint16_t u16l, u16h;
	};
	
	struct
	{
		uint8_t u8ll, u8lh, u8hl, u8hh;
	};
}pair32_t;

#endif // !_TRITON_CORE_H_
