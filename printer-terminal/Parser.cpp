#include "Parser.h"

BOOL processing_VT = false;
BYTE unprocessed_VT[32];
int  unprocessed_VTi = 0;

int parser(BYTE* read_buff, BYTE* write_buff, DWORD read_result, int* write_size) {
    BOOL   text_hide      = false,
           set_title      = false;
    int    start          = 0, 
           end            = 0,
           control_type   = 0,
           read_index     = 0,
           write_index    = 0,
           code           = 0;
    BYTE*  buff           = nullptr;

    const char Emphasis[]      = { 27, 69 };
    const char Emphasis_off[]  = { 27, 70 };
    const char Underline[]     = { 27, 45, 1 };
    const char Underline_off[] = { 27, 45, 0 };


    if (processing_VT) {
        buff = (BYTE*)calloc(read_result + unprocessed_VTi, sizeof(BYTE));
        memcpy(buff, unprocessed_VT, unprocessed_VTi);
        memcpy(buff + unprocessed_VTi, read_buff, read_result);
        read_buff = buff;
    }


    for (read_index = 0, write_index = 0; read_index < read_result; ) {
        switch (read_buff[read_index]) {
        case 27:                    // <ESC> Starts a ANSI/VT CC 
            processing_VT = true;
            set_title = false;
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
                case 'A': case 'D':            // <ESC>[nA/D
                case 'J':                      // <ESC>[2J
                case 'H':
                case 'h': case 'l':            // <ESC>[?***h/l
                    processing_VT = false;
                    start = end = 0;
                    read_index++;
                    break;

                // When we met a semicolon, There are 2 possibilities:
                // <ESC>*; or <ESC>y;xH
                case ';':                       // <ESC>]0; Un-standard set title control code
                    if (!isdigit(read_buff[read_index + 1]) && read_buff[read_index + 1] != 'H') {
                        set_title = true;
                        processing_VT = false;
                        start = end = 0;
                    }
                    read_index++;               // <ESC>y;xH we will ingnore it
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
                    }                   // And because other text effect are unimplemtable,
                                        // So simply ignore them.
                    processing_VT = false;
                    start = end = 0;
                    read_index++;
                    break;
                }
            }
            if (!processing_VT) {
                memset(unprocessed_VT, 0, 32);
                unprocessed_VTi = 0;
            }
            else {
                unprocessed_VT[unprocessed_VTi] = read_buff[read_index - 1];
                unprocessed_VTi += 1;
            }
        }
    }
    if (buff != nullptr) free(buff);

    *write_size = write_index;
    return processing_VT;
}