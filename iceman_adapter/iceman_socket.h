#ifndef __ICEMAN_SOCKET_H__
#define __ICEMAN_SOCKET_H__

#ifdef __MINGW32__
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>


#undef socklen_t
#define socklen_t int
#define sleep(t) _sleep(t * 1000)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

#define MAX_RETRY 20

#define EINIT 	-1
#define ESOCKET -2
#define EBIND 	-3
#define ELISTEN -4


#endif /* __ICEMAN_SOCKET_H__ */
