#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <ws.h>

#include "waitlist.h"

#define ADD_APPLIER         1
#define REMOVE_BELATED      2
#define CONFIRM_APPLIER     3
#define MAX_APPLIERS        MAX_CLIENTS 

struct applier
{
    ws_cli_conn_t    *clnt;
    uint32_t         deadline;
};

uint32_t get_ticks()
{
    struct timeval tv;
    gettimeofday(&tv,0);

    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

static int waitlist(struct applier* app, int todo)
{
    static struct applier ht[MAX_APPLIERS];
    int ret = -1;
    uint32_t tmp = get_ticks();
    
    switch(todo)
    {
    case ADD_APPLIER:

        for(int i=0;i<MAX_APPLIERS;i++)
        {			
            if(0==ht[i].clnt)
            {
                ht[i] = *app;
                ret = i;
                break;
            }
        }

        break;

    case REMOVE_BELATED:
    
        for(int i=0;i<MAX_APPLIERS;i++)
        {			
            if(ht[i].deadline<=tmp)
            {
                *app = ht[i];
                ht[i].clnt = 0;
                ht[i].deadline = 0;
                ret = i;
                break;
            }
        }

        break;
        
    case CONFIRM_APPLIER:
    
        for(int i=0;i<MAX_APPLIERS;i++)
        {			
            if(app->clnt==ht[i].clnt)
            {
                ht[i].clnt = 0;
                ht[i].deadline = 0;
                ret = i;
                break;
            }
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
    struct applier appl;
    
    if(0<waitlist(&appl,REMOVE_BELATED))
        return appl.clnt;

    return 0;
}

int confirm_applier(ws_cli_conn_t* cl)
{
    struct applier appl;
    
    appl.clnt = cl;

    return waitlist(&appl,CONFIRM_APPLIER);
}
