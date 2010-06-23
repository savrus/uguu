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
 *
 *  Thanks to:
 * D. J. Bernstein for ftpparse package used in this library
 * Alepar & Alex3D for various support
 * Quinn for rewriting the license agreement
 *
 *  Change log:
 * 09-09-2006 I've started writing the library ;)
 * 13-12-2006 Most needed routines are working fine :smirk:
 * 08-08-2008 Changed license to BSD, reviewed code, dropped support of the custom converters, reviewed code
 * 11-08-2008 Fixed error in skipresponse which might cause missing recognition the end response line
 * 02-01-2010 Started cross-platform adaptation
 * 06-01-2010 Added iconv support
 * 25-02-2010 Added DTP support
 * 27-02-2010 Fixed skipping the second response
 *
 *  Disclaimer for thouse who wants to criticize code:
 * This library was originally created when I had nearly no experiense in C++ and networking programming areas.
 * Of course, later I've reviewed this code, but complete rewriting of it requires too mach time.
 *
 */

#include <sys/types.h>

#ifdef _WIN32

#include <Winsock2.h>
#include <Windows.h>
#define usleep(x) Sleep((x)/1000)

#define in_addr_byte(in,N) ((in).S_un.S_un_b.s_b##N)
typedef int socklen_t;

#else // Linux etc

#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (-1)
#endif
#ifndef SOCKET_ERROR
#define SOCKET_ERROR (-1)
#endif

struct in_addr_bytes { unsigned char b1,b2,b3,b4; };
#define in_addr_byte(in,N) (reinterpret_cast<struct in_addr_bytes*>(&(in))->b##N)

static void closesocket(int sock)
{
  shutdown(sock, SHUT_RDWR);
  close(sock);
}

#endif //_WIN32

struct in_port_bytes { unsigned char b1,b2; };
#define in_port_byte(p,N) (reinterpret_cast<struct in_port_bytes*>(&(p))->b##N)

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>

#include "FtpSockLib.h"
#include "log.h"
#ifndef _MSC_VER
#include "vs_s.h"
#endif

//class CStrBuf

#define RoundSize(x) (((x)+8191)&~8191)

CStrBuf::CStrBuf(size_t _size)//_size should consider terminating '\0'
: m_size(RoundSize(_size))
{
  m_buf = (char *)malloc( m_size );
}

CStrBuf::CStrBuf(): m_size(0), m_buf(NULL)
{
}

CStrBuf::~CStrBuf()
{
  if (m_buf)
    free(m_buf);
}

void CStrBuf::setsize(size_t _size)
{
  if( _size <= m_size )
    return;
#ifdef _MSC_VER
#pragma warning(suppress:6308)
#endif
  m_buf = (char *)realloc( m_buf, m_size = RoundSize(_size) );
}

char *CStrBuf::release(void)
{
  char *res = m_buf;
  m_size = 0;
  m_buf = NULL;
  return res;
}

#undef RoundSize

//network functions
static bool safe_connect(SOCKET sock, struct sockaddr_in *sa, int _timeout)
{
  if( connect(sock, (struct sockaddr *)sa, sizeof(struct sockaddr_in)) == SOCKET_ERROR ) {
    LOG_ERR("Connect error\n");
    return false;
  }

#if 0
  timeval timeout;
  timeout.tv_sec = _timeout;
  timeout.tv_usec = 0;
  fd_set fdsw, fdse;
  FD_ZERO(&fdsw);
  FD_ZERO(&fdse);
  FD_SET(sock, &fdsw);
  FD_SET(sock, &fdse);
  int res = select(sock + 1, NULL, &fdsw, &fdse, &timeout);
  if( res<=0 || FD_ISSET(sock, &fdse) ) {
    LOG_ERR("Connect failed or timed out\n");
    return false;
  }
#endif

  socklen_t len = 4;
  int res = 0;
  if( getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&res, &len) != SOCKET_ERROR && !res )
    return true;
  LOG_ERR("Connection is broken\n");
  return false;
}

//class CFtpControl

const char *CFtpControl::DefaultAnsiCP = NULL;

