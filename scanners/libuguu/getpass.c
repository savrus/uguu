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

#define INHNO_ERROR (-2)

static void gp_set(gp_echo_mode echo, gp_save *gp)
{
    static HANDLE hIn = (HANDLE)-3;
    if (hIn == INVALID_HANDLE_VALUE || hIn == 0) {
        hIn = INVALID_HANDLE_VALUE;
        return;
    } else if ((HANDLE)-3 == hIn)
        hIn = GetStdHandle(STD_INPUT_HANDLE);
    switch (echo) {
    case GP_ECHO_RESTORE:
        SetConsoleMode(hIn, *gp);
        break;
    case GP_ECHO_ON:
    case GP_ECHO_OFF:
        if (GetConsoleMode(hIn, gp) == 0)
            hIn = INVALID_HANDLE_VALUE;
        else
            SetConsoleMode(hIn, echo
                ? *gp | ENABLE_ECHO_INPUT
                : *gp & ~ENABLE_ECHO_INPUT);
    }
}

#else /* _WIN32 */

static void gp_set(gp_echo_mode echo, gp_save *gp)
{
    gp_save newgp;
    switch (echo) {
        case GP_ECHO_RESTORE:
            tcsetattr(fileno(stdin), TCSADRAIN, gp);
        break;
        case GP_ECHO_ON:
        case GP_ECHO_OFF:
            tcgetattr(fileno(stdin), gp);
            newgp = *gp;
            if (echo == GP_ECHO_ON)
                newgp.c_lflag |= ECHO;
            else
                newgp.c_lflag &= ~ECHO;
            tcsetattr(fileno(stdin), TCSADRAIN, &newgp);
    }
}

#endif /* _WIN32 */

unsigned int gp_readline(char *buf, unsigned int size)
{
    gp_save gp;
    gp_set(GP_ECHO_OFF, &gp);
    buf[size - 1] = 0;
    buf = fgets(buf, size, stdin);
    gp_set(GP_ECHO_RESTORE, &gp);
    if (buf && (size = strlen(buf)) > 0 && buf[size - 1] == '\n') {
        buf[size - 1] = 0;
        return size - 1;
    } else
        return 0;
}

