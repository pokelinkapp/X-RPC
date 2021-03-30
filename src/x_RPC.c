#include "X-RPC/x_RPC.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#endif

#define xRPC_BUFFER_SIZE 4096

char xRPC_Buffer[xRPC_BUFFER_SIZE];

bool xRPC_RunServer = false;
bool xRPC_RunServerLoop = false;

int xRPC_opt = true;
int xRPC_master_socket, xRPC_addrlen, xRPC_new_socket,
		xRPC_activity, xRPC_valread, xRPC_sd;
int xRPC_max_sd;
struct sockaddr_in xRPC_address;


typedef void xRPC_CALLBACK(msgpack_object*, msgpack_packer*);

size_t xRPC_CallBackSize = 0;
char** xRPC_CallBackNames;
xRPC_CALLBACK** xRPC_CallBackFunctions;

void xRPC_Server_FailedRequest() {
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	msgpack_pack_char(&pk, 1);

	send(xRPC_sd, sbuf.data, sbuf.size, 0);
}


xRPC_Server_Status xRPC_Server_Start(unsigned short bindPort, const char* bindIp, unsigned short maxClients) {
	if (xRPC_RunServer) {
		return xRPC_SERVER_STATUS_ACTIVE;
	}

	xRPC_RunServer = true;

	int* client_socket = (int*)malloc(maxClients * sizeof(int));

	for (int i = 0; i < maxClients; i++) {
		client_socket[i] = 0;
	}

	fd_set readfds;

	if ((xRPC_master_socket = socket(AF_INET, SOCK_STREAM, getprotobyname("tcp")->p_proto)) == 0) {
		perror("socket failed");
		return xRPC_SERVER_STATUS_FAILED;
	}

	if (setsockopt(xRPC_master_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&xRPC_opt, sizeof(xRPC_opt)) < 0) {
		perror("setsockopt");
		return xRPC_SERVER_STATUS_FAILED;
	}

	xRPC_address.sin_family = AF_INET;
	xRPC_address.sin_addr.s_addr = inet_addr(bindIp);
	xRPC_address.sin_port = htons(bindPort);

	if (bind(xRPC_master_socket, (struct sockaddr*)&xRPC_address, sizeof(xRPC_address)) < 0) {
		perror("bind failed");
		return xRPC_SERVER_STATUS_FAILED;
	}

	if (listen(xRPC_master_socket, 5) < 0) {
		perror("listen");
		return xRPC_SERVER_STATUS_FAILED;
	}

	xRPC_addrlen = sizeof(xRPC_address);

	xRPC_RunServerLoop = true;

	while (xRPC_RunServerLoop) {
		FD_ZERO(&readfds);

		FD_SET(xRPC_master_socket, &readfds);
		xRPC_max_sd = xRPC_master_socket;

		for (int i = 0; i < maxClients; i++) {
			xRPC_sd = client_socket[i];

			if (xRPC_sd > 0) {
				FD_SET(xRPC_sd, &readfds);
			}

			if (xRPC_sd > xRPC_max_sd) {
				xRPC_max_sd = xRPC_sd;
			}
		}

		xRPC_activity = select(xRPC_max_sd + 1, &readfds, NULL, NULL, 0);

		if (xRPC_activity <= 0) {
			continue;
		}

		if (FD_ISSET(xRPC_master_socket, &readfds)) {
			if ((xRPC_new_socket = accept(xRPC_master_socket, (struct sockaddr*)&xRPC_address, &xRPC_addrlen)) < 0) {
				perror("accept");
				xRPC_RunServer = false;
				return xRPC_SERVER_STATUS_FAILED;
			}

			for (int i = 0; i < maxClients; i++) {
				if (client_socket[i] == 0) {
					client_socket[i] = xRPC_new_socket;
					break;
				}
			}
		}

		for (int i = 0; i < maxClients; i++) {
			xRPC_sd = client_socket[i];

			if (FD_ISSET(xRPC_sd, &readfds)) {
				if ((xRPC_valread = read(xRPC_sd, xRPC_Buffer, xRPC_BUFFER_SIZE)) == 0) {
					close(xRPC_sd);
					client_socket[i] = 0;
				} else {

					msgpack_object obj;

					msgpack_zone mempool;

					msgpack_zone_init(&mempool, 2048);

					msgpack_unpack(xRPC_Buffer, xRPC_valread, NULL, &mempool, &obj);

					if (obj.type != MSGPACK_OBJECT_ARRAY) {
						xRPC_Server_FailedRequest();
						break;
					}

					msgpack_object_array arr = obj.via.array;

					if (arr.size <= 0 || arr.ptr[0].type != MSGPACK_OBJECT_STR) {
						xRPC_Server_FailedRequest();
						break;
					}

					msgpack_object_str str = arr.ptr[0].via.str;
					msgpack_object* arg = NULL;

					if (arr.size >= 2) {
						arg = &arr.ptr[1];
					}

					bool foundCallback = false;

					printf("Got request for: %s\n", str.ptr);

					for (int j = 0; j < xRPC_CallBackSize; j++) {
						if (strcmp(xRPC_CallBackNames[j], str.ptr) != 0) {
							foundCallback = true;

							msgpack_sbuffer sbuf;
							msgpack_packer pk;

							msgpack_sbuffer_init(&sbuf);
							msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

							xRPC_CallBackFunctions[j](arg, &pk);

							send(xRPC_sd, sbuf.data, sbuf.size, 0);

							break;
						}
					}

					if (foundCallback == false) {
						xRPC_Server_FailedRequest();
						break;
					}

					msgpack_zone_destroy(&mempool);
				}
			}
		}
	}

	for (int i = 0; i < maxClients; i++) {
		if (client_socket == 0) {
			continue;
		}
		xRPC_sd = client_socket[i];

		if (FD_ISSET(xRPC_sd, &readfds)) {
			close(xRPC_sd);
			client_socket[i] = 0;
		}
	}

	close(xRPC_master_socket);

	free(client_socket);

	xRPC_RunServer = false;

	return xRPC_SERVER_STATUS_STOPPED;
}