CFtpControl::CFtpControl(): ServerIP(0), ServerPORT(FTP_PORT),
        login(NULL), pass(NULL), timeout_sec(DEF_TIMEOUT),
        sock(INVALID_SOCKET), dsock(INVALID_SOCKET),
        _to_utf8(ICONV_ERROR), _from_utf8(ICONV_ERROR)
{
#ifdef _WIN32
  WSADATA Data;
  if( WSAStartup(MAKEWORD(2,0), &Data) != 0 ){
    LOG_ERR("WSAStartup failed\n");
    throw NULL;
  }
  if( LOBYTE(Data.wVersion) != 2 ){
    WSACleanup();
    LOG_ERR("Invalid Winsock version(%d)\n", Data.wVersion);
    throw NULL;
  }
#endif//_WIN32
#ifdef _DEBUG
  memset(&netbuff, 0, sizeof(netbuff));
#else // _DEBUG
  *netbuff = '\0';
#endif // _DEBUG
}

CFtpControl::~CFtpControl(void)
{
  Quit();
#ifdef _WIN32
  WSACleanup();
#endif
  SetConnCP(NULL);
}

bool CFtpControl::tryconn(void)
{
  LOG_ASSERT(INVALID_SOCKET == sock, "Still connected\n");

  struct sockaddr_in sa;

  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  LOG_ASSERT(INVALID_SOCKET != sock, "Cann't create socket\n");

  do{
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(ServerPORT);
    sa.sin_addr.s_addr = ServerIP;
    //memset(&sa.sin_zero, 0, sizeof(sa.sin_zero));
    if( !safe_connect(sock, &sa, timeout_sec) )
      break;

    nb_nextcmd = 0;
    try {
      if( skipresponse() != '2' )
        break;
    } catch(const NetworkError) {
      break;
    }

    return true;
  }while(0);

  closesocket(sock);
  sock = INVALID_SOCKET;
  return false;
}

bool CFtpControl::Logon(void)
{
  int res;
  rawwritef("USER %s\r\n", login?login:"anonymous");
  res = skipresponse();
  if( res < '2' || res > '3' )
    return false;
  if( res == '2' )
    return true;
  rawwritef("PASS %s\r\n", pass?pass:"R@Cli.Lib");
  res = skipresponse();
  TryUtf8();
  return res == '2';
}

void CFtpControl::Quit(void)
{
  if( INVALID_SOCKET == sock ) return;
  CloseData(false);
  try {
  	rawwrite("QUIT\r\n");
  } catch(const NetworkError) {};
  closesocket(sock);
  sock = INVALID_SOCKET;
}

void CFtpControl::Noop(void)
{
  rawwrite("NOOP\r\n");
  skipresponse();
}

bool CFtpControl::ChDir(const char *dir)
{
  LOG_ASSERT(dir, "Bad arguments\n");
  return rawwriteConvParam("CWD %s\r\n", dir) && skipresponse() == '2';
}

#define DROP_ICONV(ic) \
  if( ICONV_ERROR != ic ) { \
    iconv_close(ic); \
    ic = ICONV_ERROR; \
  }
#define INIT_ICONV(ic,from,to,onerror) \
  if( ICONV_ERROR == (ic = iconv_open(to, from)) ) \
    onerror; \
  else \
    iconv(ic, NULL, NULL, NULL, NULL)

bool CFtpControl::SetConnCP(const char *cpId)
{
  DROP_ICONV(_from_utf8);
  DROP_ICONV(_to_utf8);
  if( !cpId )
    return true;
  const char utf8[] = "UTF-8";
  INIT_ICONV(_from_utf8, utf8, cpId, return false);
  INIT_ICONV(_to_utf8, cpId, utf8, return false);
  return true;
}

#undef DROP_ICONV
#undef INIT_ICONV

void CFtpControl::TryUtf8()
{
  if( ICONV_ERROR != _from_utf8 || ICONV_ERROR != _to_utf8 )
    return;
  CStrBuf feat;
  int n;
  rawwrite("FEAT\r\n");
  do {
    if( readresponse(feat, n) != '2' )
      break;
    _strupr_s(feat.buf(), feat.size());
    if( strstr(feat.buf(), "UTF8") == NULL )
      break;
    if( strstr(feat.buf(), "CLNT") != NULL ){
      rawwrite("CLNT CFtpControl\r\n");
      skipresponse();
    }
    //OPTS UTF8 command was removed from RFC 2640.
    //However, there are some servers which requires this command.
    rawwrite("OPTS UTF8 ON\r\n");
    skipresponse(); /*!= '2' */
    return;
  } while(0);
  LOG_ASSERT(SetConnCP(DefaultAnsiCP), "CFtpControl::DefaultAnsiCP (%s) should be valid iconv codepage name (see iconv -l)\n", DefaultAnsiCP);
}

