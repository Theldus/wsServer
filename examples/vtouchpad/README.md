# vtouchpad
This example implements a simple 'virtual touchpad' that allows a web client to control a remote computer's mouse via wsServer/websocket.

## Features
<p align="center">
<img align="center" src="https://i.imgur.com/sOTqwWI.png" alt="vtouchpad example">
<br>
vtouchpad example
</p>

The larger area represents the main touchpad, move the cursor over it to move the mouse. Left and right clicks within this area also work as expected.

The two smaller buttons represent the left and right buttons respectively.

## Building
Since mouse movement is OS-dependent, this example works on a limited number of systems, currently Linux, FreeBSD and Windows.

**Linux and FreeBSD builds:**

vtouchpad requires the environment to run under X11, and requires libX11, libXtst or libxdo (comes with xdotool).

A build for Ubuntu and the like:
```bash
$ sudo apt install libxdo-dev
$ cd wsServer/
$ mkdir build && cd build/
$ cmake ..
$ make
$ ./examples/vtouchpad/vtouchpad
```
FreeBSD (although xdotool is available, the package is currently without maintainer, so its use is discouraged; a normal build already works as expected):
```bash
$ cd wsServer/
$ mkdir build && cd build/
$ cmake ..
$ make
$ ./examples/vtouchpad/vtouchpad
```

**Windows builds:**

It does not require any special libraries, and should work on any version above Windows 2000.

```bash
$ cd wsServer/
$ mkdir build && cd build/
$ cmake .. -G "MinGW Makefiles"
$ mingw32-make
$ ./examples/vtouchpad/vtouchpad.exe
```
