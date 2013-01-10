#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "burner_socket.h"

#define SOCKET_NONBLOCK

#ifdef __MINGW32__

static SOCKET host_descriptor;
static SOCKET client_descriptor;

int prepare_connect(int port_num) {
	static bool winsock_init = false;
	struct sockaddr_in sockaddr;

	if (winsock_init == false)
	{
		WSADATA wsadata;
		if (WSAStartup (MAKEWORD (1, 0), &wsadata) == SOCKET_ERROR)
			return EINIT;

		winsock_init = true;
	}

	// Create socket to wait for connection from gdb
	host_descriptor = socket (PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (host_descriptor == INVALID_SOCKET)
		return ESOCKET;

	int optval = 1;
	setsockopt (host_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

#ifdef SOCKET_NONBLOCK
	unsigned long non_blocking = 1;
	if (ioctlsocket (host_descriptor, FIONBIO, &non_blocking) == SOCKET_ERROR)
	{
		closesocket (host_descriptor);
		return ESOCKET;
	}
#endif

	// Listen on specified port
	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons (port_num_);
	sockaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind (host_descriptor, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
		return EBIND;

	if (listen (host_descriptor, 1) < 0)
		return ELISTEN;

	return host_descriptor;
}

int wait_connect(void) {
	struct sockaddr_in sockaddr;
	int sockaddr_len = sizeof (sockaddr);

	do {

#ifdef SOCKET_NONBLOCK
		Sleep (3000);
#endif

		client_descriptor = accept (_host_descriptor, (struct sockaddr *)&sockaddr, &sockaddr_len);

	} while (client_descriptor < 0);

	/* Allow rapid reuse of this port. */
	int optval = 1;
	setsockopt (client_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	/* Enable TCP keep alive process. */
	optval = 1;
	setsockopt (client_descriptor, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval));
	/* Tell TCP not to delay small packets. This greatly speeds up interactive response. */
	optval = 1;
	setsockopt (client_descriptor, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));

	return client_descriptor;
}

int wait_transaction(void) {
	int result;
	fd_set read_fds;
	struct timeval tv;

	FD_ZERO(&read_fds);
	FD_SET(client_descriptor, &read_fds);

	if (timed_wait_)
	{
		tv.tv_sec = 0;
		tv.tv_usec = timeout_microseconds_;
		result = select (client_descriptor + 1, &read_fds, NULL, NULL, &tv);
	}
	else
	{
		result = select (client_descriptor + 1, &read_fds, NULL, NULL, NULL);
	}

	return result;
}

int close_connect(void) {
	if (client_descriptor != INVALID_SOCKET)
	{
		closesocket (client_descriptor);
		client_descriptor = INVALID_SOCKET;
	}

	return 0;
}

int burner_receive(char *a_buffer, int a_length) {
	return recv (client_descriptor, a_buffer, a_length, 0);
}

int burner_send(const char *a_buffer, int a_length) {
	return send (client_descriptor, a_buffer, a_length, 0);
}

int close_host(void) {
	WSACleanup ();

	if (host_descriptor != INVALID_SOCKET)
	{
		closesocket (host_descriptor);
		host_descriptor = INVALID_SOCKET;
	}

	return 0;
}

#else

#ifdef SOCKET_NONBLOCK
#include <fcntl.h>
#endif

static int host_descriptor;
static int client_descriptor;

int prepare_connect(int port_num) {
	struct sockaddr_in sockaddr;

	host_descriptor = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (host_descriptor < 0)
		return ESOCKET;

	/* Allow rapid reuse of this port. */
	socklen_t optval = 1;
	setsockopt(host_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

#ifdef SOCKET_NONBLOCK
	int oldopts = fcntl(host_descriptor, F_GETFL, 0);
	fcntl(host_descriptor, F_SETFL, oldopts | O_NONBLOCK);
#endif

	/* Listen on specified port */
	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons(port_num);
	sockaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(host_descriptor, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0)
		return EBIND;

	if (listen(host_descriptor, 1) < 0)
		return ELISTEN;

	return host_descriptor;
}

int wait_connect(void) {
	struct sockaddr_in sockaddr;
	socklen_t sockaddr_len = sizeof(sockaddr);

	do {
#ifdef SOCKET_NONBLOCK
		usleep(3000);
#endif
		client_descriptor = accept(host_descriptor, (struct sockaddr *)&sockaddr, &sockaddr_len);

	} while(client_descriptor < 0);

	/* Allow rapid reuse of this port. */
	socklen_t optval = 1;
	setsockopt(client_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	/* Enable TCP keep alive process. */
	optval = 1;
	setsockopt(client_descriptor, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval));
	/* Tell TCP not to delay small packets. This greatly speeds up interactive response. */
	optval = 1;
	setsockopt(client_descriptor, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));
	/* connected_ = true; */

	return client_descriptor;
}

int wait_transaction(void) {
	int result;
	fd_set read_fds;
	struct timeval tv;

	FD_ZERO(&read_fds);
	FD_SET(client_descriptor, &read_fds);

	/*
	if (timed_wait_)
	{
		tv.tv_sec = 0;
		tv.tv_usec = timeout_microseconds_;
		result = select (client_descriptor + 1, &read_fds, 0, 0, &tv);
	}
	else
	*/
	{
		result = select (client_descriptor + 1, &read_fds, 0, 0, 0);
	}

	return result;
}

int close_connect(void) {
	if (client_descriptor != -1)
	{
		close(client_descriptor);
		client_descriptor = -1;
	}

	return 0;
}

int burner_receive(char *a_buffer, int a_length) {
	return read(client_descriptor, a_buffer, a_length);
}

int burner_send(const char *a_buffer, int a_length) {
	int result;

	result = write(client_descriptor, a_buffer, a_length);

	if (result == EPIPE)
		close_connect();

	return result;
}

int close_host(void) {
	if (host_descriptor != -1)
	{
		close(host_descriptor);
		host_descriptor = -1;
	}

	return 0;
}
#endif

#ifdef __MINGW32__
SOCKET tcl_client;
#else
int tcl_client;
#endif

extern int openocd_tcl_port;

/* Prepare socket communication */
void init_tcl_client(void) {
	char ip[20] = "127.0.0.1";
	struct sockaddr_in server_addr;  

#ifdef __MINGW32__
	WSADATA wsadata;
	if (WSAStartup(MAKEWORD(1,1), &wsadata) == SOCKET_ERROR) {
	}
#endif

#ifdef __MINGW32__
	if ((tcl_client = socket(PF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
#else
	if ((tcl_client = socket(PF_INET, SOCK_STREAM, 0)) == -1)
#endif
		exit(1);

	server_addr.sin_family = PF_INET;
	server_addr.sin_port = htons(openocd_tcl_port);
#ifdef __MINGW32__
	server_addr.sin_addr.s_addr = inet_addr(ip);
#else
	inet_aton(ip, &server_addr.sin_addr);
#endif 
	memset(&(server_addr.sin_zero),0,8);
	if (connect(tcl_client, (struct sockaddr *)&server_addr,
				sizeof(struct sockaddr)) == -1)
		exit(1);
}

void close_tcl_client (void)
{
	/* free socket resource */
#ifdef __MINGW32__
	closesocket (tcl_client);
	do
	{
		WSACleanup ();
	} while (WSAGetLastError () != WSANOTINITIALISED);
#else
	close (tcl_client);
#endif
}

int tcl_send(const void *buf, int length)
{
#ifdef __MINGW32__
	return send(tcl_client, buf, length, 0);
#else
	return write(tcl_client, buf, length);
#endif
}

int tcl_receive(void *buf, int length)
{
#ifdef __MINGW32__
	return recv(tcl_client, buf, length, 0);
#else
	return read(tcl_client, buf, length);
#endif
}
