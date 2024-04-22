#ifndef WAITLIST_H__
#define WAITLIST_H__

int add_applier(ws_cli_conn_t* cl, uint32_t tmout_ms);

ws_cli_conn_t* remove_belated();

int confirm_applier(ws_cli_conn_t* cl);

#endif
