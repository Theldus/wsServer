#ifndef JSON_PARS_H__
#define JSON_PARS_H__

typedef int (*pairs)(const char* provider, const char* user);

char* alloc_peer_buff(const char* peer_file);
int  get_pairs(char* peer_bfr, pairs fncptr);

#endif
