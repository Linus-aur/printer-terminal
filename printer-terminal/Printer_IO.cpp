#include "Printer_IO.h"

extern LPWSTR printer_name;

#pragma warning (disable : 4996)
#pragma warning (disable : 6031)
using namespace std;

BOOL RawDataToPrinter(LPWSTR szPrinterName, LPBYTE lpData, DWORD dwCount)
{
    HANDLE     hPrinter;
    DOC_INFO_1 DocInfo;
    DWORD      dwJob;
    DWORD      dwBytesWritten;
    if (!OpenPrinter(szPrinterName, &hPrinter, NULL)) {
        cout << "openFail" << GetLastError() << endl;
        return false;
    }

    DocInfo.pDocName    = LPWSTR(L"PRINTER-TERMINAL\0");
    DocInfo.pOutputFile = NULL;
    DocInfo.pDatatype   = NULL;

    if ((dwJob = StartDocPrinter(hPrinter, 1, (LPBYTE)&DocInfo)) == 0) {
        int x = GetLastError();
        cout << "StartDocPrinter Fail" << x << endl;
        ClosePrinter(hPrinter);
        return false;
    }
    if (!StartPagePrinter(hPrinter)) {
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return false;
    }
    if (!WritePrinter(hPrinter, lpData, dwCount, &dwBytesWritten) || 
        !FlushPrinter(hPrinter, lpData, dwCount, &dwBytesWritten, 0)) {
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return false;
    }
    if (!EndPagePrinter(hPrinter)) {
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return false;
    }
    if (!EndDocPrinter(hPrinter)) {
        ClosePrinter(hPrinter);
        return false;
    }

    ClosePrinter(hPrinter);

    if (dwBytesWritten != dwCount)
        return false;
    return true;
}

BOOL printer(char* string) {
    return RawDataToPrinter(printer_name, (LPBYTE)string, strlen(string));
}
BOOL printer(const char* string) {
    return RawDataToPrinter(printer_name, (LPBYTE)string, strlen(string));
}
BOOL printer(const char* string, int size) {
    return RawDataToPrinter(printer_name, (LPBYTE)string, size);
}