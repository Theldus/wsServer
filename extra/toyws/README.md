# ToyWS
Since there is some demand to support a client, 'ToyWS' is a response to those
requests: ToyWS is a toy WebSocket client, meaning that it's quite simple and
made to work (guaranteed) only with wsServer.

Limitations:
 - Fixed handshake header
 - Fixed frame mask (it should be random)
 - No PING/PONG frame support
 - No close handshake support: although it can identify CLOSE frames, it
   does not send the response, only aborts the connection.
 - No support for CONT frames, that is, the entire content of a frame (TXT
   or BIN) must be contained within a single frame.
 - Possibly other things too.

Although extremely limited, ToyWS was designed for those who want to _also_
have a C client that is lightweight and compatible with wsServer, thus,
freeing the need for a browser and/or third-party libraries to test and use
wsServer.

Maybe this client will evolve into something more complete and general in the
future, but that's not in the roadmap at the moment.

## API
The API is quite simple and is summarized in 4 routines, to connect,
disconnect, send and receive frame, as follows:

```c
int tws_connect(struct tws_ctx *ctx, const char *ip, uint16_t port);
```
Connect to a given `ip` address and `port`.

**Return**:
Returns a positive number if success, otherwise, a negative number.

**Note:**
`struct tws_ctx *ctx` is for internal usage and initialized within this
function. There is no need to access this structure or modify its values, ToyWS
just needs it to maintain the consistent client state.

---

```c
void tws_close(struct tws_ctx *ctx);
```
Close the connection for the given `ctx`.

---

```c
int tws_sendframe(struct tws_ctx *ctx, uint8_t *msg, uint64_t size, int type);
```
Send a frame of type `type` with content `msg` and size `size` for a given
context `ctx`.

Valid frame types are:
- FRM_TXT
- FRM_BIN

**Return**:
Returns 0 if success, otherwise, a negative number.

---

```c
int tws_receiveframe(struct tws_ctx *ctx, char **buff, size_t *buff_size,
    int *frm_type);
```
Receive a frame and save it on `buff`.

**Parameters:**

**`buff`:**

Pointer to the target buffer. If NULL, ToyWS will allocate a new buffer that is
capable to hold the frame and save into `buff`.

If already exists: the function will try to fill the buffer with the frame
content, if the frame size is bigger than `buff_size`, the function will
reallocate `buff` and update `buff_size` with the new size.

**`buff_size`:**

Current buffer size. __Must__ point the a valid memory region. If `*buff`
points to NULL, `*buff_size` must be equals to 0.

**`frm_type`:**

Frame type read. The frame type received will be reflected into the contents of
this pointer.

**Return**: Returns 0 if success, a negative number otherwise.

**Note**:

- This routine is blocking, that is, it will only return if it manages to read
a frame or if there is an error during the reading (such as invalid (or
unsupported) frame or server disconnection).

- At the end of everything, don't forget to free the buffer!. Once its size is
relocated, a single call to 'free' is sufficient.

## Example
The example below illustrates the usage (also available at (extra/toyws/tws_test.c)):
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "toyws.h"

int main(void)
{
    struct tws_ctx ctx;
    char msg[] = "Hello";

    /* Buffer/frame params. */
    char *buff;
    int frm_type;
    size_t buff_size;

    buff      = NULL;
    buff_size = 0;
    frm_type  = 0;

    if (tws_connect(&ctx, "127.0.0.1", 8080) < 0)
        fprintf(stderr, "Unable to connect!\n");

    /* Send message. */
    printf("Send: %s\n",
        (tws_sendframe(&ctx, msg, strlen(msg), FRM_TXT) >= 0 ?
            "Success" : "Failed"));

    /* Blocks until receive a single message. */
    if (tws_receiveframe(&ctx, &buff, &buff_size, &frm_type) < 0)
        fprintf(stderr, "Unable to receive message!\n");

    printf("I received: (%s) (type: %s)\n", buff,
        (frm_type == FRM_TXT ? "Text" : "Binary"));

    tws_close(&ctx);

    free(buff);
    return (0);
}
```
