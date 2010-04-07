/* getpass - echoless input from stdin
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Copyright 2010, savrus
 * Read the COPYING file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "getpass.h"


#ifdef _WIN32

typedef unsigned long gp_save;

static void gp_echo_off(int _set, gp_save *gp)
{
    static HANDLE hIn = (HANDLE)-3;
    if (hIn == INVALID_HANDLE_VALUE || hIn == 0) {
        hIn = INVALID_HANDLE_VALUE;
        return;
    } else if ((HANDLE)-3 == hIn) {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
        gp_disable_echo(_set, gp);
        return;
    }
    if (_set) {
        if (GetConsoleMode(hIn, gp) == 0)
            hIn = INVALID_HANDLE_VALUE;
        else
            SetConsoleMode(hIn, *gp & ~ENABLE_ECHO_INPUT);
    } else
        SetConsoleMode(hIn, *gp);
}

#else /* _WIN32 */

typedef struct termios gp_save;

static void gp_echo_off(int _set, gp_save *gp)
{
    gp_save newgp;
    if (_set) {
        tcgetattr(fileno(stdin), gp);
        newgp = *gp;
        newgp.c_lflag &= ~ECHO;
        tcsetattr(fileno(stdin), TCSADRAIN, &newgp);
    } else
        tcsetattr(fileno(stdin), TCSADRAIN, gp);
}

#endif /* _WIN32 */

unsigned int gp_readline(char *buf, unsigned int size)
{
    gp_save gp;
    gp_echo_off(1, &gp);
    buf[size - 1] = 0;
    buf = fgets(buf, size, stdin);
    gp_echo_off(0, &gp);
    if (buf && (size = strlen(buf)) > 0 && buf[size - 1] == '\n') {
        buf[size - 1] = 0;
        return size - 1;
    } else
        return 0;
}

