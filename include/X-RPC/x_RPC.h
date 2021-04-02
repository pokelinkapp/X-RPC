#ifndef X_RPC_H
#define X_RPC_H

#ifdef X_RPC_STANDALONE
#ifdef OS_WIN32
#define X_RPC_EXPORT __declspec(dllexport)
#else
#define X_RPC_EXPORT
#endif
#else
#define X_RPC_EXPORT
#endif

#include "msgpack.h"

typedef enum xRPC_Client_Status {
	xRPC_CLIENT_STATUS_ACTIVE,
	xRPC_CLIENT_STATUS_DISCONNECTED,
	xRPC_CLIENT_STATUS_FAILED
} xRPC_Client_Status;

typedef enum xRPC_Server_Status {
	xRPC_SERVER_STATUS_ACTIVE,
	xRPC_SERVER_STATUS_STOPPED,
	xRPC_SERVER_STATUS_STARTING,
	xRPC_SERVER_STATUS_FAILED
} xRPC_Server_Status;

typedef enum xRPC_Server_Function_Register {
	xRPC_SERVER_FUNCTION_REGISTERED,
	xRPC_SERVER_FUNCTION_NO_NAME,
	xRPC_SERVER_FUNCTION_NAME_TOO_LARGE,
	xRPC_SERVER_FUNCTION_NULL_POINTER,
	xRPC_SERVER_FUNCTION_NAME_EXISTS
} xRPC_Server_Function_Register;

#ifdef __cplusplus
extern "C" {
#endif


// Public Functions
xRPC_Server_Status X_RPC_EXPORT xRPC_Server_Start(unsigned short bindPort, const char* bindIp, unsigned short maxClients);
void X_RPC_EXPORT xRPC_Server_Stop();
xRPC_Server_Function_Register X_RPC_EXPORT xRPC_Server_RegisterCallBack(const char* name, void(*callback)(msgpack_object*, msgpack_packer*));
void X_RPC_EXPORT xRPC_Server_ClearCallbacks();
xRPC_Server_Status X_RPC_EXPORT xRPC_Server_GetStatus();

xRPC_Client_Status X_RPC_EXPORT xRPC_Client_Start(unsigned short targetPort, const char* targetIp);
void X_RPC_EXPORT xRPC_Client_Stop();
msgpack_object X_RPC_EXPORT xRPC_Client_Call(const char* name, msgpack_object* arguments, short timeout, intptr_t* buffLocation);
xRPC_Client_Status X_RPC_EXPORT xRPC_Client_GetStatus();
// Public Functions

#ifdef __cplusplus
};
#endif

#endif //X_RPC_H
