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
#ifndef _TRITON_CORE_BIT_H_
#define _TRITON_CORE_BIT_H_

#include <bit>
#include <bitset>
#include <concepts>
#include <cstdint>

/// <summary>TritonCore API version 1</summary>
namespace TritonCore_v1
{
	/// <summary>Get the parity of a given integral value</summary>
	/// <param name="Value">Integer value</param>
	/// <returns>The number of bits set to '1' in <paramref name="Value"/></returns>
	/// <remarks>Signed integers will be converted to unsigned integers</remarks>
	template<std::integral T>
	constexpr auto GetParity(T Value)
	{
		using uint = std::make_unsigned_t<T>;
		uint uValue(Value);

		return std::popcount(uValue);
	}

	/// <summary>Determine if a given integral value is a power of 2</summary>
	/// <param name="Value">Integer value</param>
	/// <returns>True if power of 2, false otherwise</returns>
	template<std::integral T>
	constexpr auto IsPowerOfTwo(T Value)
	{
		constexpr size_t NumBits = static_cast<size_t>(CHAR_BIT * sizeof(T));
		
		return std::bitset<NumBits>(Value).count() == 1;
	}
}

#endif // !_TRITON_CORE_BIT_H_