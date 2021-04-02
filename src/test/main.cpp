#include "X-RPC/x_RPC.h"
#include <thread>
#include <string>

char text[30];

void HelloWorld(msgpack_object* args, msgpack_packer* packer) {

	if (args->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
		return;
	}

	sprintf(text, "Hello World %lu", (unsigned long)args->via.u64);

	msgpack_pack_str(packer, strlen(text) + 1);
	msgpack_pack_str_body(packer, text, strlen(text) + 1);
}

int main() {
	xRPC_Server_RegisterCallBack("helloWorld", HelloWorld);

	auto test = std::thread([]() {
		printf("RPC Listening\n");
		xRPC_Server_Start(2345, "127.0.0.1", 20);
		printf("RPC Stopped\n");
	});

	xRPC_Client_Start(2345, "127.0.0.1");

	while (xRPC_Server_GetStatus() != xRPC_SERVER_STATUS_ACTIVE) {}

	auto msgObject = msgpack_object();
	msgObject.type = MSGPACK_OBJECT_POSITIVE_INTEGER;

	intptr_t buffLocation[1];

	for (auto i = 0; i <= 10000; i++) {
		msgObject.via.u64 = i;
		auto val = xRPC_Client_Call("helloWorld", &msgObject, 10, buffLocation);

		if (val.type == MSGPACK_OBJECT_STR) {
			printf("Got text: %s\r", val.via.str.ptr);
			free((void*)buffLocation[0]);
		} else {
			printf("Did not get a response for %d\n", i);
			free((void*)buffLocation[0]);
			break;
		}
	}
	printf("\n");
	xRPC_Client_Stop();
	xRPC_Server_Stop();

	if (test.joinable()) {
		test.join();
	}

	xRPC_Server_ClearCallbacks();
}