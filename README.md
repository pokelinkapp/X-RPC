# X-RPC (Cross-RPC)

## About

Cross-RPC is a basic C RPC library using msgpack to handle data packets.

It is aimed at being easy to create a server and connect to the server instance

Example Server:

```c
char text[30];

void HelloWorld(msgpack_object* args, msgpack_packer* packer) {
    xRPC_Server_Stop();
    if (args->type != MSGPACK_OBJECT_POSITIVE_INTEGER) {
        return;
    }

    sprintf(text, "Hello World %lu", (unsigned long)args->via.u64);

    msgpack_pack_str(packer, strlen(text) + 1);
    msgpack_pack_str_body(packer, text, strlen(text) + 1);
}

int main() {
    xRPC_Server_RegisterCallBack("helloWorld", HelloWorld);
	
    xRPC_Server_Start(2345, "127.0.0.1", 20);
	
    xRPC_Server_ClearCallbacks()
}
```

Example client:

```c
int main() {
    xRPC_Client_Start(2345, "127.0.0.1");
	
    auto msgObject = msgpack_object();
	
    msgObject.type = MSGPACK_OBJECT_POSITIVE_INTEGER;
	
    msgObject.via.u64 = 200;
    msgpack_object val = xRPC_Client_Call("helloWorld", &msgobject, 10);
	
    if (val.data.type == MSGPACK_OBJECT_STR) {
        printf("Got text: %s\r", val.data.via.str.ptr);
        xRPC_Destroy_Package(&val);
    } else {
        printf("Did not get a response for %d\n", i);
        xRPC_Destroy_Package(&val);
    }
	
    xRPC_Client_Stop();
}
```
