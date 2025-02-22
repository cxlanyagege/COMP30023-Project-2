CC=cc
RPC_SYSTEM=rpc.o
SERVER=server.a
CLIENT=client.a
EXECUTABLES=rpc-server rpc-client

.PHONY: format all

all: $(RPC_SYSTEM)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) -Wall -c -o $@ $<

rpc-server: $(SERVER) $(RPC_SYSTEM)
	$(CC) -o $@ $^

rpc-client: $(CLIENT) $(RPC_SYSTEM)
	$(CC) -o $@ $^

rpc-test: $(EXECUTABLES)

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f $(RPC_SYSTEM) $(EXECUTABLES)