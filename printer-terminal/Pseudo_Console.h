#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <direct.h>

typedef struct {
    HANDLE inputReadSide, outputWriteSide;
    HANDLE outputReadSide, inputWriteSide;
} TERMINAL_RWSIDES, * PTERMINAL_RWSIDES;

HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEX* psi);
HRESULT SetUpPseudoConsole(COORD size, HPCON* hPC, PTERMINAL_RWSIDES trwsides,
    PCWSTR childApplication);