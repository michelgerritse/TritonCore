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
#ifndef _IBASE_H_
#define _IBASE_H_

#include "../TritonCore.h"

/* Abstract base interface */
struct __declspec(novtable) IBase
{
	/* Increase reference count on object */
	virtual void		IncRef() = 0;
	
	/* Decrease reference count on object */
	virtual void		DecRef() = 0;
	
	/* Get reference count for object */
	virtual uint32_t	GetRefCount() = 0;
};

#endif // !_IBASE_H_
