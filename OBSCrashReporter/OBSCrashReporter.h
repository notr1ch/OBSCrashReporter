#pragma once

#include "resource.h"
#include <TlHelp32.h>
#include <strsafe.h>

typedef struct
{
	uintptr_t	address;
	SYMBOL_INFOW	*symbol;
	wchar_t		moduleName[MAX_PATH];
} symbol_info_t;

#ifdef _WIN64
#define SUCCESS_FORMAT \
	L"%016I64X %016I64X %016I64X %016I64X " \
	"%016I64X %016I64X %s!%s+0x%I64x\r\n"
#define FAIL_FORMAT \
	L"%016I64X %016I64X %016I64X %016I64X %016I64X %016I64X %s!0x%I64x\r\n"
/*#else
#define SUCCESS_FORMAT \
L"%08.8I64X %08.8I64X %08.8I64X %08.8I64X %08.8I64X %08.8I64X %s!%s+0x%I64x\r\n"
#define FAIL_FORMAT \
L"%08.8I64X %08.8I64X %08.8I64X %08.8I64X %08.8I64X %08.8I64X %s!0x%I64x\r\n"

stackFrame.AddrStack.Offset &= 0xFFFFFFFFF;
stackFrame.AddrPC.Offset &= 0xFFFFFFFFF;
stackFrame.Params[0] &= 0xFFFFFFFF;
stackFrame.Params[1] &= 0xFFFFFFFF;
stackFrame.Params[2] &= 0xFFFFFFFF;
stackFrame.Params[3] &= 0xFFFFFFFF;*/
#endif