#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <ws.h>

#include "waitlist.h"

#define ADD_APPLIER         1
#define REMOVE_BELATED      2
#define DELETE_APPLIER      3
#define MAX_APPLIERS        MAX_CLIENTS 

struct applier
{
    ws_cli_conn_t    *clnt;
    uint32_t         deadline;
};

struct buff_state
{
    int* belated;
    int* empty;
    struct applier* bfr;
    size_t bf_sz;
    ws_cli_conn_t  *clnt;
};

uint32_t get_ticks()
{
    struct timeval tv;
    gettimeofday(&tv,0);

    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

static int get_buffer_state(struct buff_state* bst)
{
    int ret = -1;
    *bst->belated = -1;
    *bst->empty = -1;
    
    uint32_t tmp = get_ticks();

    for(size_t i=0;i<bst->bf_sz;i++)
    {
        if(bst->clnt && bst->clnt==bst->bfr[i].clnt)
        {
            ret = i;
        }

        if(bst->bfr[i].deadline<=tmp)
        {
            *bst->belated = i;
        }

        if(!bst->bfr[i].clnt)
        {
            *bst->empty = i;
        }
    }

    return ret;
}


static int waitlist(struct applier* app, int todo)
{
    static struct applier ht[MAX_APPLIERS];
    static int belated = -1;
    static int empty = -1;
    int ret = -1;
    
    struct buff_state bs={&belated,&empty,ht,MAX_APPLIERS,app->clnt};

    switch(todo)
    {
    case ADD_APPLIER:

        if(empty<0)
            get_buffer_state(&bs);

        if(empty>=0)
        {
            ret = empty;
            ht[empty] = *app;
        }
        empty = -1;

        break;

    case REMOVE_BELATED:

        if(belated<0)
            get_buffer_state(&bs);

        if(belated>=0)
        {
            ret = belated;
            *app = ht[belated];
            ht[belated].clnt = 0;
            ht[belated].deadline = 0;
        }

        belated = -1;

        break;
        
    case DELETE_APPLIER:
        
        ret = get_buffer_state(&bs);
        
        if(ret>=0)
        {
            ht[ret].clnt = 0;
            ht[ret].deadline = 0;
            break;
        }
       
        break;    
    
    default:break;
    }

    return ret;
}

int add_applier(ws_cli_conn_t* cl, uint32_t tmout_ms)
{
    struct applier appl;
    
    appl.deadline = get_ticks() + tmout_ms;
    appl.clnt = cl;

    return waitlist(&appl,ADD_APPLIER);
}

ws_cli_conn_t* remove_belated()
{
    struct applier appl={0,0};
    
    if(waitlist(&appl,REMOVE_BELATED)>=0)
        return appl.clnt;

    return 0;
}

int delete_applier(ws_cli_conn_t* cl)
{
    struct applier appl;
    
    appl.clnt = cl;

    return waitlist(&appl,DELETE_APPLIER);
}
