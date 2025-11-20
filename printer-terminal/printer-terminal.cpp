#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <direct.h>
#include <threads.h>
#include "Printer_IO.h"
#define linesize 16384

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
    HANDLE inputReadSide,
        outputWriteSide,
        outputReadSide,
        inputWriteSide;
    // Create communication channels
    if (!CreatePipe(&inputReadSide, &inputWriteSide, NULL, 0) || \
        !CreatePipe(&outputReadSide, &outputWriteSide, NULL, 0)) {
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
    if (!si.lpAttributeList) {
        return E_OUTOFMEMORY;
    }


    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired)) {
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
        NULL)) {
        HeapFree(GetProcessHeap(), 0, si.lpAttributeList);
        return HRESULT_FROM_WIN32(GetLastError());
    }

    *psi = si;
    return S_OK;
}


HRESULT SetUpPseudoConsole(COORD size, STARTUPINFOEX siEx)
{
    PCWSTR childApplication = L"C:\\Windows\\System32\\bash.exe";

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
                        EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, 
                        &siEx.StartupInfo, &pi)) {
        HeapFree(GetProcessHeap(), 0, cmdLineMutable);
        return HRESULT_FROM_WIN32(GetLastError());
    }
    return S_OK;
}


int read_from_keyboard(char* line) {
    char control2[] = { 27, 45, 0, 0 };
    char ch_s[]     = { 0, 0 };
    int  i = 0;


    for (i = 0; ; ++i) {
        ch_s[0] = getch();
        printf("%c\n", ch_s[0]);

        if (ch_s[0] == '\r') {
            line[i] = 0x0D;
            line[i + 1] = 0x0A;
            printer("\n\r");
            break;
        }
        line[i] = ch_s[0];

        printer(ch_s);
        printer(control2, 3);
    }


    i += 2;
    return i;
}

int input_from_keyboard_to_shell(void* param) {

    HANDLE write_dest   = (HANDLE)param;
    DWORD  read_result  = 0;
    DWORD  write_result = 0;
    BYTE   buff[linesize];

    HANDLE STDIN = GetStdHandle(STD_INPUT_HANDLE);

    for (;;) {
        int res = read_from_keyboard((char*)buff);
        puts("End reading.");
        puts((char*)buff);

        if (!WriteFile(write_dest, buff, res, &write_result, nullptr)) {
            puts("Failed to write to program.\n");
        }
        puts("Written to program.");
    }

    return 0;
}



int output_to_printer(char* line, int bytes) {
    char control2[] = { 27, 45, 0, 0 };

    printer(line, bytes);
    printer(control2, 3);

    return 0;
}


