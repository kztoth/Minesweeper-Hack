#include <windows.h>
#include <TlHelp32.h>
#include <stdio.h>
#include <cstdint>

#define SINGLE_CLICK_OFFSET 0x27230
#define RIGHT_CLICK_OFFSET 0x32590
#define GAME_STATE_OFFSET 0xAAA38
#define MAIN_OFFSET 0xAAC18

typedef void(*SingleClick)(int64_t MineArrayPointer, int64_t Row, int64_t Column);
typedef void(*RightClick)(int64_t FlagArrayPointer, int64_t LocationPointer);

struct Coord
{
    int32_t Column;
    int32_t Row;
};

struct Location
{
    int8_t a[48];
    Coord Coordinates;
};

bool Running;
HHOOK KBDHook;
BYTE *ProcessAddress;

// Find these from the program
int32_t Width = 30;
int32_t Height = 16;

BOOL WINAPI StartHook();
DWORD FindProcessID(char *ProgramName);
BYTE *FindProcessAddress(char *ProgramName, DWORD ProcessID);
void FindMarkedSquares(int64_t *FlagArrayPointer, Coord *HelpCoordArray, int *HelpCount);
void FindMarkedBombs(int64_t *MineArrayPointer, Coord *HelpCoordArray, int *HelpCount, Coord *BombCoordArray, int *BombCount, Coord *ClearCoordArray, int *ClearCount);
void MarkBombs(int64_t *FlagArrayPointer, Coord *BombCoordArray, int *BombCount);
void ClickClear(int64_t *MineArrayPointer, Coord *ClearCoordArray, int *ClearCount);

LRESULT CALLBACK LLKeyboardProc(int NCode, 
                              WPARAM WParam,
                              LPARAM LParam)
{
    if(NCode == HC_ACTION && WParam == WM_KEYDOWN)
    {
        PKBDLLHOOKSTRUCT HookStruct = (PKBDLLHOOKSTRUCT)LParam;
        switch(HookStruct->vkCode)
        {
            case VK_ESCAPE:
                MessageBoxA(0, "Ending hook.", "DLL", MB_OK);
                Running = false;
                KBDHook = NULL;
                break;
            case 0x48: // H
                int64_t *FlagArray = (int64_t *)(ProcessAddress + MAIN_OFFSET);
                FlagArray = (int64_t *)(*FlagArray + 0x70);
                FlagArray = (int64_t *)(*FlagArray + 0x00);

                int64_t *MineArray = (int64_t *)(ProcessAddress + GAME_STATE_OFFSET);
                MineArray = (int64_t *)(*MineArray + 0x18); 

                Coord *HelpCoordArray = (Coord *)malloc(sizeof(Coord) * Width * Height);
                int *HelpCount = (int *)malloc(sizeof(int));
                *HelpCount = 0;

                // Find the squares that are marked
                FindMarkedSquares(FlagArray, HelpCoordArray, HelpCount);

                Coord *BombCoordArray = (Coord *)malloc(sizeof(Coord) * (*HelpCount));
                int *BombCount = (int *)malloc(sizeof(int));
                *BombCount = 0;
                Coord *ClearCoordArray = (Coord *)malloc(sizeof(Coord) * (*HelpCount));
                int *ClearCount = (int *)malloc(sizeof(int));
                *ClearCount = 0;
                // compare coordarray to bomb locations
                FindMarkedBombs(MineArray, HelpCoordArray, HelpCount, BombCoordArray, BombCount, ClearCoordArray, ClearCount);

                // set squares with bombs to flag
                MarkBombs(FlagArray, BombCoordArray, BombCount);

                // click other squares
                ClickClear(MineArray, ClearCoordArray, ClearCount);

                // clean up
                free(HelpCoordArray);
                free(HelpCount);
                free(BombCoordArray);
                free(BombCount);
                free(ClearCoordArray);
                free(ClearCount);

                break;
        }
    }
    return CallNextHookEx(KBDHook, NCode, WParam, LParam);
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL,
                      DWORD fdwReason,
                      LPVOID lpvReserved)
{
    switch(fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            MessageBoxA(0, "DLL attached.", "DLL", MB_ICONEXCLAMATION | MB_OK);
            CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StartHook, NULL, 0, NULL);
            break;
    }
    return true;
}

BOOL WINAPI StartHook()
{
    KBDHook = SetWindowsHookEx(WH_KEYBOARD_LL, LLKeyboardProc, NULL, 0);

    char Name[32] = "MineSweeper.exe";

    DWORD ProcessID = FindProcessID(Name);
    ProcessAddress = FindProcessAddress(Name, ProcessID);

    Running = true;
    MSG Message;
    while(Running)
    {
        PeekMessage(&Message, 0, 0, 0, PM_NOREMOVE);
    }
    return true;
}

