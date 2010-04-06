/* getpass.h - definitions and interfaces of echoless input from stdin
*
* Copyright 2009, 2010, savrus
* Read the COPYING file in the root of the source tree.
*/

#ifndef _CONS_GETPASS_H_
#define _CONS_GETPASS_H_

#if defined(_WIN32)
#include <stddef.h>
#else
#include <termios.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* gp_readline: echoless read line
 * parameters: buffer, buffer size (max input string length + 2)
 * return value: line length or 0 if buffer too small or input error
 */
unsigned int gp_readline(char *buf, unsigned int size);


#if defined(_WIN32)
typedef unsigned long gp_save;
#else
typedef struct termios gp_save;
#endif

typedef enum {
	ECHO_RESTORE = -1,//restore echo mode saved in gp_save structure by ECHO_ON/ECHO_OFF call
	ECHO_OFF,
	ECHO_ON
} echo_mode;

/* enabling or disabling echo */
void gp_echo(echo_mode echo, gp_save *gp);

#ifdef __cplusplus
};
#endif


#endif//_CONS_GETPASS_H_
