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

#define iconv_cchar const char

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

static void closesocket(int sock)
{
  shutdown(sock, SHUT_RDWR);
  close(sock);
}

#define iconv_cchar char

#endif //_WIN32

#include <string.h>
#include <malloc.h>
#include <stddef.h>
#include <stdio.h>

#include "FtpSockLib.h"
#include "logpp.h"
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

//class CFtpControl

#define _throw_NE throw(CFtpControl::NetworkError)

const char *CFtpControl::DefaultAnsiCP = NULL;

CFtpControl::CFtpControl(): ServerIP(0), ServerPORT(FTP_PORT),
        login(NULL), pass(NULL), timeouts(DEF_TIMEOUTS),
        sock(INVALID_SOCKET), _to_utf8(ICONV_ERROR), _from_utf8(ICONV_ERROR)
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
  if( INVALID_SOCKET != sock ) return false;

#ifdef _WIN32
  int on = 1;
#endif
  struct sockaddr_in sa;

  sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  if( INVALID_SOCKET == sock ) return false;

  do{
#ifdef _WIN32
    if( ( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
        (const char *)&on, sizeof(on)) == SOCKET_ERROR ) ||
      ( setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
        (const char *)&timeouts, sizeof(timeouts)) == SOCKET_ERROR )||
      ( setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
        (const char *)&timeouts, sizeof(timeouts)) == SOCKET_ERROR ))
      break;
#endif
    timeout_sec = timeouts / 1000;

    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(ServerPORT);
    sa.sin_addr.s_addr = ServerIP;
    //memset(&sa.sin_zero, 0, sizeof(sa.sin_zero));
    if( connect(sock, (struct sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR )
      break;

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

bool CFtpControl::Logon(void) _throw_NE
{
  int res;
  rawwritef("USER %s\r\n", login?login:"anonymous");
  res = skipresponse();
  if( res < '2' )
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
  try {
  	rawwrite("QUIT\r\n");
  } catch(const NetworkError) {};
  closesocket(sock);
  sock = INVALID_SOCKET;
}

void CFtpControl::Noop(void) _throw_NE
{
  rawwrite("NOOP\r\n");
  skipresponse();
}

bool CFtpControl::ChDir(const char *dir) _throw_NE
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

void CFtpControl::TryUtf8() _throw_NE
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

bool CFtpControl::FindFirstFile( const char *DirOrMask, FtpFindInfo &FindInfo ) _throw_NE
{
  if( !rawwriteConvParam("STAT %s\r\n", DirOrMask) ) return false;
  CStrBuf listing;
  int n;
  if( readresponse(listing, n) != '2' )
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

char CFtpControl::SendCmd(const char *cmd) _throw_NE
{
  char buf[VSPRINTF_BUFFER_SIZE];
  sprintf_s(buf, "%s\r\n", cmd);
  rawwrite(buf);
  return skipresponse();
}

char CFtpControl::SendCmdf(const char *cmd, ...) _throw_NE
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

char CFtpControl::SendCmdResp( const char *cmd, CStrBuf &resp, int &r_len ) _throw_NE
{
  char buf[VSPRINTF_BUFFER_SIZE];
  if( strlen(cmd) + 3 > VSPRINTF_BUFFER_SIZE )
    return 0;
  sprintf_s(buf, "%s\r\n", cmd);
  rawwrite(buf);
  return readresponse(resp, r_len);
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

char CFtpControl::skipresponse(void) _throw_NE
{
  int i, n;
  char r_endf[4];
  do{//get at least 4 response bytes
    n = rawread(0);
  }while( n < 4 );
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
  while( findendstr(n) > __NB_SIZE )//get data till the end of line
    n = rawread(0);
  return r_endf[0];//return the high part of response code
}

char CFtpControl::readresponse( CStrBuf &resp, int &r_len ) _throw_NE
{
  int i, n, m;
  char r_endf[4];
  do{//read at least first 4 bytes
    n = rawread(0);
  } while( n < 4 );
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
    while( findendstr(n) > __NB_SIZE )//read the least of the last response line
      n = rawread(0);
  }
  r_len[resp.buf()] = '\0';//set ending zero
  return r_endf[0];//return the high part of response code
}

void CFtpControl::sockwait(bool forread) _throw_NE
{
  fd_set fds;
  timeval timeout;
  int res;
  timeout.tv_sec = timeout_sec;
  timeout.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);
  if( forread )
    res = select(0, &fds, NULL, NULL, &timeout);
  else
    res = select(0, NULL, &fds, NULL, &timeout);
  if( res<0 )
    LOG_THROW(NetworkError, "Socket is invalid or network error at %s operation\n", forread ? "read" : "write");
  else if( !res )
    LOG_THROW(NetworkError, "Wait operation timed out\n");
}

void CFtpControl::rawwrite(const char *str) _throw_NE
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

void CFtpControl::rawwritef(const char *str, ...) _throw_NE
{
  char buf[VSPRINTF_BUFFER_SIZE];
  va_list args;
  va_start(args, str);
  _vsnprintf_s(buf, _TRUNCATE, str, args);
  va_end(args);
  rawwrite(buf);
}

bool CFtpControl::rawwriteConvParam(const char *str, const char *param) _throw_NE
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
  rawwritef(str, param);
  return true;
}

int CFtpControl::rawread(int nb_start) _throw_NE
{
  LOG_ASSERT(INVALID_SOCKET != sock, "Invalid socket\n");
  int i = __NB_SIZE - nb_start;
  char *p = netbuff + nb_start;
  do
  {
    sockwait(true);
    if( ( nb_start = recv(sock, p, i, 0) ) == SOCKET_ERROR )
      LOG_THROW(NetworkError, "Socket read error\n");
    i -= nb_start;
    p += nb_start;
  }
  while( i && ( p[-1] != '\n' ) && ( p[-1] != '\r' ) );
  if( i ) *p = '\0';//end string
  return (int)(p - netbuff);
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
