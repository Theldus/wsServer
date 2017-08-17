## wsServer
wsServer - a very tiny WebSocket server library written in C

### Library
The library is made to be as simple as possible, so I don't follow to the letter the [RFC 6455](https://tools.ietf.org/html/rfc6455), the
only thing this library can do is send and receive text messages and treats them as events.

So it could not be helpful if you facing with a big application, but if you just want to send some messages between a non
serious application, help yourself, :-)

### Building
The process to build is very easy, just type ``make`` to build e ``make clean`` to clear your workspace. When the library
is compiled, a new file called libws.a will be generated, you just have to link this library across your main application.

### Why to complicate if things can be simple?
The wsServer abstracts the idea of sockets and you only need to deal with three types of events defined:

```c
/* New client. */
void onopen(int fd);

/* Client disconnected. */
void onclose(int fd);

/* Client sent a text message. */
void onmessage(int fd, unsigned char *msg);

/* fd is the File Descriptor returned by accepted connection. */
```
this is all you need to worry about, nothing to think about return values in socket, accepting connections, and so on.

As a gift, each client is treated in a separate thread, so you will not have to worry about it.
#### A complete example (file.c)
```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>

void onopen(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
	printf("Connection opened, client: %d | addr: %s\n", fd, cli);
	free(cli);
}

void onclose(int fd)
{
	char *cli;
	cli = ws_getaddress(fd);
	printf("Connection closed, client: %d | addr: %s\n", fd, cli);
	free(cli);
}

void onmessage(int fd, unsigned char *msg)
{
	char *cli;
	cli = ws_getaddress(fd);
	printf("I receive a message: %s, from: %s/%d\n", msg, cli, fd);

	sleep(2);
	ws_sendframe(fd, "hellow");
	sleep(2);
	ws_sendframe(fd, "wassup brow");
	
	free(cli);
	free(msg);
}

int main()
{
	struct ws_events evs;
	evs.onopen    = &onopen;
	evs.onclose   = &onclose;
	evs.onmessage = &onmessage;
	ws_socket(&evs, 8080);

	return 0;
}
 ```
 to build the example (assuming you already built the library), you just have to do 
 something like `gcc file.c -I include/ -o file -pthread libws.a`
 
 ----------------------------
 
 That's it, if you liked, found a bug or wanna contribute, let me know, ;-).
 
