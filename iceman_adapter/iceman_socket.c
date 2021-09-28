#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "iceman_socket.h"

#define SOCKET_NONBLOCK

#ifdef __MINGW32__
static int winsock_init = 0;
int prepare_connect(int port_num)
{
	struct sockaddr_in sockaddr;
	SOCKET host_descriptor;

	if (winsock_init == 0) {
		WSADATA wsadata;
		if (WSAStartup(MAKEWORD(1, 0), &wsadata) == SOCKET_ERROR)
			return EINIT;

		winsock_init = 1;
	}

	/* Create socket to wait for connection from gdb */
	host_descriptor = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (host_descriptor == INVALID_SOCKET)
		return ESOCKET;

	int optval = 1;

	/*setsockopt(host_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));*/
	setsockopt(host_descriptor, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&optval, sizeof(optval));

#ifdef SOCKET_NONBLOCK
	unsigned long non_blocking = 1;
	if (ioctlsocket(host_descriptor, FIONBIO, &non_blocking) == SOCKET_ERROR) {
		closesocket(host_descriptor);
		return ESOCKET;
	}
#endif

	// Listen on specified port
	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons(port_num);
	sockaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(host_descriptor, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		closesocket(host_descriptor);
		return EBIND;
	}

	if (listen(host_descriptor, 1) < 0) {
		closesocket(host_descriptor);
		return ELISTEN;
	}

	return host_descriptor;
}

int close_host(SOCKET host_descriptor)
{
	WSACleanup ();
	winsock_init = 0;
	if (host_descriptor != INVALID_SOCKET) {
		closesocket(host_descriptor);
		host_descriptor = INVALID_SOCKET;
	}

	return 0;
}

#else
extern void close(int);
#ifdef SOCKET_NONBLOCK
#include <fcntl.h>
#endif

int prepare_connect(int port_num)
{
	struct sockaddr_in sockaddr;
	int host_descriptor;

	host_descriptor = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (host_descriptor < 0)
		return ESOCKET;

	/* Allow rapid reuse of this port. */
	//socklen_t optval = 1;
	//setsockopt(host_descriptor, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

#ifdef SOCKET_NONBLOCK
	int oldopts = fcntl(host_descriptor, F_GETFL, 0);
	fcntl(host_descriptor, F_SETFL, oldopts | O_NONBLOCK);
#endif

	/* Listen on specified port */
	sockaddr.sin_family = PF_INET;
	sockaddr.sin_port = htons(port_num);
	sockaddr.sin_addr.s_addr = INADDR_ANY;

	if (bind(host_descriptor, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
		close(host_descriptor);
		return EBIND;
	}

	if (listen(host_descriptor, 1) < 0) {
		close(host_descriptor);
		return ELISTEN;
	}

	return host_descriptor;
}

int close_host(int host_descriptor)
{
	if (host_descriptor != -1) {
		close(host_descriptor);
		host_descriptor = -1;
	}

	return 0;
}
#endif

#define MAX_REGISTRY_NUMS  64
int registry_port_num[MAX_REGISTRY_NUMS] = {0, 0, 0, 0, 0, 0, 0, 0};
unsigned int registry_port_index=0;

int nds32_registry_portnum_without_bind(int port_num)
{
	registry_port_num[registry_port_index] = port_num;
	if (registry_port_index >= MAX_REGISTRY_NUMS)
		return -1;
	registry_port_index++;
	return 0;
}

int nds32_registry_portnum(int port_num)
{
	unsigned int retry_time, retry_port_num=MAX_RETRY, i, not_registry;
	int host_descriptor;

	retry_time = 0;
	host_descriptor = -1;
	while (retry_port_num) {
		while (1) {
			not_registry=1;
			for (i = 0; i < registry_port_index; i++) {
				if (port_num == registry_port_num[i]) {
					not_registry = 0;
					break;
				}
			}

			if (not_registry)
				break;
			port_num++;
		}
		while (host_descriptor < 0) {
			host_descriptor = prepare_connect(port_num);
			if ((retry_time > MAX_RETRY) || (host_descriptor > 0))
				break;
			retry_time++;
		}

		if (host_descriptor > 0)
			break;
		port_num++;
		retry_port_num--;
	}

	if(host_descriptor < 0)
		return -1;
	close_host(host_descriptor);
	registry_port_num[registry_port_index] = port_num;
	if (registry_port_index >= MAX_REGISTRY_NUMS)
		return -1;
	registry_port_index++;
	return port_num;
}

