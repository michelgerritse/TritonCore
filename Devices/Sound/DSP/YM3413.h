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
	void SendCommandData(uint32_t Command);
	void ProcessChannel0(int16_t* pChanL, int16_t* pChanR);
	void ProcessChannel1(int16_t* pChanL, int16_t* pChanR);

private:
	std::vector<uint8_t>	m_Memory;
	uint32_t				m_CommandCounter;

	uint8_t					m_Volume;
};

#endif // !_YM3413_H_