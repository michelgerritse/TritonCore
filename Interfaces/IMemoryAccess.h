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
#ifndef _IMEMORY_ACCESS_H_
#define _IMEMORY_ACCESS_H_

#include "TritonCore.h"

/* Abstract private device memory interface */
struct __declspec(novtable) IMemoryAccess
{
	/*
		This interface allows direct (backdoor) access to memory that is normally
		private to a device (eg. sound ROM/RAM). It can be used to upload ROM dumps,
		download SRAM or for VGM style players that need this type of direct access.
	*/

	/* Copy data to memory (no banking) */
	virtual void CopyToMemory(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size) = 0;
	
	/* Copy data to memory, take any banking into account */
	virtual void CopyToMemoryIndirect(uint32_t MemoryID, size_t Offset, uint8_t* Data, size_t Size) = 0;

	//TODO:
	//virtual size_t GetMemorySize() = 0;
	//virtual void SetMemorySize(size_t Size) = 0;
	//virtual void WriteMemoryDirect(uint32_t Offset, uint8_t Data) = 0;
	//virtual void WriteMemoryIndirect(uint32_t Offset, uint8_t Data) = 0;
	//virtual uint8_t ReadMemoryDirect(uint32_t Offset) = 0;
	//virtual uint8_t ReadMemoryIndirect(uint32_t Offset) = 0;
};

#endif // !_IMEMORY_ACCESS_H_