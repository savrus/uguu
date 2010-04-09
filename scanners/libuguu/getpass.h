/* getpass.h - definitions and interfaces of echoless input from stdin
 *
 * Copyright 2010, Radist <radist.nt@gmail.com>
 * Read the COPYING file in the root of the source tree.
 */

#ifndef _GETPASS_H_
#define _GETPASS_H_

#ifdef __cplusplus
extern "C" {
#endif

/* gp_readline: echoless read line
 * parameters: buffer, buffer size (max input string length + 2)
 * return value: line length or 0 if buffer too small or input error
 */
unsigned int gp_readline(char *buf, unsigned int size);

#ifdef __cplusplus
};
#endif


#endif /* _GETPASS_H_ */

