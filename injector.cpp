#include <Windows.h>
#include <iostream>
#include <Tlhelp32.h>

DWORD FindProcessID(char *ProgramName)
{
    HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(INVALID_HANDLE_VALUE == Snapshot)
        return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32); // Must be set or Process32First fails

    if(!Process32First(Snapshot, &pe32))
    {
        CloseHandle(Snapshot);
        printf("Problem with creating snapshot.\n");
        return false;
    }

    do
    {
        if(strcmp(ProgramName, pe32.szExeFile) == 0)
        {
            CloseHandle(Snapshot);
            return pe32.th32ProcessID;
        }
    }
    while(Process32Next(Snapshot, &pe32));

    CloseHandle(Snapshot);
    return false;
}

BOOL InjectDLL(HANDLE TargetProcess, char *DLLPath)
{
    LPVOID LoadLibraryAddress = (LPVOID)GetProcAddress(GetModuleHandle("kernel32.dll"), "LoadLibraryA");
    if(!LoadLibraryAddress)
    {
        printf("LoadLibrary not found.\n");
        return false;
    }

    LPVOID DLLMemoryLocation = VirtualAllocEx(TargetProcess, 
                                              NULL, 
                                              strlen(DLLPath), 
                                              MEM_COMMIT | MEM_RESERVE, 
                                              PAGE_EXECUTE_READWRITE);
    if(!DLLMemoryLocation)
    {
        printf("Could not allocate memory.\n");
        return false;
    }

    BOOL WritePath = WriteProcessMemory(TargetProcess,
                                        DLLMemoryLocation,
                                        DLLPath,
                                        strlen(DLLPath),
                                        NULL);
    if(!WritePath) 
    {
        printf("Could not write path.\n");
        return false;
    }

    HANDLE RemoteThread = CreateRemoteThread(TargetProcess,
                                             NULL,
                                             0,
                                             (LPTHREAD_START_ROUTINE)LoadLibraryAddress,
                                             DLLMemoryLocation,
                                             0,
                                             NULL);
    if(!RemoteThread)
    {
        printf("Could not create thread.\n");
        return false;
    }

    CloseHandle(RemoteThread);

    return true;
}

int main(int argc, char *argv[])
{
    if(argc != 3)
    {
        printf("Usage: injector [Process Name] [Full DLL Path]\n");
        return 0;
    }
    DWORD ProcessID = FindProcessID(argv[1]);

    if(!ProcessID)
    {
        printf("%s is either not running or could not be found.\n", argv[1]);
        return 0;
    }

    HANDLE TargetProcess = OpenProcess(PROCESS_ALL_ACCESS, 0, ProcessID);

    if(TargetProcess)
    {
        BOOL InjectStatus = InjectDLL(TargetProcess, argv[2]);
        if(InjectStatus)
        {
            printf("Injected properly.\n");
        }
        else
        {
            printf("Unable to inject.\n");
        }
    }

    CloseHandle(TargetProcess);
    return 1;
}