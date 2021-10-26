/*
Copyright (c) 2015-2021 Frank Sapone <fhsapone@gmail.com>

Permission to use, copy, modify, and distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "master.h"
#include <winbase.h>
#include <stdio.h>

#if _MSC_VER < 1400 /* FS: VC6 need hjalp. */
#include "dbghelp_ext.h"
#else
#include <dbghelp.h>
#endif

#define PRODUCTNAME "GSMasterServer"

typedef BOOL (WINAPI *ENUMERATELOADEDMODULES64) (HANDLE hProcess, PENUMLOADED_MODULES_CALLBACK64 EnumLoadedModulesCallback, PVOID UserContext);
typedef DWORD (WINAPI *SYMSETOPTIONS) (DWORD SymOptions);
typedef BOOL (WINAPI *SYMINITIALIZE) (HANDLE hProcess, PSTR UserSearchPath, BOOL fInvadeProcess);
typedef BOOL (WINAPI *SYMCLEANUP) (HANDLE hProcess);
typedef BOOL (WINAPI *STACKWALK64) (
	DWORD MachineType,
	HANDLE hProcess,
	HANDLE hThread,
	LPSTACKFRAME64 StackFrame,
	PVOID ContextRecord,
	PREAD_PROCESS_MEMORY_ROUTINE64 ReadMemoryRoutine,
	PFUNCTION_TABLE_ACCESS_ROUTINE64 FunctionTableAccessRoutine,
	PGET_MODULE_BASE_ROUTINE64 GetModuleBaseRoutine,
	PTRANSLATE_ADDRESS_ROUTINE64 TranslateAddress
	);

typedef PVOID	(WINAPI *SYMFUNCTIONTABLEACCESS64) (HANDLE hProcess, DWORD64 AddrBase);
typedef DWORD64 (WINAPI *SYMGETMODULEBASE64) (HANDLE hProcess, DWORD64 dwAddr);
typedef BOOL	(WINAPI *SYMFROMADDR) (HANDLE hProcess, DWORD64 Address, PDWORD64 Displacement, PSYMBOL_INFO Symbol);
typedef BOOL	(WINAPI *SYMGETMODULEINFO64) (HANDLE hProcess, DWORD64 dwAddr, PIMAGEHLP_MODULE64 ModuleInfo);

typedef DWORD64 (WINAPI *SYMLOADMODULE64) (HANDLE hProcess, HANDLE hFile, PSTR ImageName, PSTR ModuleName, DWORD64 BaseOfDll, DWORD SizeOfDll);

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP) (
	HANDLE hProcess,
	DWORD ProcessId,
	HANDLE hFile,
	MINIDUMP_TYPE DumpType,
	PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
	PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
	PMINIDUMP_CALLBACK_INFORMATION CallbackParam
	);

typedef HINSTANCE (WINAPI *SHELLEXECUTEA) (HWND hwnd, LPCTSTR lpOperation, LPCTSTR lpFile, LPCTSTR lpParameters, LPCTSTR lpDirectory, INT nShowCmd);

SYMGETMODULEINFO64	fnSymGetModuleInfo64;
SYMLOADMODULE64		fnSymLoadModule64;

typedef BOOL (WINAPI *VERQUERYVALUE) (const LPVOID pBlock, LPTSTR lpSubBlock, LPVOID lplpBuffer, LPUINT puLen);
typedef DWORD (WINAPI *GETFILEVERSIONINFOSIZE) (LPTSTR lptstrFilename, LPDWORD lpdwHandle);
typedef BOOL (WINAPI *GETFILEVERSIONINFO) (LPTSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);

VERQUERYVALUE			fnVerQueryValue;
GETFILEVERSIONINFOSIZE	fnGetFileVersionInfoSize;
GETFILEVERSIONINFO		fnGetFileVersionInfo;

typedef BOOL (WINAPI *ISDEBUGGERPRESENT) (VOID); /* FS: Not on Win9x. */

ISDEBUGGERPRESENT		fnIsDebuggerPresent;

char szModuleName[MAX_PATH];

int Sys_FileLength (const char *path)
{
	WIN32_FILE_ATTRIBUTE_DATA	fileData;

	if (GetFileAttributesEx (path, GetFileExInfoStandard, &fileData))
		return (int)fileData.nFileSizeLow;
	else
		return -1;
}