bool CFtpControl::FindFirstFile( const char *DirOrMask, FtpFindInfo &FindInfo, DTP_TYPE useDTP/* = NO_DTP*/ )
{
  if( NO_DTP != useDTP ) {
//     rawwrite("TYPE A\r\n");
//     skipresponse();
    if( !rawwriteConvParam("LIST %s\r\n", DirOrMask, useDTP) ) {
      LOG_ERR("Failed to LIST %s\n", DirOrMask);
      return false;
    }
  } else if( !rawwriteConvParam("STAT %s\r\n", DirOrMask) ) {
    LOG_ERR("Failed to STAT %s\n", DirOrMask);
    return false;
  }
  CStrBuf listing;
  int n;
  if( NO_DTP != useDTP ){
    readAllData(listing, n);
//     rawwrite("TYPE I\r\n");
//     skipresponse();
  } else if( readresponse(listing, n) != '2' )
    return false;
  //listing.setsize(n+2);
  //listing.buf[n+1] = '\0';
  FindInfo.FindPtr = FindInfo.FindBuff = listing.release();
  FindInfo.ConvertBuff = NULL;
  if( FindNextFile(FindInfo) ) return true;
  else {
    FindClose(FindInfo);
    return false;
  }
}

bool CFtpControl::FindNextFile( FtpFindInfo &FindInfo )
{
  int l = 0;
  do{
    if( *(FindInfo.FindPtr) == '\0' )
      return false;
    char *end = FindInfo.FindPtr;
    do
    {
      l++;
      end++;
    }
    while(
      ( *end != '\r' ) &&
      ( *end != '\0' ) &&
      ( *end != '\n' ) );
    while(
      ( *end == '\n' ) ||
      ( *end == '\r' ) ) end++;
    l = ftpparse(&FindInfo.Data, FindInfo.FindPtr, l);
    FindInfo.FindPtr = end;
  }while( !l );
  if(FindInfo.Data.name)FindInfo.Data.name[FindInfo.Data.namelen] = '\0';
  if(FindInfo.Data.id)FindInfo.Data.id[FindInfo.Data.idlen] = '\0';
  if( ICONV_ERROR != _to_utf8 && FindInfo.Data.name && FindInfo.Data.namelen > 0 ){
    if( !FindInfo.ConvertBuff )
      FindInfo.ConvertBuff = (char*)malloc(VSPRINTF_BUFFER_SIZE+1);
    iconv_cchar *src = FindInfo.Data.name;
    char *dst = FindInfo.ConvertBuff;
    size_t srclen = FindInfo.Data.namelen, dstlen = VSPRINTF_BUFFER_SIZE;
    if( !iconv(_to_utf8, &src, &srclen, &dst, &dstlen) )
      return FindNextFile(FindInfo);
    *dst = '\0';
    FindInfo.Data.name = FindInfo.ConvertBuff;
    //FindInfo.Data.namelen = strlen(FindInfo.ConvertBuff);
  }
  return true;
}

void CFtpControl::FindClose( FtpFindInfo &FindInfo )
{
  free(FindInfo.FindBuff);
  if( FindInfo.ConvertBuff )
    free(FindInfo.ConvertBuff);
}

char CFtpControl::SendCmd(const char *cmd)
{
  char buf[VSPRINTF_BUFFER_SIZE];
  sprintf_s(buf, "%s\r\n", cmd);
  rawwrite(buf);
  return skipresponse();
}

char CFtpControl::SendCmdf(const char *cmd, ...)
{
  char buf[VSPRINTF_BUFFER_SIZE], buff[VSPRINTF_BUFFER_SIZE];
  if( strlen(cmd) + 3 > VSPRINTF_BUFFER_SIZE )
    return 0;
  sprintf_s(buff, "%s\r\n", cmd);
  va_list args;
  va_start(args, cmd);
  _vsnprintf_s(buf, _TRUNCATE, buff, args);
  va_end(args);
  rawwrite(buf);
  return skipresponse();
}

