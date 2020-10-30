# wsServer

wsServer - a very tiny WebSocket server library written in C

## Library

The library is made to be as simple as possible, so I don't follow to the letter the [RFC 6455](https://tools.ietf.org/html/rfc6455) and the
only thing this library can do (until now, contributions are welcome) is send and receive text messages and treats them as events.

So it could not be helpful if you facing with a big application, but if you just want to send some messages between a non
serious application, help yourself. :-)

## Building

The process to build is very easy, just type `make` to build and `make clean` to clear your workspace. When the library
is compiled, a new file called libws.a will be generated, you just have to link this library across your main application.

If you are using any IDE with CMake support, just open this project folder and your IDE will initialize the entire project. Alternatively,
you can build the project by command line using the following steps:

```bash
mkdir build && cd build/
cmake ..
make
./send_receive # Waiting for incoming connections...
```

## Why to complicate if things can be simple?

The wsServer abstracts the idea of sockets and you only need to deal with three types of events defined:

```c
/* New client. */
void onopen(int fd);

/* Client disconnected. */
void onclose(int fd);

/* Client sent a text message. */
void onmessage(int fd, const unsigned char *msg);

/* fd is the File Descriptor returned by accepted connection. */
```

this is all you need to worry about, nothing to think about return values in socket, accepting connections, and so on.

As a gift, each client is treated in a separate thread, so you will not have to worry about it.

### A complete example (file.c)

A more complete example, including the html file, can be found in example/ folder, ;-).

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>

/**
 * @brief This function is called whenever a new connection is opened.
 * @param fd The new client file descriptor.
 */
void onopen(int fd)
{
    char *cli;
    cli = ws_getaddress(fd);
    printf("Connection opened, client: %d | addr: %s\n", fd, cli);
    free(cli);
}

/**
 * @brief This function is called whenever a connection is closed.
 * @param fd The client file descriptor.
 */
void onclose(int fd)
{
    char *cli;
    cli = ws_getaddress(fd);
    printf("Connection closed, client: %d | addr: %s\n", fd, cli);
    free(cli);
}

/**
 * @brief Message events goes here.
 * @param fd Client file descriptor.
 * @param msg Message content.
 * @note For binary files, you can use base64, ;-).
 */
void onmessage(int fd, const unsigned char *msg)
{
    char *cli;
    cli = ws_getaddress(fd);
    printf("I receive a message: %s, from: %s/%d\n", msg, cli, fd);

    sleep(2);
    ws_sendframe(fd, "hello", -1, false);
    sleep(2);
    ws_sendframe(fd, "world", -1, false);

    free(cli);
}

int main()
{
    /* Register events. */
    struct ws_events evs;
    evs.onopen    = &onopen;
    evs.onclose   = &onclose;
    evs.onmessage = &onmessage;

    /* Main loop, this function never returns. */
    ws_socket(&evs, 8080);

    return (0);
}
```

to build the example above, just invoke: `make examples`.

## SSL/TLS Support
wsServer does not currently support encryption. However, it is possible to use it in conjunction
with [Stunnel](https://www.stunnel.org/), a proxy that adds TLS support to existing projects.
Just follow [these](doc/TLS.md) four easy steps to get TLS support on wsServer.

---

That's it. If you liked, found a bug, or wanna contribute, let me know ;-).
