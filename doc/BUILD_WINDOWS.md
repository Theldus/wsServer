# Building on Windows

## Introduction
Unlike Linux, Windows does not have by default a package manager or native build
tools, which makes the process a little more laborious, but the sections below
clarify in detail what must be done to natively build the library.

## MSYS2 Setup

MSYS2 is a set of tools and libraries that provide an easy-to-use development
environment and is, in my opinion, the most straightforward way to use C in
Windows environments.

The [home page](https://www.msys2.org/) already illustrates the basic
step-by-step for the initial setup of MSYS2 but below is a very brief
description of what to do:

(note: if you already have MinGW and CMake properly installed, you can skip this
section entirely)

### 1) Download and install MSYS2
From the homepage (informed above) download the latest version available, or if
you prefer, use the same version tested here:
[msys2-x86_64-20210604.exe](https://github.com/msys2/msys2-installer/releases/download/2021-06-04/msys2-x86_64-20210604.exe).

### 2) Update packages
MSYS2 uses Pacman, a package manager, that is, a program responsible for
managing programs, libraries, and their dependencies installed on a machine.
Very well known among Arch Linux users, it is very useful to have it on MSYS2.

1) First, it is necessary to update the package database and base packages. If
it hasn't already opened, run 'MSYS2 MSYS' from the Start menu and run
`pacman -Syu`.

2) To proceed with the update, the terminal needs to be closed and opened again.
After that, continue the update with: `pacman -Su`.

### 3) Install packages
In addition to the base packages, some packages need to be installed to have a
minimal development environment:

1) Basic packages and GCC toolchain:
`pacman -S --needed base-devel mingw-w64-x86_64-toolchain`.
2) CMake and Git: `pacman -S mingw-w64-x86_64-cmake git`.

## wsServer build
With MSYS2 up and running, you have everything you need to download, compile and
run the wsServer and its examples.

To do this, run 'MSYS2 MinGW 64bit' from the Start menu and:
```bash
$ git clone https://github.com/Theldus/wsServer.git
$ cd wsServer
$ mkdir build && cd build/
$ cmake .. -DCMAKE_BUILD_TYPE=Release -G "MinGW Makefiles"
$ mingw32-make -j4
```
And that's it, the static library (libws.a) and the examples files are located in the 
build/examples folder.

The `echo` example can be run with: `./examples/echo/echo.exe`, and the test webpage can be
found at `wsServer/examples/echo`.
