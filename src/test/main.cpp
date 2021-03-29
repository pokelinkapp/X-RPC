#include "X-RPC/x_RPC.h"
#include <thread>
#include <unistd.h>

msgpack_object* HelloWorld(msgpack_object_array* args) {
	return nullptr;
}

int main() {
	xRPC_Server_RegisterCallBack("helloWorld", HelloWorld);
	xRPC_Server_RegisterCallBack("helloWorld2", HelloWorld);

	auto test = std::thread([]() {
		printf("RPC Listening\n");
		xRPC_Server_Start(2345, "127.0.0.1", 20);
		printf("RPC Stopped\n");
	});

	sleep(1);

	xRPC_Server_Stop();

	if (test.joinable()) {
		test.join();
	}

	xRPC_Server_ClearCallbacks();
}