/* PBKDF2 host boundary. 固件 PIN 格式保持不变。/ Keep the production PIN record format. */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "sha256.h"
static void hmac(const uint8_t *key,size_t n,const uint8_t *message,size_t len,uint8_t out[32]){
    uint8_t k[64]={0},pad[64],digest[32];SHA256_CTX ctx;
    if(n>64){sha256_init(&ctx);sha256_update(&ctx,key,n);sha256_final(&ctx,k);}else memcpy(k,key,n);
    for(unsigned i=0;i<64;i++)pad[i]=k[i]^0x36;
    sha256_init(&ctx);sha256_update(&ctx,pad,64);sha256_update(&ctx,message,len);sha256_final(&ctx,digest);
    for(unsigned i=0;i<64;i++)pad[i]=k[i]^0x5c;
    sha256_init(&ctx);sha256_update(&ctx,pad,64);sha256_update(&ctx,digest,32);sha256_final(&ctx,out);
    memset(k,0,sizeof(k));memset(pad,0,sizeof(pad));memset(digest,0,sizeof(digest));
}
int sim_pbkdf2(const unsigned char *password,size_t pn,const unsigned char *salt,size_t sn,unsigned rounds,unsigned char *out,size_t size){
    if(!rounds||sn>64||size>32)return -1;
    uint8_t first[68],u[32],result[32];memcpy(first,salt,sn);first[sn]=first[sn+1]=first[sn+2]=0;first[sn+3]=1;
    hmac(password,pn,first,sn+4,u);memcpy(result,u,32);
    for(unsigned i=1;i<rounds;i++){hmac(password,pn,u,32,u);for(unsigned j=0;j<32;j++)result[j]^=u[j];}
    memcpy(out,result,size);memset(u,0,sizeof(u));memset(result,0,sizeof(result));return 0;
}
