#ifndef __BURNER_SOCKET_H__
#define __BURNER_SOCKET_H__

#ifdef __MINGW32__
#include <winsock2.h>
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


/* Burner server */
int prepare_connect(int port_num);
int wait_connect(void);
int wait_transaction(void);
int close_connect(void);
int burner_receive(char *a_buffer, int a_length);
int burner_send(const char *a_buffer, int a_length);
int close_host(void);


/* TCL client */
void init_tcl_client(void);
void close_tcl_client (void);
int tcl_send(const void *buf, int length);
int tcl_receive(void *buf, int length);

#endif /* __BURNER_SOCKET_H__ */
