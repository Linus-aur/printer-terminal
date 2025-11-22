#include <direct.h>
#include <threads.h>
#include "Parser.h"
#include "Printer_IO.h"
#include "Pseudo_Console.h"
#define linesize 16384
#define VERSTR   "0.0.a-test"

#pragma warning (disable : 4996)
#pragma warning (disable : 6031)
#pragma warning (disable : 6001)
using namespace std;

LPWSTR printer_name;

int read_from_keyboard(char* line) {
    char refresh[]  = { 27, 45, 0 };
    char ch_s[]     = { 0, 0 };
    int  read_count = 0;

    for (;;++read_count) {
        ch_s[0] = getch();

        if (ch_s[0] == '\r') {
            line[read_count] = 0x0D;
            printer("\n\r");
            break;
        }
        line[read_count] = ch_s[0];

        printer(ch_s);
        printer(refresh, 3);
    }
    read_count += 1;
    return read_count;
}

int input_from_keyboard_to_shell(void* param) {
    HANDLE write_dest   = (HANDLE)param;
    DWORD  read_result  = 0;
    DWORD  write_result = 0;
    BYTE*  buff;

    if (!(buff = (BYTE*)calloc(linesize, sizeof(BYTE)))) {
        perror("Failed to alloc enough memory\n");
        return 1;
    }

    for (;;) {
        read_result = read_from_keyboard((char*)buff);
        if (!WriteFile(write_dest, buff, read_result, &write_result, nullptr)) {
            puts("Failed to write to program.\n");
        }
#ifdef _DEBUG
        puts("Written to program.");
#endif
    }

    free(buff);
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
    int    write_size     = 0;
    BYTE  *read_buff, 
          *write_buff;

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

#ifdef _DEBUG
        puts("Output from original program");
        for (int i = 0; i < read_result; ++i) {
            printf("%02X\t(%c)\n", read_buff[i], read_buff[i]);
        }
#endif

        // Translate ANSI/VT Control characters into ESC/P control characters
        while (parser(read_buff, write_buff, read_result, &write_size)) {
            ReadFile(read_source, read_buff, linesize, &read_result, nullptr);
        }

#ifdef _DEBUG
        puts("Now outputs to write_buff");
        for (int i = 0; i < write_size; ++i) {
            printf("%02X\t(%c)\n", write_buff[i], write_buff[i]);
        }
#endif
        output_to_printer((char*)write_buff, write_size);
        memset(write_buff, 0, 2 * linesize);
        memset(read_buff , 0, 2 * linesize);
    }

    free(read_buff); free(write_buff);
    return 0;
}
void destructor(void* val) {}


void GetInitInfo(char* directory, PWSTR shell) {

    printf("Terminal-printer version %s\n\n", VERSTR);
    strcpy(directory, "C:\\");
    wcscpy((wchar_t*)shell, L"C:\\Windows\\System32\\bash.exe");

    printer_name = (LPWSTR)calloc(32, sizeof(wchar_t));
    wcscpy((wchar_t*)printer_name, L"IBM-5152");

    do {
        if (chdir(directory) == -1)
            printf("Can't find that directory %s. Please specify another\n", directory);

        printf("Which directory do you want to started with? (<Enter> for C:\\) ");
        fgets(directory, 1023, stdin); directory[strlen(directory) - 1] = 0;
        if (strcmp("", directory) == 0) strcpy(directory, "C:\\");
    } while (chdir(directory));

    FILE* F = nullptr;
    do {
        if (!(F = _wfopen(shell, L"r")))
            puts("Can't find that Executable. Please specify another");

        printf("Which program do you want to run? (<Enter> for C:\\Windows\\System32\\bash.exe) ");
        fgetws(shell, 1023, stdin); shell[wcslen(shell) - 1] = 0;
        if (wcscmp(shell, L"") == 0) wcscpy(shell, L"C:\\Windows\\System32\\bash.exe");
    } while (!F);
    fclose(F);

    do {
        if (!RawDataToPrinter(printer_name, (BYTE*)"", 0)) {
            perror("Error occured. Please re-input the printer device name.\n");
        }

        printf("Enter the printer you want to use (<Enter> for IBM-5152) ");
        fgetws(printer_name, 30, stdin); printer_name[wcslen(printer_name) - 1] = 0;
        if (wcscmp(printer_name, L"") == 0) wcscpy(printer_name, L"IBM-5152");
    } while (!RawDataToPrinter(printer_name, (BYTE*)"", 0));


    puts("\nNow we are ready to start. Turn your printer on and online,");
    puts("and then press <Enter>, then see what will happen!");
    getchar();
    return;
}



int main(void) {
    HPCON            hPc;
    TERMINAL_RWSIDES trwsides;
    tss_t            key;
    thrd_t           threads[2];
    wchar_t          shell[1024];
    char             directory[1024];

    GetInitInfo(directory, shell);
    SetUpPseudoConsole({ 80, 25 }, &hPc, &trwsides, shell);

    printer("\r");

    tss_create(&key, destructor);
    thrd_create(&threads[0], input_from_keyboard_to_shell, trwsides.inputWriteSide);
    thrd_create(&threads[1], output_from_shell_to_printer, trwsides.outputReadSide);

    for (int i = 0; i < 2; i++) {
        thrd_join(threads[i], NULL);
    }

    tss_delete(key);

    return 0;
}
