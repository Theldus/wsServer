## SSL/TLS Support
wsServer does not currently support encryption. However, it is possible to use it in conjunction
with [Stunnel](https://www.stunnel.org/), a proxy that adds TLS support to existing projects.
Just follow these four easy steps below to get TLS support on wsServer.

### 1) Installing Stunnel

#### Ubuntu
```bash
$ sudo apt install stunnel
```

#### Other distros
```bash
    $ sudo something
```

### 2) Generating certificates/keys
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

### 3) Stunnel configuration file

Stunnel works by creating a proxy server on a given port that connects to the
original server on another, so we need to teach how it will talk to wsServer:

Create a file /etc/stunnel/stunnel.conf with the following content:

```text
[wsServer]
cert = /etc/stunnel/server.pem
accept = 0.0.0.0:443
connect = 127.0.0.1:8080
```

### 4) Launch Stunnel and wsServer

```bash
$ sudo stunnel /etc/stunnel/stunnel.conf
$ ./your_program_that_uses_wsServer
```

(Many thanks to [@rlaneth](https://github.com/rlaneth) for letting me know of this tool).