BOOL CALLBACK EnumerateLoadedModulesProcDump (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	VS_FIXEDFILEINFO *fileVersion;
	BYTE *verInfo;
	DWORD	dummy, len;
	FILE *fhReport = (FILE *)UserContext;
	CHAR	verString[32];
	CHAR	lowered[MAX_PATH];

	strncpy (lowered, ModuleName, sizeof(lowered) - 1);
	strlwr (lowered);

	if (fnGetFileVersionInfo && fnVerQueryValue && fnGetFileVersionInfoSize)
	{
		if (len = (fnGetFileVersionInfoSize (ModuleName, &dummy)))
		{
			verInfo = (BYTE *)LocalAlloc (LPTR, len);
			if (fnGetFileVersionInfo (ModuleName, dummy, len, verInfo))
			{
				if (fnVerQueryValue (verInfo, "\\", (LPVOID)&fileVersion, (LPUINT)&dummy))
				{
					_snprintf (verString, sizeof(verString), "%d.%d.%d.%d", HIWORD(fileVersion->dwFileVersionMS), LOWORD(fileVersion->dwFileVersionMS), HIWORD(fileVersion->dwFileVersionLS), LOWORD(fileVersion->dwFileVersionLS));
				}
				else
				{
					strncpy (verString, "unknown", sizeof(verString) - 1);
				}
			}
			else
			{
				strncpy (verString, "unknown", sizeof(verString) - 1);
			}

			LocalFree (verInfo);
		}
		else
		{
			strncpy (verString, "unknown", sizeof(verString) - 1);
		}
	}
	else
	{
		strncpy (verString, "unknown", sizeof(verString) - 1);
	}

#ifdef _M_AMD64
	fprintf (fhReport, "[0x%16I64X - 0x%16I64X] %s (%lu bytes, version %s)\r\n", ModuleBase, ModuleBase + (DWORD64)ModuleSize, ModuleName, ModuleSize, verString);
#else
	fprintf (fhReport, "[0x%08I64X - 0x%08I64X] %s (%lu bytes, version %s)\r\n", ModuleBase, ModuleBase + (DWORD64)ModuleSize, ModuleName, ModuleSize, verString);
#endif
	return TRUE;
}

