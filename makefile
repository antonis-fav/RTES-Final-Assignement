all: client

client: client.c
	gcc -pthread client.c -I /usr/local/include -l websockets -L /user/local/lib -o client

clean:
	rm -f client
	rm -rf client.dSYM
