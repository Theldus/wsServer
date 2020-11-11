# Fuzzing wsServer
wsServer intends to be robust enough to be used safely in production. Thus,
the project supports fuzzing through the `ws_file` routine, which reads a file
containing previously captured packets from the network. This routine allows
wsServer to be tested for common cases and expected to work and permits it to
be used on fuzzers, such as the AFL, supported here.

## 1) Installing/building American Fuzzy Lop
While not the focus here, building AFL should not be an issue, so the
following brief instructions should be sufficient:

```bash
# Clone the repository
git clone https://github.com/google/AFL

# Build
cd AFL/
make

# Set env vars
export PATH=$PATH:$(pwd)
export AFL_PATH=$(pwd)

# Add env vars into your ~/.bashrc
echo "export PATH=\$PATH:$(pwd)/" >> ~/.bashrc
echo "AFL_PATH=$(pwd)" >> ~/.bashrc
```
If anything fails, please check if you have the common build tools on your
system (such as `gcc`, `make`, etc.) and read the official or specific
instructions for your system.

## 2) Fuzzing
Once AFL is up and running, fuzzing is pretty straightforward:

```bash
# Make sure wsServer is in a clean state
make clean

# Build with AFL_FUZZ var set to yes:
AFL_FUZZ=yes make
```

wsServer and the test file will be compiled. Fuzzing starts automatically
right after.

---

## Input tests and file structures
All fuzzing-related parts are present in the _tests_ folder and follow the
following structure:

```text
├── in
│   ├── ch_1b_1b_508b_close
│   └── ch_1b_close
│
├── out
│
├── packets
│   ├── ch_508b_close
│   ├── ch_close
│   ├── ff_1b_close
│   ├── ff_384kB_close
│   ├── ff_ping_ping_close
│   ├── ws_1b_close
│   ├── ws_508b_ping_close
│   ├── ws_98305b_close
│   │
│   ├── frames
│   │   ├── close
│   │   ├── ping
│   │   ├── req_chrome
│   │   ├── req_firefox
│   │   └── req_websocat
│   │
│   └── msgs
│       ├── msg_1byte
│       ├── msg_384kB_cont
│       ├── msg_508bytes
│       └── msg_98305bytes
│
├── Makefile
├── run-fuzzy.sh
└── ws_file.c
```

- **in/:**
Contains the input files that will be used in AFL (parameter `-i`).

- **out/:**
Contains the AFL output (parameter `-o`). Note that the execution script
(`run-fuzzy.sh`) allows you to customize the output by the environment variable
`AFL_OUT`.

- **packets/:**
Contains packets and parts of packets captured from the network from multiple
clients (currently: Firefox, Google Chrome, and Websocat) with wsServer. It
serves as a way to 'assemble' new test files for wsServer, whether for fuzzing
or not.

  - **packets/frames:/**
Contains request frames (handshake) and control frames from multiple clients.

  - **packets/msgs/:**
Contains packets of messages sent to wsServer of varying sizes, with FRAMES of
type `FIN` and `CONT`.

### Creating new inputs
New input files are pretty simple to make: either you create from existing
packets or capture new ones via tcpdump, wireshark, etc.

Let's say you want to create one that uses a Firefox handshake, sends two messages
of one byte each, one ping and disconnect, we can then do:
```bash
cd tests/packets
cat frames/req_firefox            \
	msgs/msg_1byte msgs/msg_1byte \
	frames/ping                   \
	frames/close > ../in/ff_1b_1b_ping_close
```

For new packets, the idea is similar.

**Attention:** Since inputs need to be valid, when creating new packets, be
sure to always use a handshake (req_*) as the first file and a close frame
at the end.

---

## Acknowledgments
Thanks to [@rlaneth](https://github.com/rlaneth), who performed fuzzing
tests on wsServer and who discovered and helped me to fix many bugs in the
source code.
