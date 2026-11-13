#ifndef PRIORITY95_POLICY
#define PRIORITY95_POLICY
#include <stdint.h>
#include <stddef.h>
#include <arpa/inet.h>
#include <string.h>
struct request95 {int command,pid,uid,score,pairs;};
static int decode95(const void *bytes,size_t length,struct request95 *r){
 uint32_t a[13];if(length<4||length>sizeof(a)||length%4)return -1;memcpy(a,bytes,length);int n=(int)length/4;for(int i=0;i<n;i++)a[i]=ntohl(a[i]);memset(r,0,sizeof(*r));r->command=(int32_t)a[0];
 if(r->command==0){if(n<3||n%2!=1)return -1;r->pairs=(n-1)/2;int last_min=-1,last_score=-1001;for(int i=1;i<n;i+=2){int min=(int32_t)a[i],score=(int32_t)a[i+1];if(min<last_min||min<0||score<last_score||score< -1000||score>1000)return -1;last_min=min;last_score=score;}return 0;}
 if(r->command==1&&n==4){r->pid=(int32_t)a[1];r->uid=(int32_t)a[2];r->score=(int32_t)a[3];return r->pid>1&&(r->uid==1000||(r->uid>=10000&&r->uid<20000))&&r->score>=-1000&&r->score<=1000?0:-1;}
 if(r->command==2&&n==2){r->pid=(int32_t)a[1];return r->pid>1?0:-1;}
 return -1;
}
#endif
