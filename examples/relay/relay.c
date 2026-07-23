/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#define _GNU_SOURCE
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h> 
#include <ws.h>

#include "peers_lut.h"
#include "waitlist.h"
#include "json_pars.h"

#include <pthread.h>

/* A sync object to guard access to the peers LUT and the waitlist */
static pthread_mutex_t auth_sync;

static int term = 0;


static ws_cli_conn_t* check_auth(ws_cli_conn_t*, const unsigned char* , int* );

/**
 * @dir examples/
 * @brief wsServer examples folder
 */

/*
 * @dir examples/relay
 * @brief Relay example directory.
 * @file relay.c
 * @brief Simple relay example.
 */

/**
 * @brief Called when a client connects to the relay. It adds the
 * newly connected client to the authentication waitlist.
 *
 * @param client Client connection. The @p client parameter is used
 * in order to send messages and retrieve informations about the
 * client.
 */
void onopen(ws_cli_conn_t *client)
{
    char *cli, *port;
    cli  = ws_getaddress(client);
    port = ws_getport(client);
#ifndef DISABLE_VERBOSE
    printf("Connection opened, addr: %s, port: %s\n", cli, port);
#endif

    pthread_mutex_lock(&auth_sync);
    add_applier(client,2000);
    pthread_mutex_unlock(&auth_sync);
}

/**
 * @brief Called when a client disconnects from the relay.
 *
 * @param client Client connection. The @p client parameter is used
 * in order to send messages and retrieve informations about the
 * client.
 */
void onclose(ws_cli_conn_t *client)
{
    char *cli;
    cli = ws_getaddress(client);
#ifndef DISABLE_VERBOSE
    printf("Connection closed, addr: %s\n", cli);
#endif

    pthread_mutex_lock(&auth_sync);
    
    remove_client(client);
    delete_applier(client);
    
    pthread_mutex_unlock(&auth_sync);
}

/**
 * @brief Called when a client send a message to the relay.
 * If the client is authenticated and has a peer connected,
 * relay will proxy the message to the peer, otherwise the
 * message will be dropped. Pings and pongs are not proxied.
 *
 * @param client Client connection. The @p client parameter is used
 * in order to send messages and retrieve informations about the
 * client.
 *
 * @param msg Received message, this message can be any message.
 *
 * @param size Message size (in bytes).
 *
 * @param type Message type.
 */
void onmessage(ws_cli_conn_t *client,
               const unsigned char *msg, uint64_t size, int type)
{
    ws_cli_conn_t * peer = 0;
    int bad=0;

#ifndef DISABLE_VERBOSE	
    char *cli;
    cli = ws_getaddress(client);
    printf("I receive a message: %s (size: %" PRId64 ", type: %d), from: %s\n",
           msg, size, type, cli);
#endif

    peer = check_auth(client,msg,&bad);
    
    if(peer && WS_FR_OP_PING!=type && WS_FR_OP_PONG!=type)
        ws_sendframe(peer, (char *)msg, size, type);
    
    if(bad)
        ws_close_client(client);
    
}

static ws_cli_conn_t* check_auth(ws_cli_conn_t *client, const unsigned char *msg, int* err)
{
    ws_cli_conn_t * peer = 0;
    int bad=0;
    
    pthread_mutex_lock(&auth_sync);

    peer = get_peer(client);

    if(!peer) /* The client isn't paired */
    {
        if(!get_client_auth_status(client))/* There is no such authenticated client */
        {
            if(add_client(client,msg) < 0) /* The first message should be the client UUID */
            {
                bad = 1; /* We could not add the client: wrong UUID */
            }
        }

        delete_applier(client); /* Remove the client from the authenthication waitlist */
    }
    
    pthread_mutex_unlock(&auth_sync);
    *err = bad;
    
    return peer;
}

static void endsignal(int sig)
{
    if(SIGINT == sig)
        term = 1;
}

static void close_all()
{
    pthread_mutex_lock(&auth_sync);

    foreach(ws_close_client);

    pthread_mutex_unlock(&auth_sync);
}

static void check_belated()
{
    ws_cli_conn_t* clnt=0;
    
    pthread_mutex_lock(&auth_sync);

    clnt = remove_belated();

    if(clnt)
        remove_client(clnt);

    pthread_mutex_unlock(&auth_sync);

    if(clnt)
        ws_close_client(clnt);
}


/**
 * @brief Main routine.
 *
 * @note After invoking @ref ws_socket, this routine never returns,
 * unless if invoked from a different thread.
 */
int main(void)
{
    char* jsn = alloc_peer_buff("./peers.json");
    pthread_mutex_init(&auth_sync,0);
    

    if(0==jsn || 0>=get_pairs(jsn,add_pair))
    {
        printf("The peer file is absent or corrupt\n");
        goto end;
    }
    
    signal( SIGINT , endsignal );

    ws_socket(&(struct ws_server){
                  /*
                  * Bind host:
                  * localhost -> localhost/127.0.0.1
                  * 0.0.0.0   -> global IPv4
                  * ::        -> global IPv4+IPv6 (DualStack)
                  */
                  .host = "0.0.0.0",
                  .port = 8080,
                  .thread_loop   = 1,
                  .timeout_ms    = 1000,
                  .evs.onopen    = &onopen,
                  .evs.onclose   = &onclose,
                  .evs.onmessage = &onmessage
              });

    while (!term)
    {
        check_belated();
        usleep(5000);
    }

    close_all();
    usleep(500000);

end:
    if(jsn)
        free(jsn);

    pthread_mutex_destroy(&auth_sync);
    return (0);
}
