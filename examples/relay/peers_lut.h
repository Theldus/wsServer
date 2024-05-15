#ifndef PEERS_LUT_H__
#define PEERS_LUT_H__

typedef int (*clnthndl)(ws_cli_conn_t* cl);

int add_pair(const char* provider, const char* user);

int add_client(ws_cli_conn_t* cl, const unsigned char * uuid);

ws_cli_conn_t*  get_peer(ws_cli_conn_t* cl);

int lut_dump();

int  get_client_auth_status(ws_cli_conn_t* cl);

int  known_uuid(char* id);

void remove_client(ws_cli_conn_t* cl);

int foreach(clnthndl todo);

#endif
