#include "X-RPC/x_RPC.h"
#include <thread>
#include <string>

void HelloWorld(msgpack_object* args, msgpack_packer* packer) {
	auto text = "Hello, World!";

	msgpack_pack_str(packer, strlen(text) + 1);
	msgpack_pack_str_body(packer, text, strlen(text) + 1);
}

void HelloWorld2(msgpack_object* args, msgpack_packer* packer) {
	auto text = "Hello, World!2";

	msgpack_pack_str(packer, strlen(text) + 1);
	msgpack_pack_str_body(packer, text, strlen(text) + 1);
}

int main() {
	xRPC_Server_RegisterCallBack("helloWorld", HelloWorld);
	xRPC_Server_RegisterCallBack("helloWorld2", HelloWorld2);

	auto test = std::thread([]() {
		printf("RPC Listening\n");
		xRPC_Server_Start(2345, "127.0.0.1", 20);
		printf("RPC Stopped\n");
	});

	xRPC_Client_Start(2345, "127.0.0.1");

	auto val = xRPC_Client_Call("helloWorld", nullptr, 10);

	if (val.type == MSGPACK_OBJECT_STR) {
		printf("Got text: %s\n", val.via.str.ptr);
	}

	val = xRPC_Client_Call("helloWorld2", nullptr, 10);

	if (val.type == MSGPACK_OBJECT_STR) {
		printf("Got text: %s\n", val.via.str.ptr);
	}

	xRPC_Client_Stop();
	xRPC_Server_Stop();

	if (test.joinable()) {
		test.join();
	}

	xRPC_Server_ClearCallbacks();
}