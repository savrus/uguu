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
#define DEF_TIMEOUT 30

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
#define __DTP_BUF_SIZE 1024
#ifdef _WIN32
#include <Winsock.h>
#else
typedef int SOCKET;
typedef unsigned long DWORD;
#endif

#include <iconv.h>
#define ICONV_ERROR (iconv_t)(-1)
#ifdef _WIN32
#define iconv_cchar const char
#else//_WIN32
#define iconv_cchar char
#endif//_WIN32

#ifdef _MSC_VER
#pragma warning( disable : 4290 )
#endif
#define _throw_NE throw(NetworkError)
//Network related routines could trow CFtpControl::NetworkError
//in the case of networking error (timeout, disconnect etc).

typedef enum {NO_DTP = 0, DTP_ACTIVE, DTP_PASSIVE} DTP_TYPE;

class CFtpControl
{
public:
  unsigned int ServerIP;
  unsigned int ServerPORT;
  char *login;//NULL => anonymous
  char *pass;//NULL => some e-mail address if password is requested by server
  int timeout_sec;
  CFtpControl(void)
#ifdef _WIN32
    throw(void*)
#endif
    ;
  ~CFtpControl();
  class NetworkError {};
  class NetDataError : public NetworkError {};
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
  bool FindFirstFile(const char *DirOrMask, FtpFindInfo &FindInfo, DTP_TYPE useDTP = NO_DTP) _throw_NE;//find first file
  bool FindNextFile(FtpFindInfo &FindInfo);//find the next file, returns false if no more files found
  void FindClose(FtpFindInfo &FindInfo);//should be called after the successfull FindFirstFile call and all the subsequent FindNextFile calls

  //The following functions return the first response char or '\0' in the case error occurred
  char SendCmd(const char *cmd) _throw_NE;
  char SendCmdf(const char *cmd, ...) _throw_NE;
  char SendCmdResp(const char *cmd, CStrBuf &resp, int &r_len) _throw_NE;

  //The following functions are open data stream
  bool SendCmdConn(const char *cmd, bool Active) _throw_NE;
  bool SendCmdConnf(const char *cmd, bool Active, ...) _throw_NE;

  //data transfer support
  void AbortData(void) _throw_NE;//abort data transfer
  void SendData(const void *data, int dlen) _throw_NE;//send data to the opened data stream, returns amount of data sent
  int RecvData(void *buffer, int blen, bool waitAll = false) _throw_NE;//receive available data from the opened data stream (if waitAll, wait to fill buffer), returns 0 if connection closed
  char CloseData(bool readresp = true);//Final cleanup for data transfer, call manually after sending, don't specify param value

  //bool IsNetworkError(void);
  const char *GetLastResponse(void);
private:
  SOCKET sock, dsock;
protected:
  iconv_t _to_utf8;
private:
  iconv_t _from_utf8;
  char netbuff[__NB_SIZE];
  int nb_nextcmd;
  char skipresponse(void) _throw_NE;
  char readresponse(CStrBuf &resp, int &r_len) _throw_NE;
  void readAllData(CStrBuf &data, int &d_len) _throw_NE;
  void TryUtf8() _throw_NE;
  bool Converter(iconv_t direction, const char *src_start, char *dst, size_t dstlen);
  void sockwait(bool forread) _throw_NE;
  void dsockwait(bool forread) _throw_NE;
  void rawwrite(const char *str) _throw_NE;
  void rawwritef(const char *str, ...) _throw_NE;
  bool rawwriteConvParam(const char *str, const char *param, DTP_TYPE initDTP = NO_DTP) _throw_NE;
  int rawread(int nb_start) _throw_NE;
  bool RawConnCmd(const char *str, bool Active) _throw_NE;
  bool RawConnCmdf(const char *str, bool Active, ...) _throw_NE;
  int findendstr(int end);
};

#undef _throw_NE

#endif //__RAD_FTPSOCKLIB_H__
