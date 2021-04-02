#include "X-RPC/x_RPC.h"
#include <thread>

void HelloWorld(msgpack_object* args, msgpack_packer* packer) {
	auto text = "Hello, World!";

	msgpack_pack_str(packer, strlen(text));
	msgpack_pack_str_body(packer, text, strlen(text));
}

void HelloWorld2(msgpack_object* args, msgpack_packer* packer) {
	auto text = "Hello, World!2";

	msgpack_pack_str(packer, strlen(text));
	msgpack_pack_str_body(packer, text, strlen(text));
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
		auto str = std::string(val.via.str.ptr, val.via.str.size);
		printf("Got text: %s\n", str.c_str());
	}

	val = xRPC_Client_Call("helloWorld2", nullptr, 10);

	if (val.type == MSGPACK_OBJECT_STR) {
		auto str = std::string(val.via.str.ptr, val.via.str.size);
		printf("Got text: %s\n", str.c_str());
	}

	xRPC_Client_Stop();
	xRPC_Server_Stop();

	if (test.joinable()) {
		test.join();
	}

	xRPC_Server_ClearCallbacks();
}