char CFtpControl::SendCmdResp( const char *cmd, CStrBuf &resp, int &r_len )
{
  char buf[VSPRINTF_BUFFER_SIZE];
  if( strlen(cmd) + 3 > VSPRINTF_BUFFER_SIZE )
    return 0;
  sprintf_s(buf, "%s\r\n", cmd);
  rawwrite(buf);
  return readresponse(resp, r_len);
}

bool CFtpControl::SendCmdConn(const char *cmd, bool Active)
{
  char buf[VSPRINTF_BUFFER_SIZE];
  if( strlen(cmd) + 3 > VSPRINTF_BUFFER_SIZE )
    return false;
  sprintf_s(buf, "%s\r\n", cmd);
  if( !RawConnCmd(buf, Active) )
    LOG_THROW(NetDataError, "Cann't initialize %s data connection\n", Active ? "active" : "passive");
  return true;
}

bool CFtpControl::SendCmdConnf(const char *cmd, bool Active, ...)
{
  char buf[VSPRINTF_BUFFER_SIZE], buff[VSPRINTF_BUFFER_SIZE];
  if( strlen(cmd) + 3 > VSPRINTF_BUFFER_SIZE )
    return false;
  sprintf_s(buff, "%s\r\n", cmd);
  va_list args;
  va_start(args, Active);
  _vsnprintf_s(buf, _TRUNCATE, buff, args);
  va_end(args);
  if( !RawConnCmd(buf, Active) )
    LOG_THROW(NetDataError, "Cann't initialize %s data connection\n", Active ? "active" : "passive");
  return true;
}

void CFtpControl::AbortData(void)
{
  try {
    rawwrite("ABOR\r\n");
    skipresponse();
  } catch(const NetworkError) {
    CloseData(true);
    throw;
  }
  CloseData(true);
}

void CFtpControl::SendData(const void *data, int dlen)
{
  LOG_ASSERT(INVALID_SOCKET != dsock, "Invalid socket\n");
  int i, l = dlen;
  const char *d = (const char*)data;
  do
  {
    dsockwait(false);
    if( ( i = send(dsock, d, l, 0) ) == SOCKET_ERROR )
      LOG_THROW(NetDataError, "Data socket write error\n");
    if( !( l -= i ) )
      return;
    d += i;
  }
  while(1);
}

int CFtpControl::RecvData(void *buffer, int blen, bool waitAll/* = false*/)
{
  if( INVALID_SOCKET == dsock )
    return 0;
  int i, l = blen;
  char *b = (char*)buffer;
  do 
  {
    dsockwait(true);
    if( ( i = recv(dsock, (char *)b, l, 0) ) == SOCKET_ERROR )
      LOG_THROW(NetDataError, "Data socket read error\n");
    b += i;
    l -= i;
  }
  while (waitAll && l && i);
  if( !(blen -= l) )
    CloseData();
  return blen;
}

char CFtpControl::CloseData(bool readresp/* = true*/)
{
  if( INVALID_SOCKET == dsock )
    return 0;
  closesocket(dsock);
  dsock = INVALID_SOCKET;
  if( readresp )
    try {
      return skipresponse();
    } catch(const NetworkError) {}
  return 0;
}

#if 0
bool CFtpControl::IsNetworkError(void)
{
  if( INVALID_SOCKET == sock || WSAGetLastError() )
    return true;
  bool er = true;
  int el = sizeof(er);
  getsockopt(sock, SOL_SOCKET, SO_ERROR, (char *)&er, &el);
  return er;
}
#endif

const char *CFtpControl::GetLastResponse(void)
{
  netbuff[__NB_SIZE-1] = '\0';
  return netbuff;
}

char CFtpControl::skipresponse(void)
{
  int i, n = nb_nextcmd;
  char r_endf[4];
  nb_nextcmd = 0;
  while( n < 4 ){//get at least 4 response bytes
    n = rawread(0);
  }
  memcpy(r_endf, netbuff, 4);//save the response code
  if( '-' == r_endf[3] ){//if response is multi-line
    r_endf[3] = ' ';//setup line-ending signature
    do{
      while( ( i = findendstr(n) ) > __NB_SIZE )//get at least a line
        n = rawread(0);
      memmove_s(netbuff, sizeof(netbuff), netbuff + i, n -= i);//move the begining of the next line to the beginning of buffer
      netbuff[n] = '\0';
      while ( n < 4 )//ensure that at least four first bytes of the line were stored to netbuff
        n = rawread(n);
    }while( strncmp(r_endf, netbuff, 4) );
  }
  while( ( i = findendstr(n) ) > __NB_SIZE )//get data till the end of line
    n = rawread(0);
  if( i < n )
    memmove_s(netbuff, sizeof(netbuff), netbuff + i, nb_nextcmd = n - i);
  return r_endf[0];//return the high part of response code
}

