# wsServer

wsServer - a very tiny WebSocket server library written in C

## Library

wsServer is a tiny, lightweight WebSocket server library written in C that intends
to be easy to use, fast, hackable, and [almost](doc/AUTOBAHN.md) compliant to the
[RFC 6455](https://tools.ietf.org/html/rfc6455).

The main features are:
- Send/Receive Text and Binary messages
- PING/PONG frames
- Opening/Closing handshakes

See Autobahn [report](https://theldus.github.io/wsServer/autobahn) and the
[docs](doc/AUTOBAHN.md) for an 'in-depth' analysis.

## Building

wsServer only requires a C99-compatible compiler, and optionally Doxygen to build
the docs. The build process comes in two flavors: pure makefile or CMake.

### Make
The preferred way to build wsServer on Linux environments. This also automatically
builds an example file or a ready-to-use fuzzy version (see [here](doc/FUZZING.md) for
details):
```bash
git clone https://github.com/Theldus/wsServer
cd wsServer/
make

# Optionally, a user can also install wsServer into the system,
# either on default paths or by providing PATH or DESTDIR env
# vars to the makefile.

make install # Or make install DESTDIR=/my/folder/
```

### CMake
CMake enables the user to easily build wsServer in others environments other than Linux
and also allows the use of an IDE to build the project automatically. If that's
your case:
```bash
git clone https://github.com/Theldus/wsServer
cd wsServer/
mkdir build && cd build/
cmake ..
make
./send_receive # Waiting for incoming connections...
```

in both cases, a statically lib (libws.a) will be generated.

## Why to complicate if things can be simple?

wsServer abstracts the idea of sockets and you only need to deal with three
types of events defined:

```c
/* New client. */
void onopen(int fd);

/* Client disconnected. */
void onclose(int fd);

/* Client sent a text message. */
void onmessage(int fd, const unsigned char *msg, size_t size, int type);

/* fd is the File Descriptor returned by accepted connection. */
```

this is all you need to worry about, nothing to think about return values in socket,
accepting connections, and so on.

As a gift, each client is handled in a separate thread, so you will not have to
worry about it.

### A complete example (file.c)

A more complete example, including the html file, can be found in example/
folder, ;-).

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
 * @param fd   Client file descriptor.
 * @param msg  Message content.
 * @param size Message size.
 * @param type Message type.
 */
void onmessage(int fd, const unsigned char *msg, size_t size, int type)
{
    char *cli;
    cli = ws_getaddress(fd);
    printf("I receive a message: %s (%zu), from: %s/%d\n", msg,
        size, cli, fd);

    sleep(2);
    ws_sendframe_txt(fd, "hello", false);
    sleep(2);
    ws_sendframe_txt(fd, "world", false);

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
wsServer does not currently support encryption. However, it is possible to use it
in conjunction with [Stunnel](https://www.stunnel.org/), a proxy that adds TLS
support to existing projects. Just follow [these](doc/TLS.md) four easy steps
to get TLS support on wsServer.

## Contributing
wsServer is always open to the community and willing to accept contributions,
whether with issues, documentation, testing, new features, bugfixes, typos...
welcome aboard. Make sure to read the [coding-style](doc/CODING_STYLE.md)
guidelines before sending a PR.

## License and Authors
wsServer is licensed under GPLv3 License. Written by Davidson Francis and
[others](https://github.com/Theldus/wsServer/graphs/contributors)
contributors.
