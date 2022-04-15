# wsServer examples
This directory contains usage examples and demos for many different scenarios
that might happen when using wsServer.

**The examples:**

- [echo](echo): A simple echo server that broadcasts incoming messages
to all connected clients.

- [vtouchpad](vtouchpad): A 'virtual touchpad' that remotely controls
a computer's mouse.

If you have other examples and/or small demos that might be useful for
illustrating wsServer's functionality, feel free to submit a PR.

## Building
A typical build with `make` or `cmake` should be able to compile all examples
by default. However, note that some examples/demos may be platform specific
(either Linux, Windows...), so check the specific README for each example in
their respective folders for more information.
