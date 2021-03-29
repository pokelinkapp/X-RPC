#include "X-RPC/x_RPC.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <netdb.h>

#define xRPC_BUFFER_SIZE 512

char xRPC_Buffer[xRPC_BUFFER_SIZE];

bool xRPC_RunServer = false;
bool xRPC_RunClient = false;

int xRPC_opt = true;
int xRPC_master_socket, xRPC_addrlen, xRPC_new_socket,
		xRPC_activity, xRPC_valread, xRPC_sd;
int xRPC_max_sd;
struct sockaddr_in xRPC_address;

typedef msgpack_object* CALLBACK(msgpack_object_array*);

void xRPC_Server_FailedRequest() {
	msgpack_sbuffer sbuf;
	msgpack_packer pk;

	msgpack_sbuffer_init(&sbuf);
	msgpack_packer_init(&pk, &sbuf, msgpack_sbuffer_write);

	msgpack_pack_char(&pk, 0);

	send(xRPC_sd, sbuf.data, sbuf.size, 0);

	msgpack_packer_free(&pk);
	msgpack_sbuffer_destroy(&sbuf);
}

size_t xRPC_CallBackSize = 0;
char** xRPC_CallBackNames;
CALLBACK** xRPC_CallBackFunctions;

struct timeval timeout;


xRPC_Server_Status xRPC_Server_Start(unsigned short bindPort, const char* bindIp, unsigned short maxClients) {
	if (xRPC_RunServer) {
		return xRPC_SERVER_STATUS_ACTIVE;
	}

	timeout.tv_sec = 2;

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

	while (xRPC_RunServer) {
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

		xRPC_activity = select(xRPC_max_sd + 1, &readfds, NULL, NULL, &timeout);

		if (FD_ISSET(xRPC_master_socket, &readfds)) {
			if ((xRPC_new_socket = accept(xRPC_master_socket, (struct sockaddr*)&xRPC_address, (socklen_t*)&xRPC_addrlen)) < 0) {
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
					msgpack_unpacker unp;

					bool result = msgpack_unpacker_init(&unp, xRPC_valread);

					if (result) {
						msgpack_unpacked und;
						msgpack_unpack_return ret;
						msgpack_unpacked_init(&und);
						ret = msgpack_unpacker_next(&unp, &und);
						switch (ret) {
							case MSGPACK_UNPACK_SUCCESS: {
								msgpack_object obj = und.data;
								if (obj.type != MSGPACK_OBJECT_ARRAY) {
									xRPC_Server_FailedRequest();
									break;
								}

								msgpack_object_array arr = obj.via.array;

								if (arr.size <= 0 || arr.ptr[0].type != MSGPACK_OBJECT_STR) {
									xRPC_Server_FailedRequest();
									break;
								}

								if (arr.size >= 2 && arr.ptr[1].type != MSGPACK_OBJECT_ARRAY) {
									xRPC_Server_FailedRequest();
									break;
								}

								msgpack_object_str str = arr.ptr[0].via.str;
								msgpack_object_array* arr2 = NULL;

								if (arr.size >= 2) {
									arr2 = &arr.ptr[1].via.array;
								}

								bool foundCallback = false;

								for (int j = 0; j < xRPC_CallBackSize; j++) {
									if (strcmp(xRPC_CallBackNames[j], str.ptr) != 0) {
										foundCallback = true;
										xRPC_CallBackFunctions[j](arr2);
										break;
									}
								}

								if (foundCallback == false) {
									xRPC_Server_FailedRequest();
									break;
								}

							}
								break;
							default:
							case MSGPACK_UNPACK_EXTRA_BYTES:
							case MSGPACK_UNPACK_CONTINUE:
							case MSGPACK_UNPACK_PARSE_ERROR:
							case MSGPACK_UNPACK_NOMEM_ERROR:
								break;
						}
						msgpack_unpacked_destroy(&und);
					} else {
						xRPC_Server_FailedRequest();
					}

					msgpack_unpacker_destroy(&unp);
				}
			}
		}
	}

	close(xRPC_master_socket);

	xRPC_RunServer = false;

	return xRPC_SERVER_STATUS_STOPPED;
}

void xRPC_Server_Stop() {
	xRPC_RunServer = false;
}

xRPC_Server_Function_Register xRPC_Server_RegisterCallBack(const char* name, msgpack_object* (* callback)(msgpack_object_array*)) {
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
		CALLBACK** oldFunctions = xRPC_CallBackFunctions;
		xRPC_CallBackNames = malloc(xRPC_CallBackSize * sizeof(char*));
		xRPC_CallBackFunctions = malloc(xRPC_CallBackSize * sizeof(CALLBACK*));

		memcpy(xRPC_CallBackNames, oldNames, (xRPC_CallBackSize - 1) * sizeof(char*));
		free(oldNames);

		memcpy(xRPC_CallBackFunctions, oldFunctions, (xRPC_CallBackSize - 1) * sizeof(CALLBACK*));
		free(oldFunctions);
	} else {
		xRPC_CallBackNames = malloc(sizeof(char*));
		xRPC_CallBackFunctions = malloc(sizeof(CALLBACK*));
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


void xRPC_Client_Start(unsigned short targetPort, const char* targetIp) {

}

xRPC_Client_Status xRPC_Client_GetStatus() {
	return xRPC_CLIENT_STATUS_ACTIVE;
}

msgpack_object* xRPC_Client_Call(const char* name, msgpack_object arguments, short timeout) {
	return NULL;
}