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
	/// <summary>Get the n-th bit of a given integer.</summary>
	/// <param name= "Value">Integer value (signed and unsigned).</param>
	/// <param name="BitPos">Bit position (n-th bit).</param>
	/// <returns>The value of the bit (0 or 1) at the n-th position.</returns>
	template<std::integral T>
	constexpr auto GetBit(T Value, T BitPos)
	{
		return (Value >> BitPos) & 1;
	}

	/// <summary>Set the n-th bit of a given integer.</summary>
	/// <param name= "Value">Integer value (signed and unsigned).</param>
	/// <param name="BitPos">Bit position (n-th bit).</param>
	/// <returns>New value of <paramref name="Value"/>.</returns>
	template<std::integral T>
	constexpr auto SetBit(T Value, T BitPos)
	{
		return Value | (1 << BitPos);
	}

	/// <summary>Clear the n-th bit of a given integer.</summary>
	/// <param name= "Value">Integer value (signed and unsigned).</param>
	/// <param name="BitPos">Bit position (n-th bit).</param>
	/// <returns>New value of <paramref name="Value"/></returns>
	template<std::integral T>
	constexpr auto ClearBit(T Value, T BitPos)
	{
		return Value & ~(1 << BitPos);
	}

	/// <summary>Toggle the n-th bit of a given integer.</summary>
	/// <param name= "Value">Integer value (signed and unsigned).</param>
	/// <param name="BitPos">Bit position (n-th bit).</param>
	/// <returns>New value of <paramref name="Value"/></returns>
	template<std::integral T>
	constexpr auto FlipBit(T Value, T BitPos)
	{
		return Value ^ (1 << BitPos);
	}

	/// <summary>Test the n-th bit of a given integer.</summary>
	/// <param name= "Value">Integer value (signed and unsigned).</param>
	/// <param name="BitPos">Bit position (n-th bit).</param>
	/// <returns>True if the n-th bit is set, false otherwise.</returns>
	template<std::integral T>
	constexpr bool TestBit(T Value, T BitPos)
	{
		return Value & (1 << BitPos);
	}
	
	/// <summary>Get the parity of a given integer</summary>
	/// <param name="Value">Integer value (signed and unsigned)</param>
	/// <returns>The number of bits set to '1' in <paramref name="Value"/></returns>
	/// <remarks>Signed integers will be converted to unsigned integers</remarks>
	template<std::integral T>
	constexpr auto GetParity(T Value)
	{
		using uint = std::make_unsigned_t<T>;
		uint uValue(Value);

		return std::popcount(uValue);
	}

	/// <summary>Determine if a given integer is a power of 2</summary>
	/// <param name="Value">Integer value (signed and unsigned)</param>
	/// <returns>True if power of 2 and greater than 0, false otherwise</returns>
	template<std::integral T>
	constexpr bool IsPowerOfTwo(T Value)
	{
		return (Value && !(Value & (Value - 1)));
	}
}

#endif // !_TRITON_CORE_BIT_H_