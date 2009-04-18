/* Copyright (C) 2009 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * =========================================================================
 * File        : dbghelp.h
 * Project     : 0 A.D.
 * Description : bring in dbghelp library
 * =========================================================================
 */

#ifndef INCLUDED_DBGHELP
#define INCLUDED_DBGHELP

#include "win.h"

#define _NO_CVCONST_H	// request SymTagEnum be defined
#include <dbghelp.h>	// must come after win.h
#include <OAIdl.h>	// VARIANT

#if MSC_VERSION
# pragma comment(lib, "dbghelp.lib")
# pragma comment(lib, "oleaut32.lib")	// VariantChangeType
#endif

#endif	// #ifndef INCLUDED_DBGHELP
