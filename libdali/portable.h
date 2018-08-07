/***********************************************************************//**
 * @file portable.h:
 *
 * Platform specific headers.  This file provides a basic level of platform
 * portability.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License (GNU-LGPL) for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software.
 * If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (C) 2016 Chad Trabant, IRIS Data Management Center
 *
 * modified: 2016.291
 ***************************************************************************/

#ifndef PORTABLE_H
#define PORTABLE_H 1

#ifdef __cplusplus
extern "C" {
#endif

/* Portability to the XScale (ARM) architecture, possible others,
 * requires a packed attribute in certain places but this only works
 * with GCC and compatible for now. */
#if defined (__GNUC__)
  #define DLP_PACKED __attribute__ ((packed))
#else
  #define DLP_PACKED
#endif

/* C99 standard headers */
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#if defined(__linux__) || defined(__linux) || defined(__CYGWIN__)
  #define DLP_LINUX 1
  #define DLP_GLIBC2 1 /* Deprecated */

  #include <unistd.h>
  #include <inttypes.h>
  #include <errno.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <sys/time.h>
  #include <sys/utsname.h>
  #include <pwd.h>

#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
  #define DLP_BSD

  #include <unistd.h>
  #include <inttypes.h>
  #include <errno.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <sys/time.h>
  #include <sys/utsname.h>
  #include <pwd.h>

#elif defined(__sun__) || defined(__sun)
  #define DLP_SOLARIS 1

  #include <unistd.h>
  #include <inttypes.h>
  #include <errno.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <netdb.h>
  #include <sys/time.h>
  #include <sys/utsname.h>
  #include <pwd.h>

#elif defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
  #define DLP_WIN 1
  #define DLP_WIN32 1 /* Deprecated */

  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <process.h>
  #include <io.h>

  /* For MSVC 2012 and earlier define standard int types, otherwise use inttypes.h */
  #if defined(_MSC_VER) && _MSC_VER <= 1700
    typedef signed char int8_t;
    typedef unsigned char uint8_t;
    typedef signed short int int16_t;
    typedef unsigned short int uint16_t;
    typedef signed int int32_t;
    typedef unsigned int uint32_t;
    typedef signed __int64 int64_t;
    typedef unsigned __int64 uint64_t;
  #else
    #include <inttypes.h>
  #endif

  #if defined(_MSC_VER)
    #if !defined(PRId64)
      #define PRId64 "I64d"
    #endif
    #if !defined(SCNd64)
      #define SCNd64 "I64d"
    #endif
    #if !defined(ssize_t)
      #define ssize_t SSIZE_T
    #endif
  #endif

  #define strdup _strdup
  #define read _read
  #define write _write
  #define open _open
  #define close _close
  #define snprintf _snprintf
  #define vsnprintf _vsnprintf
  #define strncasecmp _strnicmp

#else
  typedef signed char int8_t;
  typedef unsigned char uint8_t;
  typedef signed short int int16_t;
  typedef unsigned short int uint16_t;
  typedef signed int int32_t;
  typedef unsigned int uint32_t;
  typedef signed long long int64_t;
  typedef unsigned long long uint64_t;

#endif

/* Use int for SOCKET if platform includes have not defined it */
#ifndef SOCKET
  #define SOCKET int
#endif

extern int dlp_sockstartup (void);
extern int dlp_sockconnect (SOCKET socket, struct sockaddr * inetaddr, int addrlen);
extern int dlp_sockclose (SOCKET socket);
extern int dlp_sockblock (SOCKET socket);
extern int dlp_socknoblock (SOCKET socket);
extern int dlp_noblockcheck (void);
extern int dlp_setsocktimeo (SOCKET socket, int timeout);
extern int dlp_setioalarm (int timeout);
extern int dlp_getaddrinfo (char * nodename, char * nodeport,
			    struct sockaddr * addr, size_t * addrlen);
extern int dlp_openfile (const char *filename, char perm);
extern const char *dlp_strerror (void);
extern int64_t dlp_time (void);
extern void dlp_usleep (unsigned long int useconds);
extern int dlp_genclientid (char *progname, char *clientid, size_t maxsize);

#ifdef __cplusplus
}
#endif

#endif /* PORTABLE_H */
