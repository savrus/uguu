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
typedef unsigned long gp_save;
#else
#include <termios.h>
#include <unistd.h>
typedef struct termios gp_save;
#endif

#include "getpass.h"
#include "log.h"

#ifdef _WIN32

static HANDLE gp_console_handle(gp_save *gp)
{
    //static variable is required to avoid multiple GetStdHandle calls
    //it's not clear from msdn whether we should to close handle
    static HANDLE hIn = (HANDLE)-3;
    if (hIn == INVALID_HANDLE_VALUE)
        return INVALID_HANDLE_VALUE;
    else if (hIn == (HANDLE)-3) {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn == INVALID_HANDLE_VALUE || hIn == 0)
            return hIn = INVALID_HANDLE_VALUE;
    }
    if (gp != NULL && GetConsoleMode(hIn, gp) == 0)
        hIn = INVALID_HANDLE_VALUE;
    return hIn;
}

static void gp_echo_off(gp_save *gp)
{
    HANDLE h;
    LOG_ASSERT(gp != NULL, "Bad arguments\n");
    if ((h = gp_console_handle(gp)) != INVALID_HANDLE_VALUE)
        SetConsoleMode(h, *gp & ~ENABLE_ECHO_INPUT);
}

static void gp_echo_restore(gp_save *gp)
{
    HANDLE h;
    LOG_ASSERT(gp != NULL, "Bad arguments\n");
    if ((h = gp_console_handle(NULL)) != INVALID_HANDLE_VALUE)
        SetConsoleMode(h, *gp);
}

#else /* _WIN32 */

static void gp_echo_off(gp_save *gp)
{
    gp_save newgp;
    LOG_ASSERT(gp != NULL, "Bad arguments\n");
    tcgetattr(fileno(stdin), gp);
    newgp = *gp;
    newgp.c_lflag &= ~ECHO;
    tcsetattr(fileno(stdin), TCSADRAIN, &newgp);
}

static void gp_echo_restore(gp_save *gp)
{
    LOG_ASSERT(gp != NULL, "Bad arguments\n");
    tcsetattr(fileno(stdin), TCSADRAIN, gp);
}

#endif /* _WIN32 */

/* returns 0 if failed, size of string with '0' character counted otherwise */
unsigned int gp_readline(char *buf, unsigned int size)
{
    gp_save gp;
    LOG_ASSERT(buf != NULL && size > 2, "Bad arguments\n");
    gp_echo_off(&gp);
    buf[size - 1] = 0;
    buf = fgets(buf, size, stdin);
    gp_echo_restore(&gp);
    if (buf && (size = strlen(buf)) > 0 && buf[size - 1] == '\n') {
        buf[size - 1] = 0;
        return size;
    } else
        return 0;
}

