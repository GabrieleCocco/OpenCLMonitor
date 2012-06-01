// OpenCLSpy.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "OpenCLSpy.h"
#ifdef NO_PRINT
#define printf(A) printf("")
#endif

NTSTATUS (NTAPI *pNtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG) = NULL;

int enable_debug_privilege() {
	LPCTSTR psz_privilege = SE_DEBUG_NAME;
	HANDLE hToken = 0;
    TOKEN_PRIVILEGES tkp = {0}; 

    // Get a token for this process.
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES |
                          TOKEN_QUERY, &hToken))
        return -1;

    // Get the LUID for the privilege. 
    if(LookupPrivilegeValue(NULL, psz_privilege,
                            &tkp.Privileges[0].Luid)) 
    {
        tkp.PrivilegeCount = 1;  // one privilege to set    
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        // Set the privilege for this process. 
        AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,
                              (PTOKEN_PRIVILEGES)NULL, 0); 
        if (GetLastError() != ERROR_SUCCESS)
           return -1;        
        return 0;
    }
    return -1;
}

int inject_dll(char *dllname, DWORD procID)
{
	char buf[MAX_PATH];
	LPVOID dllNameMem;
	HANDLE hProcess, hThread[2];
	LPVOID loadlibaddr, stringaddr;
	HMODULE findoff;
	DWORD exitcode, modfunc;
	int nStatus = 0;

	wchar_t w_dllname[MAX_PATH];
	mbstowcs(w_dllname, dllname, MAX_PATH);
        
	if((hProcess = OpenProcess(PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_CREATE_THREAD, FALSE, procID))){
		loadlibaddr = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
		if(loadlibaddr){
			dllNameMem = (LPVOID)VirtualAllocEx(hProcess, NULL, strlen(dllname), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			WriteProcessMemory(hProcess, (LPVOID)dllNameMem, dllname, strlen(dllname), NULL);
			hThread[0] = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadlibaddr, (LPVOID)dllNameMem, NULL, NULL);
			if(hThread[0]){
				WaitForSingleObject(hThread[0], INFINITE);
				GetExitCodeThread(hThread[0], &exitcode);
				findoff = LoadLibrary(w_dllname);
				if(findoff){
					modfunc = (DWORD)GetProcAddress(findoff, (LPSTR)1);
					exitcode += modfunc - (DWORD)findoff;
					hThread[1] = CreateRemoteThread(hProcess, NULL, NULL, (LPTHREAD_START_ROUTINE)exitcode, NULL, NULL, NULL);
					if(hThread[1]) nStatus = 1;
				}
			}
		}
		CloseHandle(hProcess);
	}
                
	return nStatus;
}

PPEB get_peb(HANDLE process) {
	NTSTATUS status;
	PROCESS_BASIC_INFORMATION pbi;
	PPEB pPeb;

	memset(&pbi, 0, sizeof(pbi));

	status = pNtQueryInformationProcess(
		process,
		ProcessBasicInformation,
		&pbi,
		sizeof(pbi),
		NULL);

	pPeb = NULL;
	if (NT_SUCCESS(status))
		pPeb = pbi.PebBaseAddress;
	return pPeb;
}