char CFtpControl::readresponse( CStrBuf &resp, int &r_len )
{
  int i, n = nb_nextcmd, m;
  char r_endf[4];
  nb_nextcmd = 0;
  while( n < 4 ){//read at least first 4 bytes
    n = rawread(0);
  }
  memcpy(r_endf, netbuff, 4);//save response code to r_endf
  i = findendstr(n);
  resp.setsize(m = min(n,i)-3);//allocate buffer for obtained data without first 4 bytes
  memcpy(resp.buf(), netbuff + 4, r_len=m-1);//save all data to buffer
  while( i > __NB_SIZE ){//continue reading and saving to buffer while \n not obtained
    n = rawread(0);
    i = findendstr(n);
    resp.setsize(r_len+(m=min(n,i))+1);
    memcpy(resp.buf() + r_len, netbuff, m);
    r_len += m;
  }
  if( r_endf[3] == '-' ){//if response is multi-line
    char r_del[4];
    int flag = true;//whether to remove first 5 bytes from each line
    memcpy(r_del /*+ 1*/, r_endf, 4);
    //r_del[0] = ' ';
    r_endf[3] = ' ';//setup line-ending signature
    r_len = 0;//discard the first line
    while(1)
    {
      memmove_s(netbuff, sizeof(netbuff), netbuff + i, n -= i);//move data obtained after end of previous line to the begining of netbuf
      netbuff[n] = '\0';
      while( n < 4 )//be sure that at least first 4 bytes was obtained
        n = rawread(n);
      if( !strncmp(r_endf, netbuff, 4) )//if the beginning of the end response line obtained, exit from loop
        break;
      i = findendstr(n);
      if( flag && !strncmp(r_del, netbuff, 4) ){//check if removing of the first four chars is required
        resp.setsize(r_len+min(n,i)-3);//allocate buffer for obtained (part of) line without first 4 bytes plus ending zero
        memcpy(resp.buf() + r_len, netbuff + 4, m=min(n,i)-4);//save them
      } else {
        resp.setsize(r_len+(m=min(n,i))+1);
        if( netbuff[0]==' ' )//try to remove at least one first space
          memcpy(resp.buf() + r_len, netbuff + 1, --m);//save (part of) line without first space
        else
          memcpy(resp.buf() +r_len, netbuff, m);//save entire (part of) line
        flag = false;//don't check if removing of the first four chars is required anymore
      }
      r_len += m;
      while( i > __NB_SIZE ){//continue reading and saving to buffer without changes while \n not obtained
        n = rawread(0);
        i = findendstr(n);
        resp.setsize(r_len+(m=min(n,i))+1);
        memcpy(resp.buf() + r_len, netbuff, m);
        r_len += m;
      }
    };
    while( ( i = findendstr(n) ) > __NB_SIZE )//read the least of the last response line
      n = rawread(0);
  }
  r_len[resp.buf()] = '\0';//set ending zero
  if( i < n )
    memmove_s(netbuff, sizeof(netbuff), netbuff + i, nb_nextcmd = n - i);
  return r_endf[0];//return the high part of response code
}

void CFtpControl::readAllData(CStrBuf &data, int &d_len)
{
  int len;
  d_len = 0;
  do {
    data.setsize(d_len+__DTP_BUF_SIZE+1);
    len = RecvData((void*)(data.buf() + d_len), __DTP_BUF_SIZE, true);
    d_len += len;
  } while(len);
  d_len[data.buf()] = '\0';
}

void CFtpControl::sockwait(bool forread)
{
  fd_set fds;
  timeval timeout;
  int res;
  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);
  if( forread )
    res = select(sock + 1, &fds, NULL, NULL, &timeout);
  else
    res = select(sock + 1, NULL, &fds, NULL, &timeout);
  if( res<=0 )
    LOG_THROW(NetworkError,
      res ? "Socket is invalid or network error at %s operation\n" : "Wait for %s operation timed out\n",
      forread ? "read" : "write");
}

