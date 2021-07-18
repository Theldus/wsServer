# Autobahn and standard support
Although wsServer strives to be as simple as possible, the library does intend to used
in production and relies on two (but not limited to) tools for that: [AFL](FUZZING.md)
and Autobahn WebSocket Testsuite. The former ensures that wsServer is stable enough
to be run even under unexpected conditions, such as when under attack. The latter
is discussed below.

[Autobahn|Testsuite](https://github.com/crossbario/autobahn-testsuite) is a third-party
tool that performs a series of automated tests that verify the correctness of client and
server websocket implementations in conformance to the specification. With more than 500
test cases, Autobahn extensively tests client and server implementations and also evaluates
its performance.

## Run tests
Testing requires pre-installation of
[Autobahn|Testsuite](https://github.com/crossbario/autobahn-testsuite). Alternatively, it is
possible to use the Docker image used in the CI tests, in this case, export the environment
variable 'TRAVIS', as `export TRAVIS=true`.

After that, tests can be invoked via Makefile or CMake:
### Makefile
```bash
# Ensure project is in clean state
$ make clean
# Build and execute tests
$ make tests
```
### CMake
```bash
$ mkdir build && cd build/
# Configure project and enable tests build
$ cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_WSSERVER_TEST=On
# Build project
$ make -j4
# Execute tests
$ make test
```

## Tests results
From the [tests](https://theldus.github.io/wsServer/autobahn), it can be seen that
wsServer passes all Autobahn|Testsuite tests. The only tests that are not run (12.*
and 13.*) concern WebSocket Compression, which is defined as an extension of the
websocket protocol and defined in
[RFC 7692](https://datatracker.ietf.org/doc/html/rfc7692). Compression does not
belong to [RFC 6455](https://tools.ietf.org/html/rfc6455).

Therefore, I believe it is safe to say that wsServer is RFC 6455 compliant and should
behave correctly in different scenarios. Any unexpected behavior regarding communication
with the client is considered an error, and an issue must be reported.