int main(int argc, char* argv[])
{
	if(argc < 2) {
		printf("Usage:\nOpenCLSpy <exepath> [args]\n");
		return 0;
	}

	char* exe_path = argv[1];
	wchar_t w_exe_path[MAX_PATH];
	mbstowcs(w_exe_path, exe_path, MAX_PATH);

	STARTUPINFO process_startup_info;
	PROCESS_INFORMATION process_information;	
	PROCESS_BASIC_INFORMATION process_basic_information;
	HANDLE process = INVALID_HANDLE_VALUE;
	
	process_startup_info.cb = sizeof(STARTUPINFO);
	ZeroMemory(&process_startup_info, sizeof(STARTUPINFO));
	ZeroMemory(&process_information, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&process_basic_information, sizeof(PROCESS_BASIC_INFORMATION));

	//Enabling debug privilege
	printf("%-40s", "Enabling debug privilege...");
	if(enable_debug_privilege() != 0) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");

	//Loading ntdll		
	printf("%-40s", "Dynamically loading ntdll.dll...");
    pNtQueryInformationProcess = (NTSTATUS(NTAPI*)(HANDLE, DWORD, PVOID, ULONG, PULONG))
        GetProcAddress(
            GetModuleHandle(TEXT("ntdll.dll")), 
            "NtQueryInformationProcess");

    if (pNtQueryInformationProcess == NULL)
    {
        printf("FAIL! (error 0x%08X)", GetLastError());
        return -1;
    }
	printf("DONE\n");

	//Create process in suspended state
	printf("%-40s", "Creating process in suspended state...", exe_path);
	bool status = CreateProcess(
		w_exe_path,
		NULL,
		NULL,
		NULL,
		false,
		CREATE_SUSPENDED,
		NULL,
		NULL,
		&process_startup_info,
		&process_information);
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");

	//Query process information
	printf("%-40s", "Retrieving process basic info...");
	PPEB peb = get_peb(process);
	if(peb == NULL) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");
	printf("- PEB base address = 0x%08X\n", peb);

	//Get DOS header
	printf("Retrieving process DOS header...");
	SIZE_T bytes_read = 0;
	IMAGE_DOS_HEADER dos_header;
	IMAGE_NT_HEADERS nt_headers;
    status = ReadProcessMemory(process, 
		(LPCVOID)peb->Reserved3[1], 
        &dos_header, 
        sizeof(IMAGE_DOS_HEADER),
        &bytes_read);
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");

	//Get NT headers
	printf("Retrieving process NT headers..");
	status = ReadProcessMemory(
		process, 
		(LPCVOID)(((DWORD)peb->Reserved3[1]) + dos_header.e_lfanew), 
        &nt_headers, 
		sizeof(IMAGE_NT_HEADERS), 
        &bytes_read);	
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");

	printf("Allocating and reading process image...");
	PCHAR image_memory = (PCHAR)GlobalAlloc(GMEM_FIXED | GMEM_ZEROINIT, nt_headers.OptionalHeader.SizeOfImage);
	status = ReadProcessMemory(
		process, 
		(LPCVOID)(peb->Reserved3[1]), 
		image_memory,
		nt_headers.OptionalHeader.SizeOfImage, 
		&bytes_read);	
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");
	printf("- Image size = %lu bytes\n", nt_headers.OptionalHeader.SizeOfImage);
	printf("- Image base address = 0x%08X\n", nt_headers.OptionalHeader.ImageBase);
	printf("- Addess of entry point = 0x%08X (absolute 0x%08X)\n", nt_headers.OptionalHeader.AddressOfEntryPoint, nt_headers.OptionalHeader.ImageBase + nt_headers.OptionalHeader.AddressOfEntryPoint);

	printf("Reading first two bytes of instructions...");
	char buffer[2];
	status = ReadProcessMemory(
		process, 
		(LPCVOID)(nt_headers.OptionalHeader.ImageBase + nt_headers.OptionalHeader.AddressOfEntryPoint),
		(LPVOID)buffer,
		2,
		&bytes_read);	
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n");
	
	printf("Patching first two bytes of instructions...");
	status = WriteProcessMemory(
		process, 
		(LPVOID)(nt_headers.OptionalHeader.ImageBase + nt_headers.OptionalHeader.AddressOfEntryPoint),
		(LPVOID)"\xEB\xFE",
		2,
		&bytes_read);	
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n"); 

	//Get thread context
	printf("Retrieving thread context...");
	CONTEXT thread_context;
	status = GetThreadContext(process_information.hThread, &thread_context);
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n"); 
	
	//Redirecting thread to the entrypoint
	printf("Redirecting thread to the entrypoint...");
	thread_context.Eip = nt_headers.OptionalHeader.AddressOfEntryPoint;
	status = SetThreadContext(process_information.hThread, &thread_context);
	if(!status) {
		printf("FAIL!");
		return -1;
	}
	printf("DONE\n"); 
	
	printf("Resuming thread...");
	ResumeThread(process_information.hThread);

	GlobalFree(image_memory);
	//TerminateThread(process_information.hThread, 0);
	//TerminateProcess(process, 0);
	CloseHandle(process);
	return 0;
}


