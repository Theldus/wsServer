# Autobahn and standard support
Although wsServer strives to be as simple as possible, the library does intend to used
in production and relies on two (but not limited to) tools for that: [AFL](doc/FUZZING.md)
and Autobahn WebSocket Testsuite. The former ensures that wsServer is stable enough
to be run even under unexpected conditions, such as when under attack. The latter
is discussed below.

[Autobahn|Testsuite](https://github.com/crossbario/autobahn-testsuite) is a third-party
tool that performs a series of automated tests that verify the correctness of client and
server websocket implementations in conformance to the specification. With more than 500
test cases, Autobahn extensively tests client and server implementations and also evaluates
its performance.

## Tests results
In order to see how well wsServer performs, the library was tested with Autobahn v0.10.9
and the results can be seen [here](https://theldus.github.io/wsServer/autobahn).

From the tests, wsServer **do not** pass:
- 6.3.1 to 6.3.2
- 6.4.1 to 6.4.4
- 6.6.1, 6.6.3, 6.6.4, 6.6.6 (ouch), 6.6.8 and 6.6.10
- 6.8.1, 6.8.2
- 6.10.1 to 6.10.3
- 6.11.5
- 6.12.1 to 6.21.8
- 7.5.1

Although it seems to be quite a lot, all of them have the very same thing in common: UTF-8
invalid sequences validation. The specs state that if an endpoint finds an invalid UTF-8
text sequence, it should drop the connection; wsServer in turn, does not, and this was
intentional.

wsServer intends to simple to use and to code, so I do not want to add an extra layer
of checks in the code that just check if all UTF-8 payloads are valid or not. If this is
a concern for your use-case, you can check if the message parameter inside the `on_message`
event is a valid UTF-8 string or not; you can check that by using some library, such as
[utf8.h](https://github.com/sheredom/utf8.h).

Please note that wsServer is **fully** capable to send/receive UTF-8 messages it just
does not verify if they are valid or not.

## Conclusion
wsServer is __almost__ RFC compliant and supports most of the desirable (and required) features
in order to talk to the vast majority of clients out there. Furthermore, wsServer is
lightweight, easy-to-use, and fast.
