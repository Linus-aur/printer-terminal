#include "Printer_IO.h"
#define PRINTERNAME (LPWSTR)L"IBM-5152"


#pragma warning (disable : 4996)
#pragma warning (disable : 6031)
using namespace std;



BOOL RawDataToPrinter(LPWSTR szPrinterName, LPBYTE lpData, DWORD dwCount)
{
    HANDLE     hPrinter;
    DOC_INFO_1 DocInfo;
    DWORD      dwJob;
    DWORD      dwBytesWritten;

    DWORD f = 0;


    if (!OpenPrinter(szPrinterName, &hPrinter, NULL)) {
        int y = GetLastError();
        cout << "openFail" << y << endl;
        return FALSE;
    }


    DocInfo.pDocName = LPWSTR(L"DOC1\0");
    DocInfo.pOutputFile = NULL;
    DocInfo.pDatatype = NULL;

    if ((dwJob = StartDocPrinter(hPrinter, 1, (LPBYTE)&DocInfo)) == 0)
    {
        int x = GetLastError();
        cout << "StartDocPrinter Fail" << x << endl;
        ClosePrinter(hPrinter);
        return FALSE;
    }

    if (!StartPagePrinter(hPrinter))
    {
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return FALSE;
    }

    if (!WritePrinter(hPrinter, lpData, dwCount, &dwBytesWritten))
    {
        EndPagePrinter(hPrinter);
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return FALSE;
    }

    int r = FlushPrinter(hPrinter, lpData, dwCount, &f, 0);

    if (!EndPagePrinter(hPrinter))
    {
        EndDocPrinter(hPrinter);
        ClosePrinter(hPrinter);
        return FALSE;
    }

    if (!EndDocPrinter(hPrinter))
    {
        ClosePrinter(hPrinter);
        return FALSE;
    }

    ClosePrinter(hPrinter);

    if (dwBytesWritten != dwCount)
        return FALSE;
    return TRUE;
}

BOOL printer(char* string) {
    return RawDataToPrinter(PRINTERNAME, (LPBYTE)string, strlen(string));
}
BOOL printer(const char* string) {
    return RawDataToPrinter(PRINTERNAME, (LPBYTE)string, strlen(string));
}
BOOL printer(const char* string, int size) {
    return RawDataToPrinter(PRINTERNAME, (LPBYTE)string, size);
}