void xRPC_Server_Stop() {
	xRPC_RunServerLoop = false;
}

xRPC_Server_Function_Register xRPC_Server_RegisterCallBack(const char* name, void(* callback)(msgpack_object*, msgpack_packer*)) {
	unsigned long nameLength = strlen(name);
	if (nameLength > 20) {
		return xRPC_SERVER_FUNCTION_NAME_TOO_LARGE;
	}

	if (nameLength == 0) {
		return xRPC_SERVER_FUNCTION_NO_NAME;
	}

	if (callback == NULL || callback == 0) {
		return xRPC_SERVER_FUNCTION_NULL_POINTER;
	}

	for (int i = 0; i < xRPC_CallBackSize; i++) {
		if (strcmp(xRPC_CallBackNames[i], name) == 0) {
			return xRPC_SERVER_FUNCTION_NAME_EXISTS;
		}
	}

	xRPC_CallBackSize += 1;

	if (xRPC_CallBackSize > 1) {
		char** oldNames = xRPC_CallBackNames;
		xRPC_CALLBACK** oldFunctions = xRPC_CallBackFunctions;
		xRPC_CallBackNames = malloc(xRPC_CallBackSize * sizeof(char*));
		xRPC_CallBackFunctions = malloc(xRPC_CallBackSize * sizeof(xRPC_CALLBACK*));

		memcpy(xRPC_CallBackNames, oldNames, (xRPC_CallBackSize - 1) * sizeof(char*));
		free(oldNames);

		memcpy(xRPC_CallBackFunctions, oldFunctions, (xRPC_CallBackSize - 1) * sizeof(xRPC_CALLBACK*));
		free(oldFunctions);
	} else {
		xRPC_CallBackNames = malloc(sizeof(char*));
		xRPC_CallBackFunctions = malloc(sizeof(xRPC_CALLBACK*));
	}

	xRPC_CallBackNames[xRPC_CallBackSize - 1] = malloc(20);

	char* parsedName = malloc(20);

	for (int i = 0; i < 20; i++) {
		if (i < nameLength) {
			parsedName[i] = name[i];
		} else {
			parsedName[i] = '\0';
		}
	}

	memcpy(xRPC_CallBackNames[xRPC_CallBackSize - 1], parsedName, 20);
	xRPC_CallBackFunctions[xRPC_CallBackSize - 1] = callback;

	return xRPC_SERVER_FUNCTION_REGISTERED;
}

