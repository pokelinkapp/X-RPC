#include "X-RPC/x_RPC.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#define SD_BOTH SHUT_RDWR
#endif

#define xRPC_BUFFER_SIZE 256

char xRPC_Buffer[xRPC_BUFFER_SIZE];

bool xRPC_RunServer = false;
bool xRPC_RunServerLoop = false;

int xRPC_opt = 1;
int xRPC_master_socket, xRPC_addrlen, xRPC_new_socket,
		xRPC_activity, xRPC_valread, xRPC_sd;
int xRPC_max_sd;
struct sockaddr_in xRPC_address;
struct timeval xRPC_Timeout;


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

	xRPC_Timeout.tv_sec = 2;

	xRPC_RunServer = true;

	int* client_socket = (int*)malloc(maxClients * sizeof(int));

	for (int i = 0; i < maxClients; i++) {
		client_socket[i] = 0;
	}

	fd_set readfds;

#ifdef WIN32
	WSADATA wsaData;
	WORD mVersionRequested = MAKEWORD(2, 2);
	WSAStartup(mVersionRequested, &wsaData);
#endif
	if ((xRPC_master_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
		xRPC_RunServer = false;
		perror("socket failed");
		return xRPC_SERVER_STATUS_FAILED;
	}

	if (setsockopt(xRPC_master_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&xRPC_opt, sizeof(xRPC_opt)) < 0) {
		xRPC_RunServer = false;
		perror("setsockopt");
		return xRPC_SERVER_STATUS_FAILED;
	}

	xRPC_address.sin_family = AF_INET;
	xRPC_address.sin_addr.s_addr = inet_addr(bindIp);
	xRPC_address.sin_port = htons(bindPort);

	if (bind(xRPC_master_socket, (struct sockaddr*)&xRPC_address, sizeof(xRPC_address)) < 0) {
		xRPC_RunServer = false;
		perror("bind failed");
		return xRPC_SERVER_STATUS_FAILED;
	}

	if (listen(xRPC_master_socket, 5) < 0) {
		xRPC_RunServer = false;
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

		xRPC_activity = select(xRPC_max_sd + 1, &readfds, NULL, NULL, &xRPC_Timeout);

		if (xRPC_activity <= 0) {
			continue;
		}

		if (FD_ISSET(xRPC_master_socket, &readfds)) {
			if ((xRPC_new_socket = accept(xRPC_master_socket, (struct sockaddr*)&xRPC_address, &xRPC_addrlen)) < 0) {
				perror("accept");
				xRPC_RunServer = false;
				shutdown(xRPC_master_socket, SD_BOTH);
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
				if ((xRPC_valread = recv(xRPC_sd, xRPC_Buffer, xRPC_BUFFER_SIZE, 0)) == 0) {
					shutdown(xRPC_sd, SD_BOTH);
					client_socket[i] = 0;
				} else {

					char* midBuff = malloc(xRPC_valread);
					long totalSize = xRPC_valread;
					long pos = xRPC_valread;

					memcpy(midBuff, xRPC_Buffer, xRPC_valread);

					if (xRPC_valread == xRPC_BUFFER_SIZE) {
						while ((xRPC_valread = recv(xRPC_sd, xRPC_Buffer, xRPC_BUFFER_SIZE, 0)) == xRPC_BUFFER_SIZE) {
							totalSize += xRPC_valread;
							char* oldbuffer = midBuff;
							midBuff = realloc(midBuff, totalSize);
							for (int i = 0; i < xRPC_valread; i++) {
								midBuff[pos] = xRPC_Buffer[i];
								pos++;
							}
							free(oldbuffer);
						}
						totalSize += xRPC_valread;
						midBuff = realloc(midBuff, totalSize);
						for (int i = 0; i < xRPC_valread; i++) {
							midBuff[pos] = xRPC_Buffer[i];
							pos++;
						}
					}

					msgpack_object obj;

					msgpack_zone mempool;

					msgpack_zone_init(&mempool, 2048);

					msgpack_unpack(midBuff, totalSize, NULL, &mempool, &obj);

					if (obj.type != MSGPACK_OBJECT_ARRAY) {
						free(midBuff);
						xRPC_Server_FailedRequest();
						break;
					}

					msgpack_object_array arr = obj.via.array;

					if (arr.size <= 0 || arr.ptr[0].type != MSGPACK_OBJECT_STR) {
						free(midBuff);
						xRPC_Server_FailedRequest();
						break;
					}

					msgpack_object_str str = arr.ptr[0].via.str;
					msgpack_object* arg = NULL;

					if (arr.size >= 2) {
						arg = &arr.ptr[1];
					}

					bool foundCallback = false;

					for (int j = 0; j < xRPC_CallBackSize; j++) {
						if (strncmp(xRPC_CallBackNames[j], str.ptr, str.size) == 0) {
							foundCallback = true;

							msgpack_sbuffer sbuf;
							msgpack_packer pk;

							msgpack_sbuffer_init(&sbuf);
							msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

							xRPC_CallBackFunctions[j](arg, &pk);

							send(xRPC_sd, sbuf.data, sbuf.size, 0);

							msgpack_sbuffer_destroy(&sbuf);

							break;
						}
					}

					free(midBuff);

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
#ifndef WIN32
			close(xRPC_sd);
#endif
			client_socket[i] = 0;
		}
	}

#ifdef WIN32
	WSACleanup();
#else
	close(xRPC_master_socket);
#endif

	free(client_socket);

	return xRPC_SERVER_STATUS_STOPPED;
}

void xRPC_Server_Stop() {
	xRPC_RunServerLoop = false;
	xRPC_RunServer = false;
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

	xRPC_CallBackSize = 0;
}

xRPC_Server_Status xRPC_Server_GetStatus() {
	return xRPC_RunServerLoop ? xRPC_SERVER_STATUS_ACTIVE : xRPC_RunServer ? xRPC_SERVER_STATUS_STARTING : xRPC_SERVER_STATUS_STOPPED;
}

bool xRPC_RunClient = false;
char xRPC_Client_Buffer[xRPC_BUFFER_SIZE];
int xRPC_sockfd, xRPC_connfd;
fd_set xRPC_ClientSet;
struct sockaddr_in xRPC_servaddr, xRPC_cli;

xRPC_Client_Status xRPC_Client_Start(unsigned short targetPort, const char* targetIp) {
	if (xRPC_RunClient) {
		return xRPC_CLIENT_STATUS_ACTIVE;
	}
#ifdef WIN32
	WSADATA wsaData;
	WORD mVersionRequested = MAKEWORD(2, 2);
	WSAStartup(mVersionRequested, &wsaData);
#endif

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
#ifdef WIN32
	WSACleanup();
#else
	close(xRPC_sockfd);
#endif
	xRPC_RunClient = false;
}

xRPC_Package xRPC_Client_Call(const char* name, msgpack_object* arguments, short timeout) {
	xRPC_Package output;
	output.data.type = MSGPACK_OBJECT_NIL;
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

	msgpack_pack_str(&pk, strlen(name) + 1);
	msgpack_pack_str_body(&pk, name, strlen(name) + 1);

	msgpack_object args;

	if (arguments == NULL) {
		args.type = MSGPACK_OBJECT_NIL;
	} else {
		args = *arguments;
	}

	msgpack_pack_object(&pk, args);

	send(xRPC_sockfd, sbuf.data, sbuf.size, 0);

	msgpack_sbuffer_destroy(&sbuf);

	FD_ZERO(&xRPC_ClientSet);
	FD_SET(xRPC_sockfd, &xRPC_ClientSet);

	xRPC_activity = select(xRPC_sockfd + 1, &xRPC_ClientSet, NULL, NULL, &schedule);
#ifdef WIN32
	if (xRPC_activity < 0) {
#else
	if (xRPC_activity == 0) {
#endif
		return output;
	}

	xRPC_valread = recv(xRPC_sockfd, xRPC_Client_Buffer, xRPC_BUFFER_SIZE, 0);

	if (xRPC_valread == 0) {
		xRPC_Client_Stop();
		return output;
	}

	output.buffer = malloc(xRPC_valread);
	size_t totalSize = xRPC_valread;
	long pos = xRPC_valread;

	memcpy(output.buffer, xRPC_Client_Buffer, xRPC_valread);

	if (xRPC_valread == xRPC_BUFFER_SIZE) {
		while ((xRPC_valread = recv(xRPC_sockfd, xRPC_Client_Buffer, xRPC_BUFFER_SIZE, 0)) == xRPC_BUFFER_SIZE) {
			totalSize += xRPC_valread;
			char* oldBuffer = output.buffer;
			output.buffer = realloc(output.buffer, totalSize);
			for (int i = 0; i < xRPC_valread; i++) {
				output.buffer[pos] = xRPC_Client_Buffer[i];
				pos++;
			}
			free(oldBuffer);
		}
		char* oldBuffer = output.buffer;
		totalSize += xRPC_valread;
		output.buffer = realloc(output.buffer, totalSize);
		for (int i = 0; i < xRPC_valread; i++) {
			output.buffer[pos] = xRPC_Client_Buffer[i];
			pos++;
		}
		free(oldBuffer);
	}

	msgpack_zone mempool;

	msgpack_zone_init(&mempool, 2048);

	msgpack_unpack(output.buffer, totalSize, NULL, &mempool, &output.data);

	msgpack_zone_destroy(&mempool);

	return output;
}

xRPC_Client_Status xRPC_Client_GetStatus() {
	return xRPC_RunClient ? xRPC_CLIENT_STATUS_ACTIVE : xRPC_CLIENT_STATUS_DISCONNECTED;
}

void xRPC_Destroy_Package(xRPC_Package* package) {
	free(package->buffer);
}