int output_from_shell_to_printer(void* param) {
    HANDLE read_source    = (HANDLE)param;
    DWORD  read_result    = 0;
    BOOL   processing_VT  = false,
           text_hide      = false,
           set_title      = false;
    int    start          = 0, 
           end            = 0,
           control_type   = 0,
           read_index     = 0,
           write_index    = 0,
           code           = 0;
    BYTE* read_buff, * write_buff;

    const char   Emphasis[]      = { 27, 69 };
    const char   Emphasis_off[]  = { 27, 70 };
    const char   Underline[]     = { 27, 45, 1 };
    const char   Underline_off[] = { 27, 45, 0 };



    if ( !(read_buff = (BYTE*)calloc(2 * linesize, sizeof(BYTE))) || \
        !(write_buff = (BYTE*)calloc(2 * linesize, sizeof(BYTE))) ) {
        perror("Failed to alloc enough memory");
        return 1;
    }


    while (1) {
        ReadFile(read_source, read_buff, linesize, &read_result, nullptr);
        for (int i = 0; i < read_result; ++i) {
            printf("%02X\t(%c)\n", read_buff[i], read_buff[i]);
        }

        // Translate ANSI/VT Control characters into ESC/P control characters
        for (read_index = 0, write_index = 0; read_index < read_result; ) {
            switch (read_buff[read_index]) {
            case 27:                    // Starts a ANSI/VT CC 
                processing_VT = true;
                start = read_index++;
                continue;

            default:
                if (!processing_VT) {
                    if (text_hide)
                        write_buff[write_index] = ' ';
                    else if (set_title)
                        --write_index;
                    else
                        write_buff[write_index] = read_buff[read_index];

                    ++read_index, ++write_index;
                    continue;
                }
                else {
                    switch (read_buff[read_index]) {
                    case '[': case '0': case '1': case '2':
                    case '3': case '4': case '5': case '6':
                    case '7': case '8': case '9': case '?':
                    case ']':
                        read_index++;
                        break;


                    // Ignore following Control Characters,
                    // because they could not be implemented
                    // or print on a dot-matrix printer:
                    case 'K': case 's': case 'u':  // <ESC>[K/s/u
                    case 'h': case 'l':            // <ESC>[?***h/l
                    case 'A': case 'D':            // <ESC>[nA/D
                    case 'J':                      // <ESC>[2J
                    case 'H': 
                        processing_VT = false;
                        start = end = 0;
                        read_index++;
                        break;
                    case ';':
                        set_title = true;
                        processing_VT = false;
                        start = end = 0;
                        read_index++;
                        break;
                    case 'C':
                        code = 0;
                        for (int i = start; i <= read_index; ++i) {
                            if (isdigit(read_buff[i])) {
                                code = code * 10 + (read_buff[i] - 48);
                            }
                        }
                        for (int i = 0; i < code; ++i, ++write_index) {
                            write_buff[write_index] = ' ';
                        }
                        processing_VT = false;
                        start = end = 0;
                        read_index++;
                        break;

                    case 'm':
                        code = 0;
                        for (int i = start; i <= read_index; ++i) {
                            if (isdigit(read_buff[i])) {
                                code = code * 10 + (read_buff[i] - 48);
                            }
                        }
                        if (code == 0) {
                            for (int i = 0; i < 2; ++i, ++write_index) {
                                write_buff[write_index] = Emphasis_off[i];
                            }
                            for (int i = 0; i < 3; ++i, ++write_index) {
                                write_buff[write_index] = Underline_off[i];
                            }
                            text_hide = false;
                        }
                        else if (code == 1) {
                            for (int i = 0; i < 2; ++i, ++write_index) {
                                write_buff[write_index] = Emphasis[i];
                            }
                        }
                        else if (code == 4) {
                            for (int i = 0; i < 3; ++i, ++write_index) {
                                write_buff[write_index] = Underline[i];
                            }
                        }
                        else if (code == 8) {
                            text_hide = true;
                        }
                        processing_VT = false;
                        start = end = 0;
                        read_index++;
                        break;
                    }
                }

            
            }
        }
        set_title = false;
        puts("Now outputs to write_buff");
        for (int i = 0; i < write_index; ++i) {
            printf("%02X\t(%c)\n", write_buff[i], write_buff[i]);
        }
        output_to_printer((char*)write_buff, write_index - 1);
        memset(write_buff, 0, 2 * linesize);
        memset(read_buff, 0, 2 * linesize);
    }

    free(read_buff); free(write_buff);
    return 0;
}
void destructor(void* val) { return; }

int main(void) {

    chdir("C:\\Users\\LinusAUR");

    HPCON            hPc;
    STARTUPINFOEX    si;
    TERMINAL_RWSIDES trwsides;
    tss_t            key;
    thrd_t           threads[2];


    SetUpPseudoConsole({ 80, 25 }, &hPc, &trwsides);
    PrepareStartupInformation(hPc, &si);
    SetUpPseudoConsole({ 80, 25 }, si);


    tss_create(&key, destructor);
    thrd_create(&threads[0], input_from_keyboard_to_shell, trwsides.inputWriteSide);
    thrd_create(&threads[1], output_from_shell_to_printer, trwsides.outputReadSide);

    for (int i = 0; i < 2; i++) {
        thrd_join(threads[i], NULL);
    }

    tss_delete(key);

    return 0;
}
