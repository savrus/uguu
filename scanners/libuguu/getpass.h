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

/* enabling or disabling echo */
/* set echo_on to 2 if this is first gp_echo call with given gp_save */
void gp_echo(int echo_on, gp_save *gp);

#ifdef __cplusplus
};
#endif


#endif//_CONS_GETPASS_H_
