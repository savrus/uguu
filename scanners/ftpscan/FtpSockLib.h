/*
 *  FTP Socket Library
 *
 *  Implements functions working with FTP Control Connection.
 *  Originally designed as the part of the Alex3D's SNT project.
 *
 * Copyright (c) 2006-2010, Radist <radist.nt@gmail.com>
 * All rights reserved.
 *
 * Read the COPYING file in the root of the source tree.
 */

#ifndef __RAD_FTPSOCKLIB_H__
#define __RAD_FTPSOCKLIB_H__

#define FTP_PORT 21
#define VSPRINTF_BUFFER_SIZE 2048
#define DEF_TIMEOUTS 30000

//for find routines
#include <sys/types.h>
#include <time.h>
extern "C" {
#include "ftpparse.h"
};
typedef struct {
  struct ftpparse Data;//name and id fields are pointed to 0-terminated strings, namelen can be invalid!
  char *FindPtr;
  char *FindBuff;
  char *ConvertBuff;
} FtpFindInfo;

#include <stdarg.h>
class CStrBuf
{
public:
  CStrBuf(size_t _size);//_size should consider terminating '\0'
  CStrBuf();
  ~CStrBuf();
  void setsize(size_t _size);
  char *release(void);
  char *buf(void) {return m_buf;};
  size_t size(void) {return m_size;};
private:
  size_t m_size;
  char *m_buf;
};

#define __NB_SIZE 1024
#ifdef _WIN32
#include <Winsock.h>
#else
typedef int SOCKET;
typedef unsigned long DWORD;
#endif

#include <iconv.h>
#define ICONV_ERROR (iconv_t)(-1)

#ifdef _MSC_VER
#pragma warning( disable : 4290 )
#endif
#define _throw_NE throw(NetworkError)
//Network related routines could trow CFtpControl::NetworkError
//in the case of networking error (timeout, disconnect etc).

class CFtpControl
{
public:
  unsigned int ServerIP;
  unsigned int ServerPORT;
  char *login;//NULL => anonymous
  char *pass;//NULL => some e-mail address if password is requested by server
  DWORD timeouts;//milliseconds !!!
  CFtpControl(void)
#ifdef _WIN32
    throw(void*)
#endif
    ;
  ~CFtpControl();
  class NetworkError {};
  bool tryconn(void);//try to establish ftp control connection
  bool Logon(void) _throw_NE;//login to server, do basic setup
  void Quit(void);//say bye to server and disconnect

  void Noop(void) _throw_NE;//send noop command
  bool ChDir(const char *dir) _throw_NE;//change directory
  //codepage support
  static const char *DefaultAnsiCP;//the default non-UTF-8 codepage, must be set before the first Logon call
  bool SetConnCP(const char *cpId);//set connection CP if it's not UTF-8 or DefaultAnsiCP, use before the first Logon call, NULL - default behaviour
  //find functions, cann't find only directories or only files
  //returns filenames converted to UTF-8
  bool FindFirstFile(const char *DirOrMask, FtpFindInfo &FindInfo) _throw_NE;//find first file
  bool FindNextFile(FtpFindInfo &FindInfo);//find the next file, returns false if no more files found
  void FindClose(FtpFindInfo &FindInfo);//should be called after the successfull FindFirstFile call and all the subsequent FindNextFile calls

  //The following functions return the first response char or '\0' in the case error occurred
  char SendCmd(const char *cmd) _throw_NE;
  char SendCmdf(const char *cmd, ...) _throw_NE;
  char SendCmdResp(const char *cmd, CStrBuf &resp, int &r_len) _throw_NE;

  //bool IsNetworkError(void);
  const char *GetLastResponse(void);
private:
  SOCKET sock;
  int timeout_sec;
  iconv_t _to_utf8;
  iconv_t _from_utf8;
  char netbuff[__NB_SIZE];
  char skipresponse(void) _throw_NE;
  char readresponse(CStrBuf &resp, int &r_len) _throw_NE;
  void TryUtf8() _throw_NE;
  bool Converter(iconv_t direction, const char *src_start, char *dst, size_t dstlen);
  void sockwait(bool forread) _throw_NE;
  void rawwrite(const char *str) _throw_NE;
  void rawwritef(const char *str, ...) _throw_NE;
  bool rawwriteConvParam(const char *str, const char *param) _throw_NE;
  int rawread(int nb_start) _throw_NE;
  int findendstr(int end);
};

#undef _throw_NE

#endif //__RAD_FTPSOCKLIB_H__