void xRPC_Server_ClearCallbacks() {
	for (int i = 0; i < xRPC_CallBackSize; i++) {
		free(xRPC_CallBackNames[i]);
	}

	free(xRPC_CallBackNames);
	free(xRPC_CallBackFunctions);
}

xRPC_Server_Status xRPC_Server_GetStatus() {
	return xRPC_RunServerLoop ? xRPC_SERVER_STATUS_ACTIVE : xRPC_RunServer ? xRPC_SERVER_STATUS_STARTING : xRPC_SERVER_STATUS_STOPPED;
}

bool xRPC_RunClient = false;
int xRPC_sockfd, xRPC_connfd;
fd_set xRPC_ClientSet;
struct sockaddr_in xRPC_servaddr, xRPC_cli;
char xRPC_Client_Buffer[xRPC_BUFFER_SIZE];

xRPC_Client_Status xRPC_Client_Start(unsigned short targetPort, const char* targetIp) {
	xRPC_sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (xRPC_sockfd == -1) {
		perror("socket");
		return xRPC_CLIENT_STATUS_FAILED;
	}

	xRPC_servaddr.sin_family = AF_INET;
	xRPC_servaddr.sin_addr.s_addr = inet_addr(targetIp);
	xRPC_servaddr.sin_port = htons(targetPort);

	if (connect(xRPC_sockfd, (struct sockaddr*)&xRPC_servaddr, sizeof(xRPC_servaddr)) != 0) {
		perror("connect");
		return xRPC_CLIENT_STATUS_FAILED;
	}

	xRPC_RunClient = true;

	return xRPC_CLIENT_STATUS_ACTIVE;
}

void xRPC_Client_Stop() {
	if (xRPC_RunClient == false) {
		return;
	}
	close(xRPC_sockfd);
	xRPC_RunClient = false;
}

msgpack_object xRPC_Client_Call(const char* name, msgpack_object* arguments, short timeout) {
	msgpack_object output;
	output.type = MSGPACK_OBJECT_NIL;
	if (xRPC_RunClient == false) {
		return output;
	}

	struct timeval schedule;
	schedule.tv_sec = timeout;

	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	msgpack_pack_array(&pk, 2);

	msgpack_pack_str(&pk, strlen(name));
	msgpack_pack_str_body(&pk, name, strlen(name));

	msgpack_object args;

	if (arguments == NULL) {
		args.type = MSGPACK_OBJECT_NIL;
	} else {
		args = *arguments;
	}

	msgpack_pack_object(&pk, args);

	write(xRPC_sockfd, sbuf.data, sbuf.size);
	FD_ZERO(&xRPC_ClientSet);
	FD_SET(xRPC_sockfd, &xRPC_ClientSet);

	xRPC_activity = select(xRPC_sockfd + 1, &xRPC_ClientSet, NULL, NULL, &schedule);

	if (xRPC_activity <= 0) {
		return output;
	}

	xRPC_valread = read(xRPC_sockfd, xRPC_Client_Buffer, xRPC_BUFFER_SIZE);

	msgpack_zone mempool;

	msgpack_zone_init(&mempool, 2048);

	msgpack_unpack(xRPC_Client_Buffer, xRPC_valread, NULL, &mempool, &output);

	msgpack_zone_destroy(&mempool);

	return output;
}

xRPC_Client_Status xRPC_Client_GetStatus() {
	return xRPC_RunClient ? xRPC_CLIENT_STATUS_ACTIVE : xRPC_CLIENT_STATUS_DISCONNECTED;
}