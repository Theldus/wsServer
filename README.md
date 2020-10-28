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
    ws_sendframe(fd, "hellow", -1, false);
    sleep(2);
    ws_sendframe(fd, "wassup brow", -1, false);

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

to build the example (assuming you already built the library), you just have to do
something like `gcc file.c -I include/ -o file -pthread libws.a`

## SSL/TLS Support
wsServer does not currently support encryption. However, it is possible to use it in conjunction
with [Stunnel](https://www.stunnel.org/), a proxy that adds TLS support to existing projects.
Just follow these four easy steps below to get TLS support on wsServer.

<details><summary>1) Installing Stunnel</summary>

#### Ubuntu
```bash
$ sudo apt install stunnel
```

#### Other distros
    $ sudo something

</details>

<details><summary>2) Generating certificates/keys</summary>

<br>

After you have Stunnel installed, generate your CA, private key and copy
to the Stunnel configure folder (usually /etc/stunnel/, but could be anywhere):

```bash
# Private key
$ openssl genrsa -out server.key 2048

# Certificate Signing Request (CSR)
$ openssl req -new -key server.key -out server.csr

# Certificate
$ openssl x509 -req -days 1024 -in server.csr -signkey server.key -out server.crt

# Append private key, certificate and copy to the right place
$ cat server.key server.crt > server.pem 
$ sudo cp server.pem /etc/stunnel/
```

Observations regarding localhost: 
1) If you want to run on localhost, the 'Common Name' field (on CSR, 2nd command) _must_
be 'localhost' (without quotes).

2) Make sure to add your .crt file to your browser's Certificate Authorities before trying
to use wsServer with TLS.

3) Google Chrome does not like localhost SSL/TLS traffic, so you need to enable
it first, go to `chrome://flags/#allow-insecure-localhost` and enable this option.
Firefox looks ok as long as you follow 2).

</details>

<details><summary>3) Stunnel configuration file</summary>

<br>

Stunnel works by creating a proxy server on a given port that connects to the
original server on another, so we need to teach how it will talk to wsServer:

Create a file /etc/stunnel/stunnel.conf with the following content:

```text
[wsServer]
cert = /etc/stunnel/server.pem
accept = 0.0.0.0:443
connect = 127.0.0.1:8080
```
</details>

<details><summary>4) Launch Stunnel and wsServer</summary>

<br>

```bash
$ sudo stunnel /etc/stunnel/stunnel.conf
$ ./your_program_that_uses_wsServer
```
</details>

(Many thanks to [@rlaneth](https://github.com/rlaneth) for letting me know of this tool).

---

That's it. If you liked, found a bug, or wanna contribute, let me know ;-).
