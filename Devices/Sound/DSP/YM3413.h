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
#ifndef _YM3413_H_
#define _YM3413_H_

#include "..\..\..\TritonCore.h"

/* Yamaha YM3413 (LDSP) */
class YM3413
{
public:
	YM3413(size_t MemorySize);
	~YM3413() = default;

	void InitialClear();
	void ResetCommandCounter();
	void SendCommandData(uint8_t Command);
	void SendSerialInput0(int16_t ChL, int16_t ChR);
	void SendSerialInput1(int16_t ChL, int16_t ChR);
	void GetSerialOutput0(int16_t* pChL, int16_t* pChR);
	void GetSerialOutput1(int16_t* pChL, int16_t* pChR);

private:
	std::vector<uint8_t>	m_Memory;
	uint32_t				m_CommandCounter;
};

#endif // !_YM3413_H_