BOOL CALLBACK EnumerateLoadedModulesProcInfo (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	DWORD64	addr = (DWORD64)UserContext;
	if (addr > ModuleBase && addr < ModuleBase + ModuleSize)
	{
		strncpy (szModuleName, ModuleName, sizeof(szModuleName) - 1);
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK EnumerateLoadedModulesProcSymInfo (PSTR ModuleName, DWORD64 ModuleBase, ULONG ModuleSize, PVOID UserContext)
{
	IMAGEHLP_MODULE64	symInfo = { 0 };
	FILE *fhReport = (FILE *)UserContext;
	PCHAR				symType;

	symInfo.SizeOfStruct = sizeof(symInfo);

	if (fnSymGetModuleInfo64 (GetCurrentProcess(), ModuleBase, &symInfo))
	{
		switch (symInfo.SymType)
		{
			case SymCoff:
				symType = "COFF";
				break;
			case SymCv:
				symType = "CV";
				break;
			case SymExport:
				symType = "Export";
				break;
			case SymPdb:
				symType = "PDB";
				break;
			case SymNone:
				symType = "No";
				break;
			default:
				symType = "Unknown";
				break;
		}

		fprintf (fhReport, "%s, %s symbols loaded.\r\n", symInfo.LoadedImageName, symType);
	}
	else
	{
		int i = GetLastError ();
		fprintf (fhReport, "%s, couldn't check symbols %d.\r\n", ModuleName, i);
	}

	return TRUE;
}

DWORD GSMasterServerExceptionHandler (DWORD exceptionCode, LPEXCEPTION_POINTERS exceptionInfo)
{
	FILE *fhReport;

	HANDLE	hProcess;

	HMODULE	hDbgHelp, hVersion;
#ifndef _DEBUG
	HMODULE hKernel32;
#endif

	MINIDUMP_EXCEPTION_INFORMATION miniInfo;
	STACKFRAME64	frame = { 0 };
	CONTEXT			context = *exceptionInfo->ContextRecord;
	SYMBOL_INFO *symInfo;
	DWORD64			fnOffset;
	CHAR			tempPath[MAX_PATH];
	CHAR			dumpPath[MAX_PATH];
	OSVERSIONINFOEX	osInfo;
	SYSTEMTIME		timeInfo;

	ENUMERATELOADEDMODULES64	fnEnumerateLoadedModules64;
	SYMSETOPTIONS				fnSymSetOptions;
	SYMINITIALIZE				fnSymInitialize;
	STACKWALK64					fnStackWalk64;
	SYMFUNCTIONTABLEACCESS64	fnSymFunctionTableAccess64;
	SYMGETMODULEBASE64			fnSymGetModuleBase64;
	SYMFROMADDR					fnSymFromAddr;
	SYMCLEANUP					fnSymCleanup;
	MINIDUMPWRITEDUMP			fnMiniDumpWriteDump;

	DWORD						ret, i;
	DWORD						machineType;
	DWORD64						InstructionPtr;

	CHAR						searchPath[MAX_PATH], *p;

#ifdef _DEBUG
	if (!bMinidumpAutogen)
	{
		ret = MessageBox (NULL, "EXCEPTION_CONTINUE_SEARCH?", "Unhandled Exception", MB_ICONERROR | MB_YESNO);
		if (ret == IDYES)
		{
			return EXCEPTION_CONTINUE_SEARCH;
		}
	}
#endif

#ifndef _DEBUG
	hKernel32 = LoadLibrary ("KERNEL32");
	if (hKernel32)
	{
		fnIsDebuggerPresent = (ISDEBUGGERPRESENT)GetProcAddress(hKernel32, "IsDebuggerPresent");
		if ((fnIsDebuggerPresent != NULL) && fnIsDebuggerPresent())
			return EXCEPTION_CONTINUE_SEARCH;

		FreeLibrary(hKernel32);
		hKernel32 = NULL;
	}
#endif

	hDbgHelp = LoadLibrary ("DBGHELP");
	hVersion = LoadLibrary ("VERSION");

	if (!hDbgHelp)
	{
		MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception and must be terminated. No crash report could be generated since GSMasterServer failed to load DBGHELP.DLL. Please obtain DBGHELP.DLL and place it in your GSMasterServer directory to enable crash dump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (hVersion)
	{
		fnVerQueryValue = (VERQUERYVALUE)GetProcAddress (hVersion, "VerQueryValueA");
		fnGetFileVersionInfo = (GETFILEVERSIONINFO)GetProcAddress (hVersion, "GetFileVersionInfoA");
		fnGetFileVersionInfoSize = (GETFILEVERSIONINFOSIZE)GetProcAddress (hVersion, "GetFileVersionInfoSizeA");
	}

	fnEnumerateLoadedModules64 = (ENUMERATELOADEDMODULES64)GetProcAddress (hDbgHelp, "EnumerateLoadedModules64");
	fnSymSetOptions = (SYMSETOPTIONS)GetProcAddress (hDbgHelp, "SymSetOptions");
	fnSymInitialize = (SYMINITIALIZE)GetProcAddress (hDbgHelp, "SymInitialize");
	fnSymFunctionTableAccess64 = (SYMFUNCTIONTABLEACCESS64)GetProcAddress (hDbgHelp, "SymFunctionTableAccess64");
	fnSymGetModuleBase64 = (SYMGETMODULEBASE64)GetProcAddress (hDbgHelp, "SymGetModuleBase64");
	fnStackWalk64 = (STACKWALK64)GetProcAddress (hDbgHelp, "StackWalk64");
	fnSymFromAddr = (SYMFROMADDR)GetProcAddress (hDbgHelp, "SymFromAddr");
	fnSymCleanup = (SYMCLEANUP)GetProcAddress (hDbgHelp, "SymCleanup");
	fnSymGetModuleInfo64 = (SYMGETMODULEINFO64)GetProcAddress (hDbgHelp, "SymGetModuleInfo64");
	fnMiniDumpWriteDump = (MINIDUMPWRITEDUMP)GetProcAddress (hDbgHelp, "MiniDumpWriteDump");

	if (!fnEnumerateLoadedModules64 || !fnSymSetOptions || !fnSymInitialize || !fnSymFunctionTableAccess64 ||
		!fnSymGetModuleBase64 || !fnStackWalk64 || !fnSymFromAddr || !fnSymCleanup || !fnSymGetModuleInfo64)// ||
		//!fnSymLoadModule64)
	{
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception. No crash report could be generated since the version of DBGHELP.DLL in use is too old. Please obtain an up-to-date DBGHELP.DLL and place it in your GSMasterServer directory to enable minidump generation.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (!bMinidumpAutogen)
	{
		ret = IDYES;
	}
	else
	{
		ret = MessageBox (NULL, PRODUCTNAME " has encountered an unhandled exception. Would you like to generate a minidump?", "Unhandled Exception", MB_ICONEXCLAMATION | MB_YESNO);
	}

	if (ret == IDNO)
	{
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	hProcess = GetCurrentProcess();

	fnSymSetOptions (SYMOPT_UNDNAME | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_LOAD_ANYTHING);

	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p) p[0] = 0;

	GetSystemTime (&timeInfo);

	i = 1;

	for (;;)
	{
		_snprintf (tempPath, sizeof(tempPath) - 1, "%s\\GSMasterServer_Crash_Log%.4d-%.2d-%.2d_%lu.txt", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
		if (Sys_FileLength(tempPath) == -1)
			break;
		i++;
	}

	fhReport = fopen (tempPath, "wb");

	if (!fhReport)
	{
		FreeLibrary (hDbgHelp);
		if (hVersion)
			FreeLibrary (hVersion);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	fnSymInitialize (hProcess, searchPath, TRUE);

#ifdef _DEBUG
	GetModuleFileName (NULL, searchPath, sizeof(searchPath));
	p = strrchr (searchPath, '\\');
	if (p) p[0] = 0;
#endif


#ifdef _M_AMD64
	machineType = IMAGE_FILE_MACHINE_AMD64;
	InstructionPtr = context.Rip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrStack.Offset = context.Rsp;
#else
	machineType = IMAGE_FILE_MACHINE_I386;
	InstructionPtr = context.Eip;
	frame.AddrPC.Offset = InstructionPtr;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif

	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	symInfo = (SYMBOL_INFO *)LocalAlloc (LPTR, sizeof(*symInfo) + 128);
	symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
	symInfo->MaxNameLen = 128;
	fnOffset = 0;

	memset (&osInfo, 0, sizeof(osInfo));
	osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	if (!GetVersionEx ((OSVERSIONINFO *)&osInfo))
	{
		osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		GetVersionEx ((OSVERSIONINFO *)&osInfo);
	}

	strncpy (szModuleName, "<unknown>", sizeof(szModuleName) - 1);
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)InstructionPtr);

	strlwr (szModuleName);

	fprintf (fhReport, "**** UNHANDLED EXCEPTION: %x\r\nFault address: %I64X (%s)\r\n", exceptionCode, InstructionPtr, szModuleName);

	fprintf (fhReport, PRODUCTNAME " module: %s\r\n", szModuleName);
	fprintf (fhReport, "Windows version: %lu.%lu (Build %lu) %s\r\n\r\n", osInfo.dwMajorVersion, osInfo.dwMinorVersion, osInfo.dwBuildNumber, osInfo.szCSDVersion);

	fprintf (fhReport, "Symbol information:\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcSymInfo, (VOID *)fhReport);

	fprintf (fhReport, "\r\nEnumerate loaded modules:\r\n");
	fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcDump, (VOID *)fhReport);

	fprintf (fhReport, "\r\nStack trace:\r\n");
	fprintf (fhReport, "Stack    EIP      Arg0     Arg1     Arg2     Arg3     Address\r\n");
	while (fnStackWalk64 (machineType, hProcess, GetCurrentThread(), &frame, &context, NULL, (PFUNCTION_TABLE_ACCESS_ROUTINE64)fnSymFunctionTableAccess64, (PGET_MODULE_BASE_ROUTINE64)fnSymGetModuleBase64, NULL))
	{
		strncpy (szModuleName, "<unknown>", sizeof(szModuleName) - 1);
		fnEnumerateLoadedModules64 (hProcess, (PENUMLOADED_MODULES_CALLBACK64)EnumerateLoadedModulesProcInfo, (VOID *)frame.AddrPC.Offset);
		strlwr (szModuleName);

		p = strrchr (szModuleName, '\\');
		if (p)
		{
			p++;
		}
		else
		{
			p = szModuleName;
		}

#ifdef _M_AMD64
		if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
		{
			fprintf (fhReport, "%16I64X %16I64X %16I64X %16I64X %16I64X %16I64X %s!%s+0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, frame.Params[0], frame.Params[1], frame.Params[2], frame.Params[3], p, symInfo->Name, fnOffset);
		}
		else
		{
			fprintf (fhReport, "%16I64X %16I64X %16I64X %16I64X %16I64X %16I64X %s!0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, frame.Params[0], frame.Params[1], frame.Params[2], frame.Params[3], p, frame.AddrPC.Offset);
		}
#else
		if (fnSymFromAddr (hProcess, frame.AddrPC.Offset, &fnOffset, symInfo) && !(symInfo->Flags & SYMFLAG_EXPORT))
		{
			fprintf (fhReport, "%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!%s+0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, symInfo->Name, fnOffset);
		}
		else
		{
			fprintf (fhReport, "%08.8I64X %08.8I64X %08.8X %08.8X %08.8X %08.8X %s!0x%I64x\r\n", frame.AddrStack.Offset, frame.AddrPC.Offset, (DWORD)frame.Params[0], (DWORD)frame.Params[1], (DWORD)frame.Params[2], (DWORD)frame.Params[3], p, frame.AddrPC.Offset);
		}
#endif
	}

	if (fnMiniDumpWriteDump)
	{
		HANDLE	hFile;
		CHAR	cdFile[MAX_PATH];

		_snprintf(cdFile, sizeof(cdFile) - 1, "GSMasterServer_Crash_Dump%.4d-%.2d-%.2d_%lu.dmp", timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);

		GetTempPath (sizeof(dumpPath) - 16, dumpPath);
		strncat(dumpPath, cdFile, sizeof(dumpPath) - 1);

		hFile = CreateFile (dumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile != INVALID_HANDLE_VALUE)
		{
			miniInfo.ClientPointers = TRUE;
			miniInfo.ExceptionPointers = exceptionInfo;
			miniInfo.ThreadId = GetCurrentThreadId ();
			if (fnMiniDumpWriteDump (hProcess, GetCurrentProcessId(), hFile, (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithDataSegs), &miniInfo, NULL, NULL))
			{
				CHAR	zPath[MAX_PATH];

				CloseHandle (hFile);

				_snprintf (zPath, sizeof(zPath) - 1, "%s\\GSMasterServer_Crash_Log%.4d-%.2d-%.2d_%lu.dmp", searchPath, timeInfo.wYear, timeInfo.wMonth, timeInfo.wDay, i);
				CopyFile (dumpPath, zPath, FALSE);
				DeleteFile (dumpPath);

				strncpy (dumpPath, zPath, sizeof(dumpPath) - 1);
				if (!bMinidumpAutogen)
				{
					MessageBox (NULL, "Minidump succesfully generated.  Please include this file when submitting a crash report to https://bitbucket.org/maraakate/gsmaster/issues.", "Unhandled Exception", MB_OK | MB_ICONEXCLAMATION);
				}
			}
			else
			{
				CloseHandle (hFile);
				DeleteFile (dumpPath);
			}
		}
	}
	else
	{
		fprintf (fhReport, "\r\nFailed to generate minidump.\r\n");
	}

	fclose (fhReport);

	LocalFree (symInfo);

	fnSymCleanup (hProcess);

	{
		HMODULE shell;
		shell = LoadLibrary ("SHELL32");
		if (shell)
		{
			SHELLEXECUTEA fncOpen = (SHELLEXECUTEA)GetProcAddress (shell, "ShellExecuteA");
			if (fncOpen)
				fncOpen (NULL, NULL, tempPath, NULL, searchPath, SW_SHOWDEFAULT);

			FreeLibrary (shell);
		}
	}

	FreeLibrary (hDbgHelp);
	if (hVersion)
		FreeLibrary (hVersion);

	return EXCEPTION_EXECUTE_HANDLER;
}