// Finds the process 
DWORD FindProcessID(char *ProgramName)
{
    HANDLE ProcessSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if(INVALID_HANDLE_VALUE == ProcessSnapshot)
        return false;

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32); // Must be set or Process32First fails.

    if(!Process32First(ProcessSnapshot, &pe32))
    {
        CloseHandle(ProcessSnapshot);
        return false;
    }
    // Loop through all the process return if matching name.
    do
    {
        if(strcmp(ProgramName, pe32.szExeFile) == 0)
        {
            CloseHandle(ProcessSnapshot);
            return pe32.th32ProcessID;
        }
    }
    while(Process32Next(ProcessSnapshot, &pe32));

    CloseHandle(ProcessSnapshot);
    return false;
}

// Finds the starting address of the exe.
BYTE *FindProcessAddress(char *ProgramName, DWORD ProcessID)
{

    HANDLE ModuleSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, ProcessID);

    if(INVALID_HANDLE_VALUE == ModuleSnapshot)
        return 0;

    MODULEENTRY32 me32;
    me32.dwSize = sizeof(MODULEENTRY32); // Must be set or Module32First fails.

    if(!Module32First(ModuleSnapshot, &me32))
    {
        CloseHandle(ModuleSnapshot);
        return 0;
    }

    // Don't need to loop here because the exe is always the first module.
    CloseHandle(ModuleSnapshot);
    return me32.modBaseAddr;


    CloseHandle(ModuleSnapshot);
    return 0;
}


void FindMarkedSquares(int64_t *FlagArrayPointer, Coord *HelpCoordArray, int *HelpCount)
{
    FlagArrayPointer = (int64_t *)(*FlagArrayPointer + 0x38);
    FlagArrayPointer = (int64_t *)(*FlagArrayPointer + 0x50);
    FlagArrayPointer = (int64_t *)(*FlagArrayPointer + 0x10);
    int64_t *Current = FlagArrayPointer;
    *HelpCount = 0;

    for(int i = 0; i < Width; i++)
    {
        for(int j = 0; j < Height; j++)
        {
            Current = FlagArrayPointer;
            Current = (int64_t *)(*Current + (i*8));
            Current = (int64_t *)(*Current + 0x10);
            Current = (int64_t *)(*Current + (j*4));
            if((int32_t)*Current == (int32_t)0xb)
            {
                Coord temp;
                temp.Column = i;
                temp.Row = j;
                HelpCoordArray[(*HelpCount)++] = temp;
            }
        }
    }
}

void FindMarkedBombs(int64_t *MineArrayPointer, Coord *HelpCoordArray, int *HelpCount, 
                                                Coord *BombCoordArray, int *BombCount, 
                                                Coord *ClearCoordArray, int *ClearCount)
{
    MineArrayPointer = (int64_t *)(*MineArrayPointer + 0x58);
    MineArrayPointer = (int64_t *)(*MineArrayPointer + 0x10);
    int64_t *Current = MineArrayPointer;

    for(int i = 0; i < *HelpCount; i++)
    {
        int32_t Column = HelpCoordArray[i].Column;
        int8_t Row = HelpCoordArray[i].Row;
            Current = MineArrayPointer;
            Current = (int64_t *)(*Current + (Column*8));
            Current = (int64_t *)(*Current + 0x10);
            Current = (int64_t *)(*Current + Row);
            Coord temp;
            temp.Column = Column;
            temp.Row = Row;
            if((int8_t)*Current == (int8_t)0x1)
            {
                BombCoordArray[(*BombCount)++] = temp;
            }
            else
            {
                ClearCoordArray[(*ClearCount)++] = temp;   
            }
    }
}

void MarkBombs(int64_t *FlagArrayPointer, Coord *BombCoordArray, int *BombCount)
{

    RightClick Right_Click = NULL;
    Right_Click = (RightClick)(ProcessAddress + RIGHT_CLICK_OFFSET);
    for(int i = 0; i < *BombCount; i++)
    {
        Location tempLoc;
        tempLoc.Coordinates = BombCoordArray[i];
        Right_Click(*FlagArrayPointer, (int64_t)&tempLoc);
        Right_Click(*FlagArrayPointer, (int64_t)&tempLoc);
    }
}

void ClickClear(int64_t *MineArrayPointer, Coord *ClearCoordArray, int *ClearCount)
{
    SingleClick Single_Click = NULL;
    Single_Click = (SingleClick)(ProcessAddress + SINGLE_CLICK_OFFSET);
    for(int i = 0; i < *ClearCount; i++)
    {
        Single_Click(*MineArrayPointer, ClearCoordArray[i].Column, ClearCoordArray[i].Row);
    }
}

