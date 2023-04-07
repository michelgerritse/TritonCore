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
#ifndef _TRITON_CORE_H_
#define _TRITON_CORE_H_

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <climits>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

/* 32-bit memory pair */
union pair32_t
{
	/* Memory layout on little endian machines:
	
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

	void operator =(const uint32_t &data) /* assignment operator */
	{
		u32 = data;
	};

	uint32_t operator = (pair32_t &data)
	{
		data.u32 = u32;
		return data.u32;
	}
};

/* 16-bit memory pair */
union pair16_t
{
	/* Memory layout on little endian machines:

	15                                  0
	+-----------------------------------+
	|                u16                |
	+-----------------------------------+
	|       u8h       |       u8l       |
	+-----------------+-----------------+

	*/
	
	uint16_t u16;

	struct
	{
		uint8_t u8l, u8h;
	};

	void operator =(const uint16_t data)
	{
		u16 = data;
	};
};

namespace TritonCore
{
	/* Returns the parity of a given integral type */
	template<std::integral T>
	constexpr uint32_t GetParity(T value)
	{
		using uint = std::make_unsigned_t<T>;
		uint uValue(value);

		return std::popcount(uValue);
	}

} // namespace TritonCore

#endif // !_TRITON_CORE_H_
