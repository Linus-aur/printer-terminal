#pragma once
#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <direct.h>

BOOL RawDataToPrinter(LPWSTR szPrinterName, LPBYTE lpData, DWORD dwCount);
BOOL printer(char* string);
BOOL printer(const char* string);
BOOL printer(const char* string, int size);