void CFtpControl::dsockwait(bool forread)
{
  //should we also check control connection?
  fd_set fds;
  timeval timeout;
  int res;
  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(dsock, &fds);
  if( forread )
    res = select(dsock + 1, &fds, NULL, NULL, &timeout);
  else
    res = select(dsock + 1, NULL, &fds, NULL, &timeout);
  if( res<=0 )
    LOG_THROW(NetworkError,
      res ? "Data socket is invalid or network error at %s operation\n" : "Wait for %s data operation timed out\n",
      forread ? "read" : "write");
}

void CFtpControl::rawwrite(const char *str)
{
  LOG_ASSERT(INVALID_SOCKET != sock, "Invalid socket\n");
  int i, l = (int)strlen(str);
  do
  {
    sockwait(false);
    if( ( i = send(sock, str, l, 0) ) == SOCKET_ERROR )
      LOG_THROW(NetworkError, "Socket write error\n");
    if( !( l -= i ) )
      return;
    str += i;
  }
  while(1);
}

void CFtpControl::rawwritef(const char *str, ...)
{
  char buf[VSPRINTF_BUFFER_SIZE];
  va_list args;
  va_start(args, str);
  _vsnprintf_s(buf, _TRUNCATE, str, args);
  va_end(args);
  rawwrite(buf);
}

bool CFtpControl::rawwriteConvParam(const char *str, const char *param, DTP_TYPE initDTP/* = NO_DTP*/)
{
  char buf[VSPRINTF_BUFFER_SIZE+1];
  if( ICONV_ERROR != _from_utf8 ){
    iconv_cchar *src = const_cast<char*>(param);
    char *dst = buf;
    size_t srclen = strlen(src), dstlen = VSPRINTF_BUFFER_SIZE;
    size_t res = iconv(_from_utf8, &src, &srclen, &dst, &dstlen);
    *dst = '\0';
    if( res == (size_t)(-1) ){
      iconv(_from_utf8, NULL, NULL, NULL, NULL);
      switch(errno){
      case E2BIG:
        LOG_ERR("String too long (%s)\n", param);
        break;
      case EILSEQ:
      case EINVAL:
        LOG_ERR("Invalid or broken string (%s)\n", param);
        return false;
      default:
        LOG_ASSERT(false, "Invalid errno value (%d)\n", errno);
      }
    }
    param = buf;
  }
  if( initDTP )
    return RawConnCmdf(str, DTP_ACTIVE == initDTP, param);
  else
    rawwritef(str, param);
  return true;
}

int CFtpControl::rawread(int nb_start)
{
  LOG_ASSERT(INVALID_SOCKET != sock, "Invalid socket\n");
  int i = __NB_SIZE - nb_start;
  char *p = netbuff + nb_start;
  do
  {
    sockwait(true);
    if( ( nb_start = recv(sock, p, i, 0) ) == SOCKET_ERROR )
      LOG_THROW(NetworkError, "Socket read error\n");
    if( !nb_start )
      LOG_THROW(NetworkError, "Connection closed by server\n");
    i -= nb_start;
    p += nb_start;
  }
  while( i && ( p[-1] != '\n' ) && ( p[-1] != '\r' ) );
  if( i ) *p = '\0';//end string
  return (int)(p - netbuff);
}

