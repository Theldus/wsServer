#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>
#include "peers_lut.h"

/* 
 * This is a bastardised version of the hashtable used to store crosslinks
 * between clients connected through the relay, with client pointers playing
 * the role of hashes. The crosslink is an object which references two
 * connected clients by their authentication uuids, passed to the relay as
 * the very first message after the client connects. The table uses binary
 * search to find a hash (a pointer in this case).
 */

#define ADD_UUID_PAIR     1
#define ADD_CLIENT        2
#define GET_PEER          3
#define REMOVE_CLIENT     4
#define GET_CLIENT_UUID   5
#define FIND_UUID         6
#define DUMP            666


struct crosslink
{
    ws_cli_conn_t    *clnt;
    const char       *uuid_own;
    const char       *uuid_peer;
    ws_cli_conn_t    *peer;
};

static void loc_lock()
{}

static void loc_unlock()
{}

static int compare(const void * s1, const void * s2)
{
    const struct crosslink* v1=(const struct crosslink*)s1;
    const struct crosslink* v2=(const struct crosslink*)s2;
    
    if(v1->clnt < v2->clnt)
        return -1;
    if(v1->clnt > v2->clnt)
        return 1;
    return 0;
}

int binary_search(const struct crosslink* A, size_t n, const ws_cli_conn_t* T)
{
    int L = 0;
    int R = n - 1;
    int m;
    
    while (L <= R)
    {
        m = (L + R) >> 1;
        if( A[m].clnt < T )
            L = m + 1;
        else if (A[m].clnt > T )
            R = m - 1;
        else
            return m;
    }
    return -1;
}

static int hashtable(struct crosslink* clnk, int todo)
{
    static struct crosslink ht[MAX_CLIENTS];
    int ret = -1;
    int tmp = 0;

    loc_lock();

    switch(todo)
    {
    case ADD_UUID_PAIR:

        tmp = 0;

        for(int i=0;i<MAX_CLIENTS;i++)
        {
            if(0==ht[i].uuid_own)
            {
                if(0==tmp)
                {
                    ht[i].uuid_own = clnk->uuid_own;
                    ht[i].uuid_peer = clnk->uuid_peer;
                    tmp++;
                }
                else
                {
                    ht[i].uuid_own = clnk->uuid_peer;
                    ht[i].uuid_peer = clnk->uuid_own;
                    ret = i;
                    break;
                }
            }
        }

        break;

    case ADD_CLIENT:

        tmp = 0;
        int i = 0;

        for(;i<MAX_CLIENTS;i++)
        {
            if(0!=ht[i].uuid_own && 0==strcmp(clnk->uuid_own,ht[i].uuid_own))
            {
				if(ht[i].clnt)
				  break; // There is already a client connected with this uiid.
				  
                ht[i].clnt = clnk->clnt;
                tmp++;
            }
            else
                if(0!=ht[i].uuid_peer && 0==strcmp(clnk->uuid_own ,ht[i].uuid_peer))
                {
					if(ht[i].peer)
					    break; // There is already a client connected with this uiid.
					    
                    ht[i].peer = clnk->clnt;
                    tmp++;
                }
                
            if(2==tmp)
            {   /* Since the binary search is used for looking-up, the array must be sorted */
                qsort(ht,MAX_CLIENTS,sizeof(struct crosslink),compare);
                ret = i;
                break;
            }
        }
            
        break;

    case GET_PEER:

        tmp = binary_search(ht,MAX_CLIENTS,clnk->clnt);

        if(0<=tmp)
            clnk->peer = ht[tmp].peer;

        break;
        
    case GET_CLIENT_UUID:

        tmp = binary_search(ht,MAX_CLIENTS,clnk->clnt);

        if(0<=tmp)
            clnk->uuid_own = ht[tmp].uuid_own;

        break;
        
    case FIND_UUID:

        for(int i=0;i<MAX_CLIENTS;i++)
        {
            if(strstr(ht[i].uuid_own,clnk->uuid_own))
            {
                clnk->clnt = ht[i].clnt;
                break;
            }
        }

        break;
        
   case REMOVE_CLIENT:
   
		tmp = 0;
		
		for(int i=0;i<MAX_CLIENTS;i++)
        {
            if(clnk->clnt == ht[i].clnt)
            {
                ht[i].clnt=0;
                tmp++;
            }
            
            if(clnk->clnt == ht[i].peer)
            {
                ht[i].peer=0;
                tmp++;
            }
            
            if(2==tmp)
              break;            
        }
		
        break;

    case DUMP:
        printf("\n");
        for(int i=0;i<MAX_CLIENTS;i++)
        {
            printf("%02i: cl=%p uuid=%s uuid_p=%s peer=%p\n",i,(void*)ht[i].clnt,ht[i].uuid_own,ht[i].uuid_peer,(void*)ht[i].peer);
        }
        printf("\n");
        break;
    }

    loc_unlock();
    return ret;
}

int add_pair(const char* provider, const char* user)
{
    struct crosslink clnk;
    
    clnk.uuid_own = provider;
    clnk.uuid_peer = user;

    return hashtable(&clnk,ADD_UUID_PAIR);
}

int add_client(ws_cli_conn_t* cl, const unsigned char * uuid)
{
    struct crosslink clnk;
    
    clnk.uuid_own = (const char *)uuid;
    clnk.clnt = cl;
    
    if(uuid)
        return hashtable(&clnk,ADD_CLIENT);
        
    return -1;
}

ws_cli_conn_t*  get_peer(ws_cli_conn_t* cl)
{
    struct crosslink clnk={0,0,0,0};
    
    clnk.clnt = cl;
    hashtable(&clnk, GET_PEER);
    return clnk.peer;
}

int  get_client_auth_status(ws_cli_conn_t* cl)
{
    struct crosslink clnk={0,0,0,0};
    
    clnk.clnt = cl;
    hashtable(&clnk, GET_CLIENT_UUID);
    return 0!=clnk.uuid_own;
}

int  known_uuid(char* id)
{
    struct crosslink clnk={0,0,0,0};
    
    clnk.uuid_own = id;
    hashtable(&clnk, FIND_UUID);
    return 0!=clnk.clnt;
}

void remove_client(ws_cli_conn_t* cl)
{
    struct crosslink clnk={cl,0,0,0};
    
    hashtable(&clnk, FIND_UUID);
}


int lut_dump()
{
    struct crosslink clnk;
    return hashtable(&clnk,DUMP);
}
