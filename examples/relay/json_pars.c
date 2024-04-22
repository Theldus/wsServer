#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ws.h>

#define JSMN_STATIC 
#include "jsmn.h"
#include "json_pars.h"


static int check_symb(char s) 
{	/*
    65-70 A-F
    97-102 a-f
    48-57 0-9
    */
    if ( (s>=65 && s<=70) || (s>=97 && s<=102) || (s>=48 && s<=57) )
        return 1;

    return 0;
}

static int is_uuid(const char* uu)
{
    int len=0;
    while(0!=uu[len])
    {
        if( (len>=0 && len<8) || (len>=9 && len<13) || (len>=14 && len<18) || (len>=19 && len<23) || (len>=24 && len<36))
        {
            if(0==check_symb(uu[len]))
            {
                return 0;
            }
        }
        else
            if('-'!=uu[len])
                return 0;

        if(len > 35)
            return 0;

        len++;
    }

    if(len < 36)
        return 0;

    return 1;
}


char* alloc_peer_buff(const char* peer_file)
{
    FILE*  fp  = fopen(peer_file,"rb");
    size_t tmp = 0;
    char*  txt = 0;

    if(fp)
    {
        fseek(fp,0,SEEK_END);
        tmp = ftell(fp);
        fseek(fp,0,SEEK_SET);

        txt = malloc(tmp+1);
        txt[tmp]=0;
        tmp = fread(txt,1,tmp,fp);
        fclose(fp);
    }

    return txt;
}


int  get_pairs(char* peer_bfr, pairs fncptr)
{			 
    const char* provider=0;
    const char* user=0;

    char tmp=0;

    jsmn_parser parser;
    jsmntok_t tokens[MAX_CLIENTS*3]={0};
    jsmn_init(&parser);
    int ret = jsmn_parse(&parser,peer_bfr,strlen(peer_bfr),tokens,MAX_CLIENTS*3);

    if(0>ret)
        return ret;

    for(int i=0;i<MAX_CLIENTS*3;i++)
    {
        tmp = peer_bfr[tokens[i].start];
        
        if('p' == tmp || 'u' == tmp)
        {
            
            if(tokens[i+1].end)
                peer_bfr[tokens[i+1].end]=0;

            if(tokens[i+1].start && is_uuid(peer_bfr + tokens[i+1].start))
            {
                switch(tmp)
                {
                case 'p':
                    provider = peer_bfr + tokens[i+1].start;
                    break;
                case 'u':
                    if(provider)
                        user = peer_bfr + tokens[i+1].start;
                    break;
                default:break;
                }
            }

            if(provider && user)
            {
                fncptr(provider,user);
                provider=0;
                user=0;
            }
        }
    }

    return 0;
}