bool CFtpControl::RawConnCmd(const char *str, bool Active)
{
  LOG_ASSERT(INVALID_SOCKET == dsock, "Previous connection not closed\n");

  dsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  LOG_ASSERT(INVALID_SOCKET != dsock, "Cann't create socket\n");

  struct sockaddr_in sa;
  socklen_t sa_len = sizeof(struct sockaddr_in);

  try {//this is just to handle NetworkError situation (close dsock), exception will be rethrown

    if( Active ){
      //ACTIVE mode: prepare listening socket and sent it's name with PORT command
      // then execute command and accept connection

      if( getsockname(sock, (struct sockaddr*)&sa, &sa_len) || sizeof(struct sockaddr_in) != sa_len )
        LOG_THROW(NetDataError, "Cann't obtain socket name\n");
      for(int busy_retries = 2; busy_retries > 0; --busy_retries){
        sa.sin_port = 0;
        if( bind(dsock, (struct sockaddr*)&sa, sizeof(struct sockaddr_in)) )
          LOG_THROW(NetDataError, "Bind error\n");

        if( listen(dsock, 1) )
          LOG_THROW(NetDataError, "Listen error\n");
        if( getsockname(dsock, (struct sockaddr*)&sa, &sa_len) || sizeof(struct sockaddr_in) != sa_len )
          LOG_THROW(NetDataError, "Cann't obtain socket name\n");
        rawwritef("PORT %u,%u,%u,%u,%u,%u\r\n",
          in_addr_byte(sa.sin_addr,1),
          in_addr_byte(sa.sin_addr,2),
          in_addr_byte(sa.sin_addr,3),
          in_addr_byte(sa.sin_addr,4),
          in_port_byte(sa.sin_port,1),
          in_port_byte(sa.sin_port,2));
        if( '2' != skipresponse() ) break;

        rawwrite(str);//execute command

        timeval timeout;
        timeout.tv_sec = timeout_sec;
        timeout.tv_usec = 0;
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(dsock, &fds);
        FD_SET(sock, &fds);
        if( select(max(sock, dsock) + 1, &fds, NULL, NULL, &timeout) < 1 ) {
          LOG_ERR("Waiting for incoming connection timed out\n");
          skipresponse();
          break;
        }
        if( !FD_ISSET(dsock, &fds) && FD_ISSET(sock, &fds) ) {
          bool is_error = true;
          switch( skipresponse() ){
          case '4':
            LOG_ERR("Server wasn't able to connect:\n\t%s\t%s", str, GetLastResponse());
            usleep(1000000*timeout_sec/5);
            closesocket(dsock);
            dsock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            LOG_ASSERT(INVALID_SOCKET != dsock, "Cann't create socket\n");
            continue;
          case '1':
            is_error = false;
          }
          if( is_error ) break;
        }

        SOCKET s = accept(dsock, NULL, NULL);
        if( INVALID_SOCKET == s ) {
          LOG_ERR("Cann't accept connection\n");
          skipresponse();
          break;
        }

        closesocket(dsock);
        dsock = s;
        if( '1' == skipresponse() )
          return true;
        break;
      }

    } else {
      //PASSIVE mode: obtain server socket name from reply on PASV command
      // then execute command and connect

      CStrBuf p_reply;
      int r_len = 0;
      rawwrite("PASV\r\n");
      if( '2' == readresponse(p_reply, r_len) && r_len ) {
        const char *sname = p_reply.buf();
        while( ( *sname < '0' || *sname > '9' ) && *++sname );
        if( !*sname )
          LOG_THROW(NetDataError, "Invalid PASV response format\n");
        unsigned int b1, b2, b3, b4, p1, p2;
        b1 = b2 = b3 = b4 = p1 = p2 = 0;
        if( sscanf_s(sname, "%u,%u,%u,%u,%u,%u", &b1, &b2, &b3, &b4, &p1, &p2) != 6 )
          LOG_THROW(NetDataError, "Invalid PASV response format\n");
        memset(&sa, 0, sizeof(sa));
        sa.sin_family = AF_INET;
        in_addr_byte(sa.sin_addr,1) = b1;
        in_addr_byte(sa.sin_addr,2) = b2;
        in_addr_byte(sa.sin_addr,3) = b3;
        in_addr_byte(sa.sin_addr,4) = b4;
        in_port_byte(sa.sin_port,1) = p1;
        in_port_byte(sa.sin_port,2) = p2;

        rawwrite(str);//execute command
        if( !safe_connect(dsock, &sa, timeout_sec) )
          skipresponse();
        else if( '1' == skipresponse() )
          return true;
      }
    }

  } catch(const NetworkError) {
    closesocket(dsock);
    dsock = INVALID_SOCKET;
    throw;
  }

  closesocket(dsock);
  dsock = INVALID_SOCKET;
  return false;
}

bool CFtpControl::RawConnCmdf(const char *str, bool Active, ...)
{
  char buf[VSPRINTF_BUFFER_SIZE];
  va_list args;
  va_start(args, Active);
  _vsnprintf_s(buf, _TRUNCATE, str, args);
  va_end(args);
  return RawConnCmd(buf, Active);
}

int CFtpControl::findendstr(int end)
{
  int i;
  char *p = netbuff;
  for(i=0;i<end;i++)
    if( ( *p == '\n' ) || ( *(p++) == '\0' ) )
      return i+1;
  return __NB_SIZE+1;
}

