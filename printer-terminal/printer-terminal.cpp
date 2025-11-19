
#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <direct.h>
#include <threads.h>
#include "Printer_IO.h"

const int bufsize = 256;

#pragma warning (disable : 4996)
#pragma warning (disable : 6031)
#pragma warning (disable : 6001)
using namespace std;


typedef struct {
    HANDLE inputReadSide, outputWriteSide;
    HANDLE outputReadSide, inputWriteSide;
} TERMINAL_RWSIDES, * PTERMINAL_RWSIDES;


HRESULT SetUpPseudoConsole(COORD size, HPCON* hPC, PTERMINAL_RWSIDES trwsides)
{
    HRESULT hr = S_OK;

    // Create communication channels

    HANDLE inputReadSide, outputWriteSide;
    HANDLE outputReadSide, inputWriteSide;

    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    if (!CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0))
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    hr = CreatePseudoConsole(size, inputReadSide, outputWriteSide, 0, hPC);

    *trwsides = { inputReadSide, outputWriteSide, outputReadSide, inputWriteSide };

    return hr;

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
    if (!si.lpAttributeList)
    {
        return E_OUTOFMEMORY;
    }


    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired))
    {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Set the pseudoconsole information into the list
    if (!UpdateProcThreadAttribute(si.lpAttributeList,
        0,
        PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
        hpc,
        sizeof(hpc),
        NULL,
        NULL))
    {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;

    return S_OK;
}


HRESULT SetUpPseudoConsole(COORD size, STARTUPINFOEX siEx)
{
    PCWSTR childApplication = L"C:\\Windows\\System32\\cmd.exe";

    // Create mutable text string for CreateProcessW command line string.
    const size_t charsRequired = wcslen(childApplication) + 1;
    PWSTR cmdLineMutable = (PWSTR)HeapAlloc(GetProcessHeap(), 0, sizeof(wchar_t) * charsRequired);

    if (!cmdLineMutable)
    {
        return E_OUTOFMEMORY;
    }

    wcscpy_s(cmdLineMutable, charsRequired, childApplication);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    // Call CreateProcess
    if (!CreateProcessW(NULL,
        cmdLineMutable,
        NULL,
        NULL,
        FALSE,
        EXTENDED_STARTUPINFO_PRESENT,
        NULL,
        NULL,
        &siEx.StartupInfo,
        &pi))
    {
        HeapFree(GetProcessHeap(), 0, cmdLineMutable);
        return HRESULT_FROM_WIN32(GetLastError());
    }


    return S_OK;
}


int read_from_keyboard(char* line) {
    char control2[] = { 27, 45, 0, 0 };
    char ch_s[2] = { 0, 0 };
    int i = 0;


    for (i = 0; ; ++i) {
        ch_s[0] = getch();
        printf("%c\n", ch_s[0]);

        if (ch_s[0] == '\r') {
            line[i] = 0;
            printer("\n\r");
            break;
        }
        line[i] = ch_s[0];

        printer(ch_s);
        printer(control2, 3);
    }



    return 0;
}

int input_from_stdin_to_shell(void* param) {

    HANDLE write_dest = (HANDLE)param;
    BYTE buff[bufsize];
    DWORD read_result = 0;
    DWORD write_result = 0;

    HANDLE STDIN = GetStdHandle(STD_INPUT_HANDLE);

    for (;;) {
        read_from_keyboard((char*)buff);
        if (!WriteFile(write_dest, buff, read_result, &write_result, nullptr)) {
            perror("Failed to write to program.\n");
        }
    }

    return 0;
}



int output_to_printer(char* line, int bytes) {
    char control2[] = { 27, 45, 0, 0 };
    printer(line, bytes);
    printer(control2, 3);
    return 0;
}


int output_from_shell_to_stdout(void* param) {

    HANDLE read_source = (HANDLE)param;

    BYTE* buff = (BYTE*)calloc(bufsize, sizeof(BYTE));
    DWORD read_result = 0;
    DWORD write_result = 0;

    HANDLE STDOUT = GetStdHandle(STD_OUTPUT_HANDLE);

    for (;;) {
        ReadFile(read_source, buff, bufsize, &read_result, nullptr);
        buff[read_result] = 0;

        //for (int i = 0; i < read_result; ++i) {
        //    printf("%02X\t(%c)\n", buff[i], buff[i]);
        //}

        output_to_printer((char*)buff, read_result);
        memset(buff, 0, bufsize);
    }

    return 0;
}
void destructor(void* val) { return; }

int main(void) {

    HPCON hPc;
    STARTUPINFOEX si;
    TERMINAL_RWSIDES trwsides;
    SetUpPseudoConsole({ 80, 25 }, &hPc, &trwsides);
    PrepareStartupInformation(hPc, &si);
    SetUpPseudoConsole({ 80, 25 }, si);





    tss_t key;
    thrd_t threads[2];
    tss_create(&key, destructor);
    thrd_create(&threads[0], input_from_stdin_to_shell, trwsides.inputWriteSide);
    thrd_create(&threads[1], output_from_shell_to_stdout, trwsides.outputReadSide);

    for (int i = 0; i < 2; i++) {
        thrd_join(threads[i], NULL);
    }

    tss_delete(key);

    return 0;
}
