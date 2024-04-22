#ifndef PEERS_LUT_H__
#define PEERS_LUT_H__

int add_pair(const char* provider, const char* user);

int add_client(ws_cli_conn_t* cl, const char* guid);

ws_cli_conn_t*  get_peer(ws_cli_conn_t* cl);

int lut_dump();


#endif
