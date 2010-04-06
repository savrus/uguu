/* getpass - echoless input from stdin
*
* Copyright 2009, 2010, savrus
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
void gp_set(int echo_on, gp_save *gp)
{
	static HANDLE hIn = (HANDLE)-3;
	if (hIn == INVALID_HANDLE_VALUE || hIn == 0) {
		hIn = INVALID_HANDLE_VALUE;
		return;
	} else if ((HANDLE)-3 == hIn)
		hIn = GetStdHandle(STD_INPUT_HANDLE);
	if (echo_on) {
		if (echo_on == 2)
			GetConsoleMode(hIn, gp);
		SetConsoleMode(hIn, *gp | ENABLE_ECHO_INPUT);
	} else {
		if (GetConsoleMode(hIn, gp) == 0)
			hIn = INVALID_HANDLE_VALUE;
		else
			SetConsoleMode(hIn, *gp & ~ENABLE_ECHO_INPUT);
	}
}
#else
void gp_set(int echo_on, gp_save *gp)
{
	if (!echo_on || echo_on == 2)
		tcgetattr(fileno(stdin), gp);
	if (echo_on)
		gp->c_lflag |= ECHO;
	else
		gp->c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSADRAIN, gp);
}
#endif

unsigned int gp_readline(char *buf, unsigned int size)
{
	gp_save gp;
	gp_set(0, &gp);
	buf[size-1] = 0;
	buf = fgets(buf, size, stdin);
	gp_set(1, &gp);
	if (buf && (size = strlen(buf)) > 0 && '\n' == buf[size-1]) {
		buf[size-1] = 0;
		return size-1;
	} else return 0;
}

