#include "stdafx.h"
#include "OBSCrashReporter.h"

BOOL CALLBACK module_name_callback(PCTSTR module_name, DWORD64 module_base, ULONG module_size, PVOID param)
{
	symbol_info_t *info = (symbol_info_t *)param;

	if (info->address >= module_base && info->address < module_base + module_size)
	{
		StringCbCopy(info->moduleName, sizeof(info->moduleName), module_name);
		return FALSE;
	}

	return TRUE;
}

BOOL GetModuleNameForAddress(HANDLE hProcess, symbol_info_t *info)
{
	info->moduleName[0] = 0;

	EnumerateLoadedModulesW64(hProcess, module_name_callback, info);

	if (!info->moduleName[0])
		return FALSE;

	return TRUE;
}

BOOL OutputThreadStack(HANDLE hProcess, DWORD threadID)
{
	STACKFRAME64 stackFrame = {0};
	CONTEXT context;
	HANDLE hThread;

	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, threadID);
	if (!hThread)
		return FALSE;

	context.ContextFlags = CONTEXT_ALL;
	GetThreadContext(hThread, &context);

	stackFrame.AddrPC.Offset = context.Rip;
	stackFrame.AddrFrame.Offset = context.Rbp;
	stackFrame.AddrStack.Offset = context.Rsp;

	stackFrame.AddrFrame.Mode = AddrModeFlat;
	stackFrame.AddrPC.Mode = AddrModeFlat;
	stackFrame.AddrStack.Mode = AddrModeFlat;

	while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProcess, hThread, &stackFrame, &context, nullptr,
		SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
	{
		DWORD64		offset;
		symbol_info_t	info;
		wchar_t		*moduleName;
		wchar_t		stackLine[1024];
		BOOL		success;

		info.address = stackFrame.AddrPC.Offset;

		info.symbol = (SYMBOL_INFOW *)LocalAlloc(LPTR, sizeof(*info.symbol) +  256);
		info.symbol->MaxNameLen = 256;
		info.symbol->SizeOfStruct = sizeof(*info.symbol);

		if (!GetModuleNameForAddress(hProcess, &info))
		{
			moduleName = L"<unknown>";
		}
		else
		{
			moduleName = wcsrchr(info.moduleName, '\\');
			if (!moduleName)
				moduleName = info.moduleName;
			else
				moduleName++;
		}

		success = SymFromAddrW(hProcess, info.address, &offset, info.symbol);

		if (success && !(info.symbol->Flags & SYMFLAG_EXPORT))
		{
			StringCbPrintf(stackLine, sizeof(stackLine), SUCCESS_FORMAT,
				stackFrame.AddrStack.Offset,
				stackFrame.AddrPC.Offset,
				stackFrame.Params[0],
				stackFrame.Params[1],
				stackFrame.Params[2],
				stackFrame.Params[3],
				moduleName, info.symbol->Name, offset);
		}
		else
		{
			StringCbPrintf(stackLine, sizeof(stackLine), FAIL_FORMAT,
				stackFrame.AddrStack.Offset,
				stackFrame.AddrPC.Offset,
				stackFrame.Params[0],
				stackFrame.Params[1],
				stackFrame.Params[2],
				stackFrame.Params[3],
				moduleName, stackFrame.AddrPC.Offset);
		}
	}

	return TRUE;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	if (!lpCmdLine[0])
		return -1;

	LPWSTR *argv;
	int argc;

	argv = CommandLineToArgvW (lpCmdLine, &argc);
	if (!argv)
		return -1;

	if (argc < 3)
		return -1;

	DWORD processID = wcstoul(argv[0], NULL, 10);
	if (!processID)
		return -1;

	DWORD threadID = wcstoul(argv[1], NULL, 10);

	uintptr_t *exceptionPtrs = (uintptr_t *)wcstoull(argv[2], NULL, 16);

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE, FALSE, processID);
	if (!hProcess)
		return 1;

	SymInitialize(hProcess, "..\\obs-plugins\\64bit;..\\obs-plugins\32bit;D:\\ProgramData\\Symbols", TRUE);
	SymSetOptions(SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS | SYMOPT_UNDNAME);

	HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, processID);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		return 1;

	OutputThreadStack(hProcess, threadID);

	THREADENTRY32 te = {0};
	te.dwSize = sizeof(te);
	if (Thread32First(hSnapshot, &te))
	{
		do
		{
			if (te.dwSize >= FIELD_OFFSET(THREADENTRY32, th32OwnerProcessID) + sizeof(te.th32OwnerProcessID))
			{
				if (te.th32OwnerProcessID != processID)
					continue;

				if (te.th32ThreadID == threadID)
					continue;

				OutputThreadStack(hProcess, te.th32ThreadID);
			}
		}
		while (Thread32Next(hSnapshot, &te));
	}
	CloseHandle(hSnapshot);

	HANDLE hMinidump = CreateFile(L"obs.dmp", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
	if (!hMinidump)
		return 1;

	PMINIDUMP_EXCEPTION_INFORMATION exceptionInformationPtr;
	MINIDUMP_EXCEPTION_INFORMATION exceptionInformation;

	if (exceptionPtrs && threadID)
	{
		exceptionInformation.ExceptionPointers = (PEXCEPTION_POINTERS)exceptionPtrs;
		exceptionInformation.ClientPointers = TRUE;
		exceptionInformation.ThreadId = threadID;
		exceptionInformationPtr = &exceptionInformation;
	}
	else
		exceptionInformationPtr = nullptr;

	if (!MiniDumpWriteDump(
		hProcess,
		processID,
		hMinidump,
		(MINIDUMP_TYPE)(MiniDumpWithProcessThreadData | MiniDumpWithThreadInfo | MiniDumpIgnoreInaccessibleMemory | MiniDumpWithUnloadedModules),
		exceptionInformationPtr,
		NULL,
		NULL
	))
	{
		HRESULT err = GetLastError();
		DebugBreak();
	}

	LocalFree(argv);

	return 0;
}

