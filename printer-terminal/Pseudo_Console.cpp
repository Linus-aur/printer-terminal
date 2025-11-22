#include "Pseudo_Console.h"
HRESULT SetUpPseudoConsole(COORD size, HPCON* hPC, PTERMINAL_RWSIDES trwsides, 
    PCWSTR childApplication)
{
    HRESULT hr = S_OK;
    HANDLE  inputReadSide, outputWriteSide,
            outputReadSide, inputWriteSide;

    // Create communication channels
    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0) || \
        !CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
        return HRESULT_FROM_WIN32(GetLastError());
    }
    hr = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, hPC);
    *trwsides = { inputReadSide, outputWriteSide, outputReadSide, inputWriteSide };

    STARTUPINFOEX siEx;
    PrepareStartupInformation(*hPC, &siEx);

    // Create mutable text string for CreateProcessW command line string.
    const size_t charsRequired = wcslen(childApplication) + 1;
    PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

    if (!cmdLineMutable) {
        return E_OUTOFMEMORY;
    }
    wcscpy_s(cmdLineMutable, charsRequired, childApplication);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Create Process
    if (!CreateProcessW(NULL, cmdLineMutable, NULL, NULL, FALSE,
        EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &siEx.StartupInfo, &pi)) {
        HeapFree(GetProcessHeap(), 0, cmdLineMutable);
        return HRESULT_FROM_WIN32(GetLastError());
    }
    return S_OK;
}


HRESULT PrepareStartupInformation(HPCON hpc, STARTUPINFOEX* psi)
{
    // Prepare Startup Information structure
    STARTUPINFOEX si;
    ZeroMemory(&si, sizeof(si));
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    // Discover the size required for the list
    size_t bytesRequired = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);

    // Allocate memory to represent the list
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(GetProcessHeap(), 0, bytesRequired);
    if (!si.lpAttributeList) {
        return E_OUTOFMEMORY;
    }

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hpc,  sizeof(hpc), NULL, NULL)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;
    return S_OK;
}